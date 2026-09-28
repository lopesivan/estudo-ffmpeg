#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define WIDTH 640
#define HEIGHT 480
#define VIDEO_DEVICE "/dev/video0"
#define OUTPUT_FILE "output_video.raw"
#define CAPTURE_TIME 10 // seconds
#define FRAME_RATE 30

int main()
{
    int fd, outputFile;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;

    fd = open (VIDEO_DEVICE, O_RDWR);
    if (fd < 0)
    {
        perror ("Cannot open device");
        return 1;
    }

    if (ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        perror ("Cannot query capabilities");
        close (fd);
        return 1;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl (fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        perror ("Cannot set format");
        close (fd);
        return 1;
    }

    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl (fd, VIDIOC_REQBUFS, &req) < 0)
    {
        perror ("Cannot request buffers");
        close (fd);
        return 1;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (ioctl (fd, VIDIOC_QUERYBUF, &buf) < 0)
    {
        perror ("Cannot query buffer");
        close (fd);
        return 1;
    }

    void* buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED)
    {
        perror ("Cannot mmap buffer");
        close (fd);
        return 1;
    }

    outputFile = open (OUTPUT_FILE, O_WRONLY | O_CREAT, 0644);
    if (outputFile < 0)
    {
        perror ("Cannot open output file");
        munmap (buffer, buf.length);
        close (fd);
        return 1;
    }

    if (ioctl (fd, VIDIOC_QBUF, &buf) < 0)
    {
        perror ("Cannot enqueue buffer");
        close (outputFile);
        munmap (buffer, buf.length);
        close (fd);
        return 1;
    }

    if (ioctl (fd, VIDIOC_STREAMON, &buf.type) < 0)
    {
        perror ("Cannot start streaming");
        close (outputFile);
        munmap (buffer, buf.length);
        close (fd);

        return 1;
    }

    for (int i = 0; i < CAPTURE_TIME * FRAME_RATE; i++)
    {
        if (ioctl (fd, VIDIOC_DQBUF, &buf) < 0)
        {
            perror ("Cannot dequeue buffer");
            break;
        }

        write (outputFile, buffer, buf.bytesused);

        if (ioctl (fd, VIDIOC_QBUF, &buf) < 0)
        {
            perror ("Cannot enqueue buffer");
            break;
        }
    }

    if (ioctl (fd, VIDIOC_STREAMOFF, &buf.type) < 0)
    {
        perror ("Cannot stop streaming");
    }

    close (outputFile);
    munmap (buffer, buf.length);
    close (fd);

    printf ("Video capture completed. Saved to '%s'.\n", OUTPUT_FILE);
    printf ("Please note that the output file is in raw YUYV format. You may need to convert it to a more common format, such as MP4, using a tool like FFmpeg.\n");

    return 0;
}
