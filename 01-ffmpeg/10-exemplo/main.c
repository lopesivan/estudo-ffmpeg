#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

#define DEVICE_PATH "/dev/video0"
#define OUTPUT_FILENAME "output.mp4"
#define FRAME_RATE 30
#define DURATION 10
#define CAPTURE_W 640
#define CAPTURE_H 480
#define NUM_BUFFERS 4

typedef struct {
  void *start;
  size_t length;
} MappedBuffer;

int main(void) {
  int ret = 0;
  int fd = -1;

  MappedBuffer buffers[NUM_BUFFERS];
  memset(buffers, 0, sizeof(buffers));

  AVFormatContext *ctx_out = NULL;
  AVStream *stream = NULL;
  const AVCodec *codec = NULL;
  AVCodecContext *codec_ctx = NULL;
  struct SwsContext *sws_ctx = NULL;
  AVFrame *frame = NULL; // destino YUV420P
  AVPacket *pkt = NULL;

  // =================================================================
  // 1. Abre o dispositivo V4L2
  // =================================================================
  fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    fprintf(stderr, "Could not open device %s: %s\n", DEVICE_PATH,
            strerror(errno));
    return 1;
  }

  struct v4l2_capability cap;
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
    fprintf(stderr, "VIDIOC_QUERYCAP: %s\n", strerror(errno));
    goto cleanup;
  }

  __u32 caps = cap.capabilities;
  if (caps & V4L2_CAP_DEVICE_CAPS)
    caps = cap.device_caps;

  if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "Device does not support video capture\n");
    goto cleanup;
  }
  if (!(caps & V4L2_CAP_STREAMING)) {
    fprintf(stderr, "Device does not support streaming\n");
    goto cleanup;
  }

  // =================================================================
  // 2. Formato YUYV 640x480
  // =================================================================
  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = CAPTURE_W;
  fmt.fmt.pix.height = CAPTURE_H;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
    fprintf(stderr, "VIDIOC_S_FMT: %s\n", strerror(errno));
    goto cleanup;
  }

  fprintf(stderr, "Formato da câmera: %ux%u, fourcc=0x%08X\n",
          fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);

  // O driver pode negociar um formato diferente do pedido (permitido pela
  // spec do V4L2). Se isso acontecer, o sws_getContext abaixo, que assume
  // AV_PIX_FMT_YUYV422 como formato de origem, receberia dados no formato
  // errado e geraria imagem corrompida sem erro nenhum.
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
    fprintf(stderr,
            "Driver negociou formato diferente do pedido (esperado YUYV, "
            "recebido fourcc=0x%08X). Ajuste o sws_getContext ou force "
            "outro formato de captura.\n",
            fmt.fmt.pix.pixelformat);
    goto cleanup;
  }

  // =================================================================
  // 3. Framerate
  // =================================================================
  struct v4l2_streamparm parm;
  memset(&parm, 0, sizeof(parm));
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = FRAME_RATE;
    ioctl(fd, VIDIOC_S_PARM, &parm);
  }

  // =================================================================
  // 4. Pipeline V4L2: REQBUFS -> QUERYBUF -> mmap -> QBUF -> STREAMON
  // =================================================================
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = NUM_BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
    fprintf(stderr, "VIDIOC_REQBUFS: %s\n", strerror(errno));
    goto cleanup;
  }
  if (req.count < 2) {
    fprintf(stderr, "Buffers insuficientes (%u)\n", req.count);
    goto cleanup;
  }
  // A spec do V4L2 não garante que o driver devolva req.count <= o valor
  // pedido; sem este limite, o laço abaixo poderia escrever fora dos
  // limites do array buffers[NUM_BUFFERS].
  if (req.count > NUM_BUFFERS) {
    fprintf(stderr,
            "Driver alocou mais buffers (%u) do que o suportado (%d)\n",
            req.count, NUM_BUFFERS);
    goto cleanup;
  }

  for (unsigned i = 0; i < req.count; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
      fprintf(stderr, "VIDIOC_QUERYBUF[%u]: %s\n", i, strerror(errno));
      goto cleanup;
    }

    buffers[i].length = buf.length;
    buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
    if (buffers[i].start == MAP_FAILED) {
      fprintf(stderr, "mmap[%u]: %s\n", i, strerror(errno));
      buffers[i].start = NULL;
      goto cleanup;
    }
  }

  for (unsigned i = 0; i < req.count; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      fprintf(stderr, "VIDIOC_QBUF[%u]: %s\n", i, strerror(errno));
      goto cleanup;
    }
  }

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
    fprintf(stderr, "VIDIOC_STREAMON: %s\n", strerror(errno));
    goto cleanup;
  }

  // =================================================================
  // 5. Encoder H.264
  // =================================================================
  codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    fprintf(stderr, "Could not find encoder H.264\n");
    goto cleanup;
  }

  codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    fprintf(stderr, "Could not allocate codec context\n");
    goto cleanup;
  }

  codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_ctx->codec_id = AV_CODEC_ID_H264;
  codec_ctx->width = fmt.fmt.pix.width;
  codec_ctx->height = fmt.fmt.pix.height;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->time_base = (AVRational){1, FRAME_RATE};
  codec_ctx->framerate = (AVRational){FRAME_RATE, 1};
  codec_ctx->bit_rate = 1000000;
  codec_ctx->gop_size = 12;
  codec_ctx->max_b_frames = 0;
  codec_ctx->profile = AV_PROFILE_H264_HIGH;
  codec_ctx->level = 40;

  if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "Could not open encoder\n");
    goto cleanup;
  }

  // =================================================================
  // 6. Contexto de saída MP4
  // =================================================================
  if (avformat_alloc_output_context2(&ctx_out, NULL, "mp4", OUTPUT_FILENAME) <
          0 ||
      !ctx_out) {
    fprintf(stderr, "Could not allocate output context\n");
    goto cleanup;
  }

  stream = avformat_new_stream(ctx_out, NULL);
  if (!stream) {
    fprintf(stderr, "Could not create video stream\n");
    goto cleanup;
  }

  if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
    fprintf(stderr, "Could not copy codec parameters\n");
    goto cleanup;
  }
  stream->time_base = codec_ctx->time_base;

  if (!(ctx_out->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&ctx_out->pb, OUTPUT_FILENAME, AVIO_FLAG_WRITE) < 0) {
      fprintf(stderr, "Could not open output file\n");
      goto cleanup;
    }
  }

  if (avformat_write_header(ctx_out, NULL) < 0) {
    fprintf(stderr, "Could not write header\n");
    goto cleanup;
  }

  // =================================================================
  // 7. Frame YUV420P (destino) + sws_ctx (uma vez só)
  // =================================================================
  frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Could not allocate frame\n");
    goto cleanup;
  }
  frame->format = codec_ctx->pix_fmt;
  frame->width = codec_ctx->width;
  frame->height = codec_ctx->height;
  if (av_frame_get_buffer(frame, 32) < 0) {
    fprintf(stderr, "Could not allocate frame buffer\n");
    goto cleanup;
  }

  // YUYV (640x480) -> YUV420P (640x480)
  sws_ctx =
      sws_getContext(fmt.fmt.pix.width, fmt.fmt.pix.height, AV_PIX_FMT_YUYV422,
                     codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                     SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    fprintf(stderr, "Could not create sws context\n");
    goto cleanup;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Could not allocate packet\n");
    goto cleanup;
  }

  // =================================================================
  // 8. Loop principal
  // =================================================================
  int64_t start_time = av_gettime_relative();
  int frame_count = 0;

  uint8_t *src_slice[4] = {NULL, NULL, NULL, NULL};
  int src_stride[4] = {0, 0, 0, 0};

  while (1) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
      if (errno == EAGAIN) {
        usleep(1000);
        continue;
      }
      fprintf(stderr, "VIDIOC_DQBUF: %s\n", strerror(errno));
      break;
    }

    // -------- YUYV -> YUV420P --------
    // Fonte: buffer mmapado pelo driver (packed YUYV: só plano [0])
    src_slice[0] = (uint8_t *)buffers[buf.index].start;
    src_stride[0] = fmt.fmt.pix.width * 2; // YUYV = 2 bytes/pixel

    if (av_frame_make_writable(frame) < 0) {
      ioctl(fd, VIDIOC_QBUF, &buf);
      break;
    }

    sws_scale(sws_ctx, (const uint8_t *const *)src_slice, src_stride, 0,
              fmt.fmt.pix.height, frame->data, frame->linesize);

    frame->pts = av_rescale_q(frame_count, (AVRational){1, FRAME_RATE},
                              codec_ctx->time_base);

    // -------- Encode --------
    if (avcodec_send_frame(codec_ctx, frame) < 0) {
      fprintf(stderr, "Could not send frame\n");
      ioctl(fd, VIDIOC_QBUF, &buf);
      break;
    }

    while (1) {
      ret = avcodec_receive_packet(codec_ctx, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      if (ret < 0) {
        fprintf(stderr, "Could not encode frame\n");
        ioctl(fd, VIDIOC_QBUF, &buf);
        goto cleanup;
      }

      pkt->stream_index = stream->index;
      av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);

      if (av_interleaved_write_frame(ctx_out, pkt) < 0) {
        fprintf(stderr, "Could not write packet\n");
        av_packet_unref(pkt);
        ioctl(fd, VIDIOC_QBUF, &buf);
        goto cleanup;
      }
      av_packet_unref(pkt);
    }

    frame_count++;

    // Devolve o buffer para o driver
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
      fprintf(stderr, "VIDIOC_QBUF: %s\n", strerror(errno));
      break;
    }

    if ((av_gettime_relative() - start_time) / 1000000 >= DURATION)
      break;
  }

  // =================================================================
  // 9. Flush do encoder
  // =================================================================
  avcodec_send_frame(codec_ctx, NULL);
  while (1) {
    ret = avcodec_receive_packet(codec_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    if (ret < 0)
      break;

    pkt->stream_index = stream->index;
    av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
    av_interleaved_write_frame(ctx_out, pkt);
    av_packet_unref(pkt);
  }

  av_write_trailer(ctx_out);
  fprintf(stderr, "OK: %d frames gravados em %s\n", frame_count,
          OUTPUT_FILENAME);
  ret = 0;

cleanup:
  if (fd >= 0) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &t);

    for (unsigned i = 0; i < NUM_BUFFERS; i++) {
      if (buffers[i].start && buffers[i].start != MAP_FAILED)
        munmap(buffers[i].start, buffers[i].length);
    }
    close(fd);
  }

  if (sws_ctx)
    sws_freeContext(sws_ctx);
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&codec_ctx);

  if (ctx_out) {
    if (ctx_out->pb)
      avio_closep(&ctx_out->pb);
    avformat_free_context(ctx_out);
  }

  return ret;
}
