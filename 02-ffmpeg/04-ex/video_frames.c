#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h> // Add this before videodev2.h
#include <unistd.h>

#define BUFFER_COUNT 4
#define NUM_FRAMES 30

struct buffer
{
    void* start;
    size_t length;
};

struct camera
{
    int fd;
    struct buffer* buffers;
    unsigned int n_buffers;
    int width;
    int height;
    int format;
};

// Converter YUYV para RGB
void yuyv_to_rgb(const unsigned char* yuyv, unsigned char* rgb, int width,
                 int height)
{
    int i, j;
    const unsigned char* src = yuyv;
    unsigned char* dst = rgb;

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j += 2)
        {
            int y1 = src[0];
            int u = src[1];
            int y2 = src[2];
            int v = src[3];

            // Converter YUV para RGB (primeiro pixel)
            int r1 = y1 + 1.4075 * (v - 128);
            int g1 = y1 - 0.3455 * (u - 128) - 0.7169 * (v - 128);
            int b1 = y1 + 1.7790 * (u - 128);

            // Converter YUV para RGB (segundo pixel)
            int r2 = y2 + 1.4075 * (v - 128);
            int g2 = y2 - 0.3455 * (u - 128) - 0.7169 * (v - 128);
            int b2 = y2 + 1.7790 * (u - 128);

            // Clamping
            dst[0] = (r1 < 0) ? 0 : (r1 > 255) ? 255 : r1;
            dst[1] = (g1 < 0) ? 0 : (g1 > 255) ? 255 : g1;
            dst[2] = (b1 < 0) ? 0 : (b1 > 255) ? 255 : b1;

            dst[3] = (r2 < 0) ? 0 : (r2 > 255) ? 255 : r2;
            dst[4] = (g2 < 0) ? 0 : (g2 > 255) ? 255 : g2;
            dst[5] = (b2 < 0) ? 0 : (b2 > 255) ? 255 : b2;

            src += 4;
            dst += 6;
        }
    }
}

// Salvar frame como PNG
int save_png(const char* filename, unsigned char* rgb_data, int width,
             int height)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Erro ao criar arquivo %s\n", filename);
        return -1;
    }

    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "Erro ao criar estrutura PNG\n");
        fclose(fp);
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "Erro ao criar info PNG\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        fprintf(stderr, "Erro durante escrita PNG\n");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return -1;
    }

    png_init_io(png_ptr, fp);

    // Configurar header PNG
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Escrever dados da imagem
    png_bytep* row_pointers = malloc(height * sizeof(png_bytep));
    for (int y = 0; y < height; y++)
    {
        row_pointers[y] = rgb_data + y * width * 3;
    }

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    // Limpeza
    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 0;
}

// Inicializar dispositivo V4L2
int init_device(struct camera* cam, const char* device_name)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;

    // Abrir dispositivo
    cam->fd = open(device_name, O_RDWR | O_NONBLOCK);
    if (cam->fd == -1)
    {
        fprintf(stderr, "Erro ao abrir %s: %s\n", device_name,
                strerror(errno));
        return -1;
    }

    // Verificar capacidades
    if (ioctl(cam->fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        fprintf(stderr, "Erro ao consultar capacidades: %s\n",
                strerror(errno));
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fprintf(stderr, "Dispositivo não suporta captura de vídeo\n");
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "Dispositivo não suporta streaming\n");
        return -1;
    }

    printf("Driver: %s\n", cap.driver);
    printf("Card: %s\n", cap.card);
    printf("Bus: %s\n", cap.bus_info);

    // Configurar formato
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        fprintf(stderr, "Erro ao configurar formato: %s\n", strerror(errno));
        return -1;
    }

    cam->width = fmt.fmt.pix.width;
    cam->height = fmt.fmt.pix.height;
    cam->format = fmt.fmt.pix.pixelformat;

    printf("Resolução configurada: %dx%d\n", cam->width, cam->height);

    // Solicitar buffers
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(cam->fd, VIDIOC_REQBUFS, &req) == -1)
    {
        fprintf(stderr, "Erro ao solicitar buffers: %s\n", strerror(errno));
        return -1;
    }

    if (req.count < 2)
    {
        fprintf(stderr, "Memória insuficiente\n");
        return -1;
    }

    cam->buffers = calloc(req.count, sizeof(struct buffer));
    if (!cam->buffers)
    {
        fprintf(stderr, "Erro ao alocar memória para buffers\n");
        return -1;
    }

    cam->n_buffers = req.count;

    // Mapear buffers
    for (unsigned int i = 0; i < cam->n_buffers; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            fprintf(stderr, "Erro ao consultar buffer %d: %s\n", i,
                    strerror(errno));
            return -1;
        }

        cam->buffers[i].length = buf.length;
        cam->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, cam->fd, buf.m.offset);

        if (cam->buffers[i].start == MAP_FAILED)
        {
            fprintf(stderr, "Erro ao mapear buffer %d: %s\n", i,
                    strerror(errno));
            return -1;
        }
    }

    printf("Inicialização concluída com sucesso!\n");
    return 0;
}

// Iniciar captura
int start_capture(struct camera* cam)
{
    // Enfileirar todos os buffers
    for (unsigned int i = 0; i < cam->n_buffers; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(cam->fd, VIDIOC_QBUF, &buf) == -1)
        {
            fprintf(stderr, "Erro ao enfileirar buffer %d: %s\n", i,
                    strerror(errno));
            return -1;
        }
    }

    // Iniciar streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam->fd, VIDIOC_STREAMON, &type) == -1)
    {
        fprintf(stderr, "Erro ao iniciar streaming: %s\n", strerror(errno));
        return -1;
    }

    printf("Captura iniciada!\n");
    return 0;
}

// Capturar um frame
int capture_frame(struct camera* cam, int frame_num)
{
    struct v4l2_buffer buf;
    fd_set fds;
    struct timeval tv;
    int r;

    // Aguardar dados disponíveis
    FD_ZERO(&fds);
    FD_SET(cam->fd, &fds);

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    r = select(cam->fd + 1, &fds, NULL, NULL, &tv);
    if (r == -1)
    {
        fprintf(stderr, "Erro em select(): %s\n", strerror(errno));
        return -1;
    }

    if (r == 0)
    {
        fprintf(stderr, "Timeout na captura\n");
        return -1;
    }

    // Desenfileirar buffer
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(cam->fd, VIDIOC_DQBUF, &buf) == -1)
    {
        fprintf(stderr, "Erro ao desenfileirar buffer: %s\n",
                strerror(errno));
        return -1;
    }

    // Processar frame
    unsigned char* rgb_data = malloc(cam->width * cam->height * 3);
    if (!rgb_data)
    {
        fprintf(stderr, "Erro ao alocar memória para RGB\n");
        return -1;
    }

    // Converter YUYV para RGB
    yuyv_to_rgb((unsigned char*)cam->buffers[buf.index].start, rgb_data,
                cam->width, cam->height);

    // Salvar como PNG
    char filename[64];
    snprintf(filename, sizeof(filename), "webcam_frame_%03d.png", frame_num);

    if (save_png(filename, rgb_data, cam->width, cam->height) == 0)
    {
        printf("Frame %d salvo: %s\n", frame_num, filename);
    }
    else
    {
        fprintf(stderr, "Erro ao salvar frame %d\n", frame_num);
    }

    free(rgb_data);

    // Reenfileirar buffer
    if (ioctl(cam->fd, VIDIOC_QBUF, &buf) == -1)
    {
        fprintf(stderr, "Erro ao reenfileirar buffer: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

// Parar captura
int stop_capture(struct camera* cam)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam->fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        fprintf(stderr, "Erro ao parar streaming: %s\n", strerror(errno));
        return -1;
    }

    printf("Captura parada!\n");
    return 0;
}

// Limpar recursos
void cleanup_camera(struct camera* cam)
{
    if (cam->buffers)
    {
        for (unsigned int i = 0; i < cam->n_buffers; i++)
        {
            if (cam->buffers[i].start != MAP_FAILED)
            {
                munmap(cam->buffers[i].start, cam->buffers[i].length);
            }
        }
        free(cam->buffers);
    }

    if (cam->fd != -1)
    {
        close(cam->fd);
    }
}

int main(int argc, char* argv[])
{
    const char* device_name = "/dev/video0";
    struct camera cam = {0};

    if (argc > 1)
    {
        device_name = argv[1];
    }

    printf("Capturando %d frames da webcam: %s\n", NUM_FRAMES, device_name);

    // Inicializar câmera
    if (init_device(&cam, device_name) != 0)
    {
        fprintf(stderr, "Erro na inicialização da câmera\n");
        return 1;
    }

    // Iniciar captura
    if (start_capture(&cam) != 0)
    {
        fprintf(stderr, "Erro ao iniciar captura\n");
        cleanup_camera(&cam);
        return 1;
    }

    // Capturar frames
    printf("Iniciando captura de %d frames...\n", NUM_FRAMES);
    for (int i = 1; i <= NUM_FRAMES; i++)
    {
        if (capture_frame(&cam, i) != 0)
        {
            fprintf(stderr, "Erro ao capturar frame %d\n", i);
            break;
        }

        // Pequena pausa entre frames
        usleep(100000); // 100ms
    }

    // Parar captura e limpar
    stop_capture(&cam);
    cleanup_camera(&cam);

    printf("\nCaptura concluída! %d frames salvos como PNG.\n", NUM_FRAMES);
    return 0;
}
