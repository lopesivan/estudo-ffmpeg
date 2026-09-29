#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <math.h>
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

// ============================================================
// CONFIGURAÇÃO DA CRUZ GIRATÓRIA
// ============================================================
#define LINE_THICKNESS 10 // espessura da linha em pixels
#define CROSS_COLOR_Y 76  // vermelho (BT.601)
#define CROSS_COLOR_U 84
#define CROSS_COLOR_V 255
#define CROSS_ROTATION_SPEED 2.0 // graus por frame
// ============================================================

typedef struct {
  void *start;
  size_t length;
} MappedBuffer;

// ============================================================
// Helpers de desenho
// ============================================================
static void paint_dot(uint8_t *plane, int linesize, int w, int h, int x, int y,
                      int r, uint8_t value) {
  for (int dy = -r; dy <= r; dy++) {
    int yy = y + dy;
    if (yy < 0 || yy >= h)
      continue;
    uint8_t *row = plane + yy * linesize;
    for (int dx = -r; dx <= r; dx++) {
      int xx = x + dx;
      if (xx < 0 || xx >= w)
        continue;
      row[xx] = value;
    }
  }
}

static void draw_line_yuv(AVFrame *frame, double x0, double y0, double x1,
                          double y1, int thickness) {
  const int W = frame->width;
  const int H = frame->height;
  const int cw = (W + 1) / 2;
  const int ch = (H + 1) / 2;

  const int r = thickness / 2;

  const double dx = x1 - x0;
  const double dy = y1 - y0;
  const double length = sqrt(dx * dx + dy * dy);
  if (length < 1.0)
    return;

  const double step = 0.5;
  const int steps = (int)(length / step);

  for (int i = 0; i <= steps; i++) {
    double t = (double)i / steps;
    int x = (int)(x0 + t * dx + 0.5);
    int y = (int)(y0 + t * dy + 0.5);

    paint_dot(frame->data[0], frame->linesize[0], W, H, x, y, r, CROSS_COLOR_Y);
    paint_dot(frame->data[1], frame->linesize[1], cw, ch, x / 2, y / 2,
              (r + 1) / 2, CROSS_COLOR_U);
    paint_dot(frame->data[2], frame->linesize[2], cw, ch, x / 2, y / 2,
              (r + 1) / 2, CROSS_COLOR_V);
  }
}

static void draw_rotating_cross(AVFrame *frame, double angle_degrees,
                                int thickness) {
  const int W = frame->width;
  const int H = frame->height;

  const double cx = W / 2.0;
  const double cy = H / 2.0;
  const double R = sqrt((double)(W * W + H * H)) / 2.0;

  const double rad = angle_degrees * M_PI / 180.0;

  draw_line_yuv(frame, cx + R * cos(rad), cy + R * sin(rad), cx - R * cos(rad),
                cy - R * sin(rad), thickness);

  const double rad90 = rad + M_PI / 2.0;
  draw_line_yuv(frame, cx + R * cos(rad90), cy + R * sin(rad90),
                cx - R * cos(rad90), cy - R * sin(rad90), thickness);
}

// ============================================================
// main
// ============================================================
int main(void) {
  int ret = 0;
  int fd = -1;

  MappedBuffer buffers[NUM_BUFFERS];
  memset(buffers, 0, sizeof(buffers));

  AVFormatContext *out_ctx = NULL;
  AVStream *out_stream = NULL;
  const AVCodec *encoder = NULL;
  AVCodecContext *enc_ctx = NULL;
  struct SwsContext *sws_ctx = NULL;
  AVFrame *yuv_frame = NULL;
  AVPacket *pkt = NULL;

  // =================================================================
  // 1. Abre o dispositivo V4L2
  // =================================================================
  fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "Erro ao abrir %s: %s\n", DEVICE_PATH, strerror(errno));
    return 1;
  }

  struct v4l2_capability cap;
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    fprintf(stderr, "VIDIOC_QUERYCAP falhou: %s\n", strerror(errno));
    goto cleanup;
  }

  __u32 caps = cap.capabilities;
  if (caps & V4L2_CAP_DEVICE_CAPS)
    caps = cap.device_caps;

  if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) {
    fprintf(stderr, "Dispositivo não suporta captura com streaming.\n");
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

  if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
    fprintf(stderr, "VIDIOC_S_FMT falhou: %s\n", strerror(errno));
    goto cleanup;
  }

  fprintf(stderr, "Formato da câmera: %ux%u\n", fmt.fmt.pix.width,
          fmt.fmt.pix.height);

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
  // 4. Buffers V4L2
  // =================================================================
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = NUM_BUFFERS;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
    fprintf(stderr, "VIDIOC_REQBUFS falhou: %s\n", strerror(errno));
    goto cleanup;
  }

  for (unsigned i = 0; i < req.count; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
      fprintf(stderr, "VIDIOC_QUERYBUF[%u] falhou: %s\n", i, strerror(errno));
      goto cleanup;
    }

    buffers[i].length = buf.length;
    buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
    if (buffers[i].start == MAP_FAILED) {
      fprintf(stderr, "mmap[%u] falhou: %s\n", i, strerror(errno));
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
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
      fprintf(stderr, "VIDIOC_QBUF[%u] falhou: %s\n", i, strerror(errno));
      goto cleanup;
    }
  }

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
    fprintf(stderr, "VIDIOC_STREAMON falhou: %s\n", strerror(errno));
    goto cleanup;
  }

  // =================================================================
  // 5. Encoder H.264
  // =================================================================
  encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!encoder) {
    fprintf(stderr, "Encoder H.264 não encontrado.\n");
    goto cleanup;
  }

  enc_ctx = avcodec_alloc_context3(encoder);
  if (!enc_ctx) {
    fprintf(stderr, "Erro ao alocar contexto do encoder.\n");
    goto cleanup;
  }

  enc_ctx->codec_id = AV_CODEC_ID_H264;
  enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  enc_ctx->width = fmt.fmt.pix.width;
  enc_ctx->height = fmt.fmt.pix.height;
  enc_ctx->time_base = (AVRational){1, FRAME_RATE};
  enc_ctx->framerate = (AVRational){FRAME_RATE, 1};
  enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  enc_ctx->bit_rate = 1000000;
  enc_ctx->gop_size = 12;
  enc_ctx->max_b_frames = 0;
  enc_ctx->profile = AV_PROFILE_H264_HIGH;
  enc_ctx->level = 40;

  if (avcodec_open2(enc_ctx, encoder, NULL) < 0) {
    fprintf(stderr, "Erro ao abrir o encoder H.264.\n");
    goto cleanup;
  }

  // =================================================================
  // 6. MP4
  // =================================================================
  if (avformat_alloc_output_context2(&out_ctx, NULL, "mp4", OUTPUT_FILENAME) <
          0 ||
      !out_ctx) {
    fprintf(stderr, "Erro ao criar contexto MP4.\n");
    goto cleanup;
  }

  out_stream = avformat_new_stream(out_ctx, NULL);
  if (!out_stream) {
    fprintf(stderr, "Erro ao criar stream de saída.\n");
    goto cleanup;
  }

  if (avcodec_parameters_from_context(out_stream->codecpar, enc_ctx) < 0) {
    fprintf(stderr, "Erro ao copiar parâmetros do encoder.\n");
    goto cleanup;
  }
  out_stream->time_base = enc_ctx->time_base;

  if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&out_ctx->pb, OUTPUT_FILENAME, AVIO_FLAG_WRITE) < 0) {
      fprintf(stderr, "Erro ao abrir %s\n", OUTPUT_FILENAME);
      goto cleanup;
    }
  }

  if (avformat_write_header(out_ctx, NULL) < 0) {
    fprintf(stderr, "Erro ao escrever header do MP4.\n");
    goto cleanup;
  }

  // =================================================================
  // 7. Frames
  // =================================================================
  yuv_frame = av_frame_alloc();
  if (!yuv_frame) {
    fprintf(stderr, "Erro ao alocar frame.\n");
    goto cleanup;
  }
  yuv_frame->format = enc_ctx->pix_fmt;
  yuv_frame->width = enc_ctx->width;
  yuv_frame->height = enc_ctx->height;
  if (av_frame_get_buffer(yuv_frame, 32) < 0) {
    fprintf(stderr, "Erro ao alocar buffer do frame.\n");
    goto cleanup;
  }

  sws_ctx = sws_getContext(fmt.fmt.pix.width, fmt.fmt.pix.height,
                           AV_PIX_FMT_YUYV422, enc_ctx->width, enc_ctx->height,
                           enc_ctx->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    fprintf(stderr, "Erro ao criar sws_ctx.\n");
    goto cleanup;
  }

  pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Erro ao alocar pacote.\n");
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

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
      if (errno == EAGAIN) {
        usleep(1000);
        continue;
      }
      fprintf(stderr, "VIDIOC_DQBUF falhou: %s\n", strerror(errno));
      break;
    }

    // ---------- YUYV -> YUV420P ----------
    src_slice[0] = (uint8_t *)buffers[buf.index].start;
    src_stride[0] = fmt.fmt.pix.width * 2;

    if (av_frame_make_writable(yuv_frame) < 0) {
      ioctl(fd, VIDIOC_QBUF, &buf);
      break;
    }

    sws_scale(sws_ctx, (const uint8_t *const *)src_slice, src_stride, 0,
              fmt.fmt.pix.height, yuv_frame->data, yuv_frame->linesize);

    // ===========================================================
    // CRUZ GIRATÓRIA
    // ===========================================================
    double angle = frame_count * CROSS_ROTATION_SPEED;
    draw_rotating_cross(yuv_frame, angle, LINE_THICKNESS);
    // ===========================================================

    yuv_frame->pts = av_rescale_q(frame_count, (AVRational){1, FRAME_RATE},
                                  enc_ctx->time_base);

    // ---------- Encode ----------
    if (avcodec_send_frame(enc_ctx, yuv_frame) < 0) {
      fprintf(stderr, "Erro ao enviar frame ao encoder.\n");
      ioctl(fd, VIDIOC_QBUF, &buf);
      break;
    }

    while (1) {
      ret = avcodec_receive_packet(enc_ctx, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      if (ret < 0) {
        fprintf(stderr, "Erro ao receber pacote do encoder.\n");
        ioctl(fd, VIDIOC_QBUF, &buf);
        goto cleanup;
      }

      pkt->stream_index = out_stream->index;
      av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);

      if (av_interleaved_write_frame(out_ctx, pkt) < 0) {
        fprintf(stderr, "Erro ao gravar pacote.\n");
        av_packet_unref(pkt);
        ioctl(fd, VIDIOC_QBUF, &buf);
        goto cleanup;
      }
      av_packet_unref(pkt);
    }

    frame_count++;

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
      fprintf(stderr, "VIDIOC_QBUF falhou: %s\n", strerror(errno));
      break;
    }

    if ((av_gettime_relative() - start_time) / 1000000 >= DURATION)
      break;
  }

  // =================================================================
  // 9. Flush
  // =================================================================
  avcodec_send_frame(enc_ctx, NULL);
  while (1) {
    ret = avcodec_receive_packet(enc_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    if (ret < 0)
      break;

    pkt->stream_index = out_stream->index;
    av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
    av_interleaved_write_frame(out_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_write_trailer(out_ctx);
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
  av_frame_free(&yuv_frame);
  av_packet_free(&pkt);
  avcodec_free_context(&enc_ctx);

  if (out_ctx) {
    if (out_ctx->pb)
      avio_closep(&out_ctx->pb);
    avformat_free_context(out_ctx);
  }

  return ret;
}
