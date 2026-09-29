#pragma once

#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

struct VideoReaderState {
  int width = 0;
  int height = 0;

  AVRational time_base{0, 1};

  AVFormatContext *av_format_ctx = nullptr;
  AVCodecContext *av_codec_ctx = nullptr;

  int video_stream_index = -1;

  AVFrame *av_frame = nullptr;
  AVPacket *av_packet = nullptr;

  SwsContext *sws_scaler_ctx = nullptr;
};

bool video_reader_open(VideoReaderState *state, const char *filename);

bool video_reader_read_frame(VideoReaderState *state,
                             std::uint8_t *frame_buffer, std::int64_t *pts);

bool video_reader_seek_frame(VideoReaderState *state, std::int64_t timestamp);

void video_reader_close(VideoReaderState *state);
