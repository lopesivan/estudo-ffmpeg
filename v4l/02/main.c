#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define BUFFER_COUNT 4

struct buffer
{
    void* start;
    size_t length;
};

int main()
{
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    struct buffer buffers[BUFFER_COUNT];
    int i, n;

    // abre o dispositivo de vídeo
    fd = open ("/dev/video0", O_RDWR);
    if (fd == -1)
    {
        perror ("Erro ao abrir o dispositivo de vídeo");
        return 1;
    }

    // verifica se o dispositivo de vídeo suporta a captura de vídeo
    if (ioctl (fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        perror ("Erro ao verificar as capacidades do dispositivo de vídeo");
        return 1;
    }
    if (! (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        perror ("O dispositivo de vídeo não suporta a captura de vídeo");
        return 1;
    }

    // define o formato de vídeo
    memset (&fmt, 0, sizeof (fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if (ioctl (fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        perror ("Erro ao definir o formato de vídeo");
        return 1;
    }

    // solicita a alocação de buffers de vídeo
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = BUFFER_COUNT;
    if (ioctl (fd, VIDIOC_REQBUFS, &reqbuf) == -1)
    {
        perror ("Erro ao solicitar a alocação de buffers de vídeo");
        return 1;
    }

    // mapeia os buffers de vídeo para a memória do processo
    for (i = 0; i < BUFFER_COUNT; i++)
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl (fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            perror ("Erro ao consultar os buffers de vídeo");
            return 1;
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED)
        {
            perror ("Erro ao mapear o buffer de vídeo para a memória do processo");
            return 1;
        }
    }

    // enfileira os buffers de vídeo para captura
    for (i = 0; i < BUFFER_COUNT; i++)
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl (fd, VIDIOC_QBUF, &buf) == -1)
        {
            perror ("Erro ao enfileirar o buffer de vídeo para a captura");
            return 1;
        }
    }

    // inicia a captura de vídeo
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror ("Erro ao iniciar a captura de vídeo");
        return 1;
    }

    // captura os frames de vídeo
    for (n = 0; n < 10; n++)   // captura 10 frames de vídeo
    {
        memset (&buf, 0, sizeof (buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl (fd, VIDIOC_DQBUF, &buf) == -1)
        {
            perror ("Erro ao desenfileirar o buffer de vídeo");
            return 1;
        }
        printf ("Capturado frame de vídeo %d com tamanho %d bytes\n", n, buf.bytesused);

        // processa o frame de vídeo aqui, se necessário

        if (ioctl (fd, VIDIOC_QBUF, &buf) == -1)
        {
            perror ("Erro ao enfileirar o buffer de vídeo para a captura");
            return 1;
        }
    }

    // para a captura de vídeo
    if (ioctl (fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror ("Erro ao parar a captura de vídeo");
        return 1;
    }

    // libera os buffers de vídeo
    for (i = 0; i < BUFFER_COUNT; i++)
    {
        munmap (buffers[i].start, buffers[i].length);
    }

    // fecha o dispositivo de vídeo
    close (fd);

    return 0;

}
