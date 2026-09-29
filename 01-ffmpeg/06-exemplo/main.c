#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

#define VIDEO_DEVICE "/dev/video0"
#define OUTPUT_MP4_FILENAME "output.mp4"
#define TARGET_DURATION_S 10
#define FPS 25
#define ENC_W 640
#define ENC_H 480

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int ret = 0;

  // ---------- Entrada (câmera) ----------
  AVFormatContext *in_ctx = NULL;
  const AVInputFormat *in_fmt = NULL;
  AVCodecContext *dec_ctx = NULL;
  const AVCodec *decoder = NULL;
  int video_idx = -1;

  // ---------- Encoder ----------
  AVCodecContext *enc_ctx = NULL;
  const AVCodec *encoder = NULL;
  struct SwsContext *sws_ctx = NULL;
  AVFrame *enc_frame = NULL; // YUV420P (entrada do encoder)
  AVFrame *dec_frame = NULL; // formato nativo da câmera
  AVPacket *in_pkt = NULL;
  AVPacket *out_pkt = NULL;

  // ---------- Saída MP4 ----------
  AVFormatContext *out_ctx = NULL;
  AVStream *out_stream = NULL;

  int64_t start_time = 0;
  int frame_count = 0;

  // =================================================================
  // 1. Entrada v4l2
  // =================================================================
  avdevice_register_all();

  in_fmt = av_find_input_format("v4l2");
  if (!in_fmt) {
    fprintf(stderr, "Formato v4l2 indisponível.\n");
    return -1;
  }

  AVDictionary *opts = NULL;
  av_dict_set(&opts, "video_size", "640x480", 0);
  av_dict_set(&opts, "framerate", "25", 0);

  if (avformat_open_input(&in_ctx, VIDEO_DEVICE, in_fmt, &opts) < 0) {
    fprintf(stderr, "Erro ao abrir %s\n", VIDEO_DEVICE);
    av_dict_free(&opts);
    return -1;
  }
  av_dict_free(&opts);

  if (avformat_find_stream_info(in_ctx, NULL) < 0) {
    fprintf(stderr, "Erro em avformat_find_stream_info\n");
    goto cleanup;
  }

  for (unsigned i = 0; i < in_ctx->nb_streams; i++) {
    if (in_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_idx = (int)i;
      break;
    }
  }
  if (video_idx < 0) {
    fprintf(stderr, "Nenhum stream de vídeo na câmera.\n");
    goto cleanup;
  }

  // =================================================================
  // 2. Decoder da câmera
  // =================================================================
  AVCodecParameters *in_par = in_ctx->streams[video_idx]->codecpar;
  decoder = avcodec_find_decoder(in_par->codec_id);
  if (!decoder) {
    fprintf(stderr, "Decoder não encontrado (codec_id=%d)\n", in_par->codec_id);
    goto cleanup;
  }

  dec_ctx = avcodec_alloc_context3(decoder);
  if (!dec_ctx)
    goto cleanup;

  if (avcodec_parameters_to_context(dec_ctx, in_par) < 0)
    goto cleanup;
  if (avcodec_open2(dec_ctx, decoder, NULL) < 0) {
    fprintf(stderr, "Erro ao abrir decoder.\n");
    goto cleanup;
  }

  // =================================================================
  // 3. Encoder H.264
  // =================================================================
  encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!encoder) {
    fprintf(stderr, "Encoder H.264 não encontrado.\n");
    goto cleanup;
  }

  enc_ctx = avcodec_alloc_context3(encoder);
  if (!enc_ctx)
    goto cleanup;

  enc_ctx->codec_id = AV_CODEC_ID_H264;
  enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  enc_ctx->width = ENC_W;
  enc_ctx->height = ENC_H;
  enc_ctx->time_base = (AVRational){1, FPS};
  enc_ctx->framerate = (AVRational){FPS, 1};
  enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  enc_ctx->bit_rate = 400000;
  enc_ctx->gop_size = 12;
  enc_ctx->max_b_frames = 0;

  if (avcodec_open2(enc_ctx, encoder, NULL) < 0) {
    fprintf(stderr, "Erro ao abrir encoder H.264.\n");
    goto cleanup;
  }

  // =================================================================
  // 4. Saída MP4 (única)
  // =================================================================
  if (avformat_alloc_output_context2(&out_ctx, NULL, "mp4",
                                     OUTPUT_MP4_FILENAME) < 0 ||
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
    fprintf(stderr, "Erro ao copiar parâmetros para o stream.\n");
    goto cleanup;
  }
  out_stream->time_base = enc_ctx->time_base;

  if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&out_ctx->pb, OUTPUT_MP4_FILENAME, AVIO_FLAG_WRITE) < 0) {
      fprintf(stderr, "Erro ao abrir %s\n", OUTPUT_MP4_FILENAME);
      goto cleanup;
    }
  }

  if (avformat_write_header(out_ctx, NULL) < 0) {
    fprintf(stderr, "Erro ao escrever header MP4.\n");
    goto cleanup;
  }

  // =================================================================
  // 5. Frames e pacotes
  // =================================================================
  enc_frame = av_frame_alloc();
  if (!enc_frame)
    goto cleanup;
  enc_frame->format = enc_ctx->pix_fmt;
  enc_frame->width = enc_ctx->width;
  enc_frame->height = enc_ctx->height;
  if (av_frame_get_buffer(enc_frame, 32) < 0) {
    fprintf(stderr, "Erro ao alocar buffer do frame de encode.\n");
    goto cleanup;
  }

  dec_frame = av_frame_alloc();
  if (!dec_frame)
    goto cleanup;

  in_pkt = av_packet_alloc();
  out_pkt = av_packet_alloc();
  if (!in_pkt || !out_pkt)
    goto cleanup;

  // =================================================================
  // 6. Loop principal: captura -> decode -> scale -> encode -> mux
  // =================================================================
  start_time = av_gettime_relative();

  while (1) {
    ret = av_read_frame(in_ctx, in_pkt);
    if (ret < 0)
      break;

    if (in_pkt->stream_index != video_idx) {
      av_packet_unref(in_pkt);
      continue;
    }

    ret = avcodec_send_packet(dec_ctx, in_pkt);
    av_packet_unref(in_pkt);
    if (ret < 0) {
      fprintf(stderr, "Erro send_packet decoder.\n");
      goto cleanup;
    }

    while (1) {
      ret = avcodec_receive_frame(dec_ctx, dec_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        break;
      if (ret < 0) {
        fprintf(stderr, "Erro receive_frame decoder.\n");
        goto cleanup;
      }

      // Sanidade do frame decodificado
      if (dec_frame->format < 0 || dec_frame->width <= 0 ||
          dec_frame->height <= 0) {
        av_frame_unref(dec_frame);
        continue;
      }

      // Cria o contexto de conversão na primeira vez (pix_fmt real
      // da câmera só é conhecido após o primeiro frame decodificado)
      if (!sws_ctx) {
        sws_ctx = sws_getContext(
            dec_frame->width, dec_frame->height,
            (enum AVPixelFormat)dec_frame->format, enc_ctx->width,
            enc_ctx->height, enc_ctx->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
        if (!sws_ctx) {
          fprintf(stderr, "Erro ao criar sws_ctx.\n");
          goto cleanup;
        }
      }

      if (av_frame_make_writable(enc_frame) < 0)
        goto cleanup;

      // Converte para YUV420P no tamanho do encoder
      sws_scale(sws_ctx, (const uint8_t *const *)dec_frame->data,
                dec_frame->linesize, 0, dec_frame->height, enc_frame->data,
                enc_frame->linesize);

      enc_frame->pts =
          av_rescale_q(frame_count, (AVRational){1, FPS}, enc_ctx->time_base);

      ret = avcodec_send_frame(enc_ctx, enc_frame);
      if (ret < 0) {
        fprintf(stderr, "Erro send_frame encoder.\n");
        goto cleanup;
      }

      while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, out_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        if (ret < 0) {
          fprintf(stderr, "Erro receive_packet encoder.\n");
          goto cleanup;
        }

        out_pkt->stream_index = out_stream->index;
        av_packet_rescale_ts(out_pkt, enc_ctx->time_base,
                             out_stream->time_base);

        if (av_interleaved_write_frame(out_ctx, out_pkt) < 0) {
          fprintf(stderr, "Erro ao gravar no MP4.\n");
          goto cleanup;
        }

        av_packet_unref(out_pkt);
      }

      frame_count++;
      av_frame_unref(dec_frame);

      if ((av_gettime_relative() - start_time) / 1000000 >= TARGET_DURATION_S)
        break;
    }

    if ((av_gettime_relative() - start_time) / 1000000 >= TARGET_DURATION_S)
      break;
  }

  // =================================================================
  // 7. Flush do encoder (envia os frames ainda em buffer)
  // =================================================================
  avcodec_send_frame(enc_ctx, NULL);
  while (1) {
    ret = avcodec_receive_packet(enc_ctx, out_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    if (ret < 0)
      break;

    out_pkt->stream_index = out_stream->index;
    av_packet_rescale_ts(out_pkt, enc_ctx->time_base, out_stream->time_base);
    av_interleaved_write_frame(out_ctx, out_pkt);
    av_packet_unref(out_pkt);
  }

  // =================================================================
  // 8. Fecha o muxer
  // =================================================================
  av_write_trailer(out_ctx);

  fprintf(stderr, "OK: %d frames gravados em %s\n", frame_count,
          OUTPUT_MP4_FILENAME);
  ret = 0;

cleanup:
  if (sws_ctx)
    sws_freeContext(sws_ctx);
  av_frame_free(&enc_frame);
  av_frame_free(&dec_frame);
  av_packet_free(&in_pkt);
  av_packet_free(&out_pkt);
  avcodec_free_context(&enc_ctx);
  avcodec_free_context(&dec_ctx);
  avformat_close_input(&in_ctx);

  if (out_ctx) {
    if (out_ctx->pb)
      avio_closep(&out_ctx->pb);
    avformat_free_context(out_ctx);
  }

  return ret;
}
