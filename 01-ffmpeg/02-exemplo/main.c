#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

/* Ativa o log com: make CFLAGS+=-DVERBOSE=1 */
#ifndef VERBOSE
#define VERBOSE 0
#endif

#if VERBOSE
#include <time.h>
#define LOG(fmt, ...)                                                          \
  do {                                                                         \
    struct timespec _ts;                                                       \
    clock_gettime(CLOCK_MONOTONIC, &_ts);                                      \
    fprintf(stderr, "[%5ld.%03ld] %s:%d: " fmt "\n", (long)_ts.tv_sec,         \
            _ts.tv_nsec / 1000000L, __func__, __LINE__, ##__VA_ARGS__);        \
  } while (0)
#else
#define LOG(fmt, ...)                                                          \
  do {                                                                         \
  } while (0)
#endif

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <arquivo de entrada> <arquivo de saída>\n",
            argv[0]);
    return 1;
  }

  avformat_network_init();
  LOG("iniciado: entrada=%s saida=%s", argv[1], argv[2]);

  // Abre o arquivo de entrada
  AVFormatContext *inputFormatCtx = NULL;
  if (avformat_open_input(&inputFormatCtx, argv[1], NULL, NULL) < 0) {
    fprintf(stderr, "Não foi possível abrir o arquivo de entrada '%s'\n",
            argv[1]);
    return 1;
  }

  // Recupera informações sobre o arquivo de entrada
  if (avformat_find_stream_info(inputFormatCtx, NULL) < 0) {
    fprintf(
        stderr,
        "Não foi possível recuperar as informações do arquivo de entrada\n");
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  LOG("entrada aberta: formato=%s streams=%u", inputFormatCtx->iformat->name,
      inputFormatCtx->nb_streams);

  // Procura o fluxo de vídeo no arquivo de entrada
  int videoStreamIndex = -1;
  for (unsigned int i = 0; i < inputFormatCtx->nb_streams; i++) {
    if (inputFormatCtx->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i;
      break;
    }
  }
  if (videoStreamIndex < 0) {
    fprintf(
        stderr,
        "Não foi possível encontrar um fluxo de vídeo no arquivo de entrada\n");
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  LOG("stream de vídeo: index=%d", videoStreamIndex);

  // Recupera informações sobre o codec de vídeo utilizado no arquivo de entrada
  AVCodecParameters *codecParams =
      inputFormatCtx->streams[videoStreamIndex]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
  if (!codec) {
    fprintf(stderr, "Não foi possível encontrar um decodificador para o codec "
                    "de vídeo no arquivo de entrada\n");
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  LOG("decodificador: %s", codec->name);

  // Abre o codec de vídeo
  AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
  if (!codecCtx) {
    fprintf(stderr, "Não foi possível alocar o contexto do codec de vídeo\n");
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  if (avcodec_parameters_to_context(codecCtx, codecParams) < 0) {
    fprintf(stderr,
            "Não foi possível configurar o contexto do codec de vídeo\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  if (avcodec_open2(codecCtx, codec, NULL) < 0) {
    fprintf(stderr, "Não foi possível abrir o codec de vídeo\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    return 1;
  }
  LOG("codec aberto: %dx%d pix_fmt=%d", codecCtx->width, codecCtx->height,
      codecCtx->pix_fmt);

  // Cria o arquivo de saída
  AVFormatContext *outputFormatCtx = NULL;
  if (avformat_alloc_output_context2(&outputFormatCtx, NULL, NULL, argv[2]) <
      0) {
    fprintf(stderr, "Não foi possível criar o arquivo de saída '%s'\n",
            argv[2]);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    return 1;
  }

  // Cria o fluxo de vídeo no arquivo de saída (cópia, sem recodificar)
  AVStream *videoStream = avformat_new_stream(outputFormatCtx, NULL);
  if (!videoStream) {
    fprintf(stderr,
            "Não foi possível criar o fluxo de vídeo no arquivo de saída\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }

  // Configura o contexto do codec de vídeo no arquivo de saída
  if (avcodec_parameters_from_context(videoStream->codecpar, codecCtx) < 0) {
    fprintf(
        stderr,
        "Não foi possível configurar o codec de vídeo no arquivo de saída\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }
  videoStream->time_base = inputFormatCtx->streams[videoStreamIndex]->time_base;

  // Abre o arquivo de saída
  if (!(outputFormatCtx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&outputFormatCtx->pb, argv[2], AVIO_FLAG_WRITE) < 0) {
      fprintf(stderr, "Não foi possível abrir o arquivo de saída '%s'\n",
              argv[2]);
      avcodec_free_context(&codecCtx);
      avformat_close_input(&inputFormatCtx);
      avformat_free_context(outputFormatCtx);
      return 1;
    }
  }

  // Escreve o cabeçalho do arquivo de saída
  if (avformat_write_header(outputFormatCtx, NULL) < 0) {
    fprintf(stderr,
            "Não foi possível escrever o cabeçalho do arquivo de saída\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }
  LOG("cabeçalho de saída escrito");

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    fprintf(stderr, "Não foi possível alocar o pacote\n");
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }

  // Grava os frames no arquivo de saída durante 10 segundos.
  // Isto é uma cópia de fluxo (remux): os pacotes já codificados do
  // stream de vídeo de entrada são reescritos direto no arquivo de
  // saída, sem decodificar nem recodificar.
  int64_t startPts = AV_NOPTS_VALUE;
  int64_t endTime = av_gettime_relative() + 10000000;
  int packetCount = 0;

  while (av_read_frame(inputFormatCtx, packet) >= 0) {
    if (packet->stream_index == videoStreamIndex) {
      if (startPts == AV_NOPTS_VALUE)
        startPts = packet->pts;

      packet->stream_index = videoStream->index;
      av_packet_rescale_ts(packet,
                           inputFormatCtx->streams[videoStreamIndex]->time_base,
                           videoStream->time_base);

      if (av_interleaved_write_frame(outputFormatCtx, packet) < 0) {
        fprintf(stderr, "Erro ao escrever o pacote no arquivo de saída\n");
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&inputFormatCtx);
        avformat_free_context(outputFormatCtx);
        return 1;
      }
      packetCount++;
      LOG("pacote #%d escrito, pts=%lld", packetCount, (long long)packet->pts);
    } else {
      av_packet_unref(packet);
    }

    if (av_gettime_relative() >= endTime) {
      LOG("limite de 10s atingido, parando");
      break;
    }
  }

  // Finaliza a gravação do arquivo de saída
  av_write_trailer(outputFormatCtx);
  LOG("trailer escrito, total de %d pacotes", packetCount);

  // Libera os recursos alocados
  av_packet_free(&packet);
  avcodec_free_context(&codecCtx);
  avformat_close_input(&inputFormatCtx);
  if (outputFormatCtx && !(outputFormatCtx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&outputFormatCtx->pb);
  avformat_free_context(outputFormatCtx);

  return 0;
}
