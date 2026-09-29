#include "video_reader.hpp"

#include <cstdio>
#include <cstring>

namespace {

const char *av_error_string(int error) {
  static thread_local char buffer[AV_ERROR_MAX_STRING_SIZE];

  std::memset(buffer, 0, sizeof(buffer));
  av_strerror(error, buffer, sizeof(buffer));

  return buffer;
}

AVPixelFormat normalize_pixel_format(AVPixelFormat format) {
  switch (format) {
  case AV_PIX_FMT_YUVJ420P:
    return AV_PIX_FMT_YUV420P;

  case AV_PIX_FMT_YUVJ422P:
    return AV_PIX_FMT_YUV422P;

  case AV_PIX_FMT_YUVJ444P:
    return AV_PIX_FMT_YUV444P;

  case AV_PIX_FMT_YUVJ440P:
    return AV_PIX_FMT_YUV440P;

  default:
    return format;
  }
}

bool receive_frame(VideoReaderState *state) {
  const int result =
      avcodec_receive_frame(state->av_codec_ctx, state->av_frame);

  if (result == 0) {
    return true;
  }

  if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
    return false;
  }

  std::fprintf(stderr, "avcodec_receive_frame: %s\n", av_error_string(result));

  return false;
}

} // namespace

bool video_reader_open(VideoReaderState *state, const char *filename) {
  if (state == nullptr || filename == nullptr) {
    return false;
  }

  video_reader_close(state);

  int result =
      avformat_open_input(&state->av_format_ctx, filename, nullptr, nullptr);

  if (result < 0) {
    std::fprintf(stderr, "avformat_open_input: %s\n", av_error_string(result));

    video_reader_close(state);
    return false;
  }

  result = avformat_find_stream_info(state->av_format_ctx, nullptr);

  if (result < 0) {
    std::fprintf(stderr, "avformat_find_stream_info: %s\n",
                 av_error_string(result));

    video_reader_close(state);
    return false;
  }

  const AVCodec *codec = nullptr;

  state->video_stream_index = av_find_best_stream(
      state->av_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

  if (state->video_stream_index < 0) {
    std::fprintf(stderr, "Nenhum stream de video encontrado: %s\n",
                 av_error_string(state->video_stream_index));

    video_reader_close(state);
    return false;
  }

  AVStream *stream = state->av_format_ctx->streams[state->video_stream_index];

  state->time_base = stream->time_base;

  state->av_codec_ctx = avcodec_alloc_context3(codec);

  if (state->av_codec_ctx == nullptr) {
    std::fprintf(stderr, "Nao foi possivel criar AVCodecContext\n");

    video_reader_close(state);
    return false;
  }

  result = avcodec_parameters_to_context(state->av_codec_ctx, stream->codecpar);

  if (result < 0) {
    std::fprintf(stderr, "avcodec_parameters_to_context: %s\n",
                 av_error_string(result));

    video_reader_close(state);
    return false;
  }

  result = avcodec_open2(state->av_codec_ctx, codec, nullptr);

  if (result < 0) {
    std::fprintf(stderr, "avcodec_open2: %s\n", av_error_string(result));

    video_reader_close(state);
    return false;
  }

  state->width = state->av_codec_ctx->width;
  state->height = state->av_codec_ctx->height;

  if (state->width <= 0 || state->height <= 0) {
    std::fprintf(stderr, "Dimensoes de video invalidas: %dx%d\n", state->width,
                 state->height);

    video_reader_close(state);
    return false;
  }

  state->av_frame = av_frame_alloc();

  if (state->av_frame == nullptr) {
    std::fprintf(stderr, "Nao foi possivel criar AVFrame\n");

    video_reader_close(state);
    return false;
  }

  state->av_packet = av_packet_alloc();

  if (state->av_packet == nullptr) {
    std::fprintf(stderr, "Nao foi possivel criar AVPacket\n");

    video_reader_close(state);
    return false;
  }

  return true;
}

bool video_reader_read_frame(VideoReaderState *state,
                             std::uint8_t *frame_buffer, std::int64_t *pts) {
  if (state == nullptr || frame_buffer == nullptr || pts == nullptr ||
      state->av_format_ctx == nullptr || state->av_codec_ctx == nullptr ||
      state->av_frame == nullptr || state->av_packet == nullptr) {
    return false;
  }

  while (true) {
    int result = avcodec_receive_frame(state->av_codec_ctx, state->av_frame);

    if (result == 0) {
      break;
    }

    if (result == AVERROR_EOF) {
      return false;
    }

    if (result != AVERROR(EAGAIN)) {
      std::fprintf(stderr, "avcodec_receive_frame: %s\n",
                   av_error_string(result));

      return false;
    }

    result = av_read_frame(state->av_format_ctx, state->av_packet);

    if (result == AVERROR_EOF) {
      result = avcodec_send_packet(state->av_codec_ctx, nullptr);

      if (result < 0 && result != AVERROR_EOF) {
        std::fprintf(stderr, "Erro ao finalizar decoder: %s\n",
                     av_error_string(result));

        return false;
      }

      continue;
    }

    if (result < 0) {
      std::fprintf(stderr, "av_read_frame: %s\n", av_error_string(result));

      return false;
    }

    if (state->av_packet->stream_index != state->video_stream_index) {
      av_packet_unref(state->av_packet);
      continue;
    }

    result = avcodec_send_packet(state->av_codec_ctx, state->av_packet);

    av_packet_unref(state->av_packet);

    if (result == AVERROR(EAGAIN)) {
      continue;
    }

    if (result < 0) {
      std::fprintf(stderr, "avcodec_send_packet: %s\n",
                   av_error_string(result));

      return false;
    }
  }

  std::int64_t frame_pts = state->av_frame->best_effort_timestamp;

  if (frame_pts == AV_NOPTS_VALUE) {
    frame_pts = state->av_frame->pts;
  }

  *pts = frame_pts;

  const AVPixelFormat source_format = normalize_pixel_format(
      static_cast<AVPixelFormat>(state->av_frame->format));

  state->sws_scaler_ctx = sws_getCachedContext(
      state->sws_scaler_ctx,

      state->av_frame->width, state->av_frame->height, source_format,

      state->width, state->height, AV_PIX_FMT_RGBA,

      SWS_BILINEAR,

      nullptr, nullptr, nullptr);

  if (state->sws_scaler_ctx == nullptr) {
    std::fprintf(stderr, "Nao foi possivel criar SwsContext\n");

    return false;
  }

  std::uint8_t *destination[] = {frame_buffer, nullptr, nullptr, nullptr};

  int destination_linesize[] = {state->width * 4, 0, 0, 0};

  const int scaled_height =
      sws_scale(state->sws_scaler_ctx,

                state->av_frame->data, state->av_frame->linesize,

                0, state->av_frame->height,

                destination, destination_linesize);

  if (scaled_height <= 0) {
    std::fprintf(stderr, "sws_scale falhou\n");

    return false;
  }

  return true;
}

bool video_reader_seek_frame(VideoReaderState *state, std::int64_t timestamp) {
  if (state == nullptr || state->av_format_ctx == nullptr ||
      state->av_codec_ctx == nullptr || state->video_stream_index < 0) {
    return false;
  }

  const int result =
      av_seek_frame(state->av_format_ctx, state->video_stream_index, timestamp,
                    AVSEEK_FLAG_BACKWARD);

  if (result < 0) {
    std::fprintf(stderr, "av_seek_frame: %s\n", av_error_string(result));

    return false;
  }

  avcodec_flush_buffers(state->av_codec_ctx);

  if (state->av_packet != nullptr) {
    av_packet_unref(state->av_packet);
  }

  if (state->av_frame != nullptr) {
    av_frame_unref(state->av_frame);
  }

  return true;
}

void video_reader_close(VideoReaderState *state) {
  if (state == nullptr) {
    return;
  }

  if (state->sws_scaler_ctx != nullptr) {
    sws_freeContext(state->sws_scaler_ctx);
    state->sws_scaler_ctx = nullptr;
  }

  av_packet_free(&state->av_packet);

  av_frame_free(&state->av_frame);

  avcodec_free_context(&state->av_codec_ctx);

  avformat_close_input(&state->av_format_ctx);

  state->width = 0;
  state->height = 0;

  state->time_base = AVRational{0, 1};

  state->video_stream_index = -1;
}
