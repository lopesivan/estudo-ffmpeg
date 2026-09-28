#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define DEVICE_PATH "/dev/video0"
#define OUTPUT_FILENAME "output.mp4"
#define FRAME_RATE 30
#define DURATION 10

int main(void) {
    // Open device
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Could not open device %s: %s\n", DEVICE_PATH, strerror(errno));
        return 1;
    }

    // Query device capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        fprintf(stderr, "Could not query device capabilities: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        close(fd);
        return 1;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        close(fd);
        return 1;
    }

    // Set device format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        fprintf(stderr, "Could not set device format: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Set device framerate
    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &parm) == -1) {
        fprintf(stderr, "Could not get device framerate: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = FRAME_RATE;
    if (ioctl(fd, VIDIOC_S_PARM, &parm) == -1) {
        fprintf(stderr, "Could not set device framerate: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Initialize libavformat
    av_register_all();

    // Create output context
    AVOutputFormat *fmt_out = av_guess_format("mp4", NULL, NULL);
    if (!fmt_out) {
        fprintf(stderr, "Could not guess output format\n");
        close(fd);
        return 1;
    }
    AVFormatContext *ctx_out = avformat_alloc_context();
    if (!ctx_out) {
        fprintf(stderr, "Could not allocate output context\n");
        close(fd);
        return 1;
    }
    ctx_out->oformat = fmt_out;
    snprintf(ctx_out->filename, sizeof(ctx_out->filename), "%s", OUTPUT_FILENAME);

    // Create video stream
    AVStream *stream = avformat_new_stream(ctx_out, NULL);
    if (!stream) {
        fprintf(stderr, "Could not create video stream\n");
        close(fd);
        return 1;
    }

stream->id = 0;
AVCodecParameters *codecpar = stream->codecpar;
codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
codecpar->codec_id = AV_CODEC_ID_H264;
codecpar->width = fmt.fmt.pix.width;
codecpar->height = fmt.fmt.pix.height;
codecpar->format = AV_PIX_FMT_YUV420P;
codecpar->bit_rate = 1000000;
codecpar->profile = FF_PROFILE_H264_HIGH;
codecpar->level = 40;

// Find encoder
AVCodec *codec = avcodec_find_encoder(codecpar->codec_id);
if (!codec) {
    fprintf(stderr, "Could not find encoder\n");
    close(fd);
    return 1;
}

// Create codec context
AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
if (!codec_ctx) {
    fprintf(stderr, "Could not allocate codec context\n");
    close(fd);
    return 1;
}
codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
codec_ctx->codec_id = codecpar->codec_id;
codec_ctx->width = codecpar->width;
codec_ctx->height = codecpar->height;
codec_ctx->pix_fmt = codecpar->format;
codec_ctx->time_base = (AVRational){1, FRAME_RATE};
codec_ctx->bit_rate = codecpar->bit_rate;
codec_ctx->profile = codecpar->profile;
codec_ctx->level = codecpar->level;

// Open encoder
if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "Could not open encoder\n");
    close(fd);
    return 1;
}

// Initialize frame
AVFrame *frame = av_frame_alloc();
if (!frame) {
    fprintf(stderr, "Could not allocate frame\n");
    close(fd);
    return 1;
}
frame->width = codec_ctx->width;
frame->height = codec_ctx->height;
frame->format = codec_ctx->pix_fmt;
if (av_frame_get_buffer(frame, 0) < 0) {
    fprintf(stderr, "Could not allocate frame buffer\n");
    close(fd);
    return 1;
}

// Initialize packet
AVPacket pkt = {0};

// Open output file
if (avio_open(&ctx_out->pb, ctx_out->filename, AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr, "Could not open output file\n");
    close(fd);
    return 1;
}

// Write header
if (avformat_write_header(ctx_out, NULL) < 0) {
    fprintf(stderr, "Could not write header\n");
    close(fd);
    return 1;
}

// Initialize timer
int64_t start_time = av_gettime();
int64_t end_time;
int64_t duration;

// Read frames and write to output file
for (;;) {
    // Read frame
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        fprintf(stderr, "Could not dequeue buffer: %s\n", strerror(errno));
        break;
    }

    // Convert to YUV420P
    uint8_t *src_data[4] = {0};
    src_data[0] = (uint8_t *)mmap(NULL,

    if (ret < 0) {
        fprintf(stderr, "Could not encode frame\n");
        break;
    }

    // Write packet to output file
    if (pkt.size > 0) {
        av_packet_rescale_ts(&pkt, codec_ctx->time_base, stream->time_base);
        av_interleaved_write_frame(ctx_out, &pkt);
        av_packet_unref(&pkt);
    }

    // Calculate duration
    end_time = av_gettime();
    duration = (end_time - start_time) / 1000;
    if (duration >= DURATION * 1000) {
        break;
    }
}

// Write trailer
av_write_trailer(ctx_out);

// Clean up
avcodec_free_context(&codec_ctx);
av_frame_free(&frame);
avformat_close_input(&ctx_out);
close(fd);

return 0;

}


Este código usa a biblioteca libavformat e libavcodec da ffmpeg para capturar o vídeo da webcam (/dev/video0) e gravar 10 segundos de vídeo sem áudio no formato mp4 h264 em um arquivo de saída. O vídeo é codificado em H.264 usando o codec libx264.
