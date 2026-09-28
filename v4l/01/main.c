#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define WIDTH 640
#define HEIGHT 480
#define FPS 30

int main()
{
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    void* buffer;
    int n_buffers;
    struct timeval start_time, end_time;
    long int elapsed_time;

    fd = open ("/dev/video0", O_RDWR); // abre o dispositivo de vídeo
    if (fd == -1)
    {
        perror ("Erro ao abrir o dispositivo de vídeo");
        return -1;
    }

    if (ioctl (fd, VIDIOC_QUERYCAP, &cap) == -1) // verifica as capacidades do dispositivo de vídeo
    {
        perror ("Erro ao verificar as capacidades do dispositivo de vídeo");
        return -1;
    }

    memset (&fmt, 0, sizeof (fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl (fd, VIDIOC_S_FMT, &fmt) == -1) // define o formato de vídeo
    {
        perror ("Erro ao definir o formato de vídeo");
        return -1;
    }

    memset (&req, 0, sizeof (req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl (fd, VIDIOC_REQBUFS, &req) == -1) // solicita a alocação de buffers de memória
    {
        perror ("Erro ao solicitar a alocação de buffers de memória");
        return -1;
    }

    buffer = malloc (req.count * sizeof (*buffer));
    if (!buffer)
    {
        perror ("Erro ao alocar memória para os buffers");
        return -1;
    }

    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl (fd, VIDIOC_QUERYBUF, &buf) == -1) // consulta informações sobre os buffers
        {
            perror ("Erro ao consultar informações sobre os buffers");
            return -1;
        }

        ((char*)buffer)[buf.m.offset] = 0;

        if (mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset) == MAP_FAILED) // m

        {
            perror ("Erro ao mapear os buffers para a memória");
            return -1;
        }
    }

    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl (fd, VIDIOC_QBUF, &buf) == -1) // enfileira buffers para captura de vídeo
        {
            perror ("Erro ao enfileirar os buffers para a captura de vídeo");
            return -1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (fd, VIDIOC_STREAMON, &type) == -1) // inicia a transmissão de vídeo
    {
        perror ("Erro ao iniciar a transmissão de vídeo");
        return -1;
    }

    gettimeofday (&start_time, NULL); // marca o tempo inicial

    while (1)
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl (fd, VIDIOC_DQBUF, &buf) == -1) // retira buffers da fila
        {
            perror ("Erro ao retirar os buffers da fila");
            return -1;
        }

        if (buf.bytesused > 0) // verifica se o buffer contém dados
        {
            // faça algo com os dados do buffer
        }

        if (ioctl (fd, VIDIOC_QBUF, &buf) == -1) // enfileira buffers para captura de vídeo
        {
            perror ("Erro ao enfileirar os buffers para a captura de vídeo");
            return -1;
        }

        gettimeofday (&end_time, NULL); // marca o tempo final
        elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_usec - start_time.tv_usec); // calcula o tempo decorrido em microssegundos
        if (elapsed_time >= 10000000) // verifica se o tempo decorrido é maior ou igual a 10 segundos
            break;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (fd, VIDIOC_STREAMOFF, &type) == -1) // interrompe a transmissão de vídeo
    {
        perror ("Erro ao interromper a transmissão de vídeo");
        return -1;
    }

    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
        munmap (buffer, req.count * sizeof (*buffer)); // desfaz o mapeamento dos buffers para a memória

    free (buffer); // libera a memória alocada para os buffers

    close (fd); // fecha o dispositivo de vídeo

    return 0;
}

