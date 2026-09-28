#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <arquivo de entrada> <arquivo de saída>\n",
            argv[0]);
    return 1;
  }

  // Inicializa as bibliotecas do FFmpeg
  //  XXX: 😢     >
  //  av_register_all() é necessária, embora pareça estar obsoleta desde o
  //  ffmpeg 4.0.
  //  o que deve ser usado em vez de av_register_all()?
  //  Nada. Você pode simplesmente omitir essa chamada de função no ffmpeg 4.0
  //  e posteriores.
  // ->
  // av_register_all(); // comentada

  avformat_network_init();

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

  // Procura o fluxo de vídeo no arquivo de entrada
  int videoStreamIndex = -1;
  for (int i = 0; i < inputFormatCtx->nb_streams; i++) {
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

  // Recupera informações sobre o codec de vídeo utilizado no arquivo de entrada
  AVCodecParameters *codecParams =
      inputFormatCtx->streams[videoStreamIndex]->codecpar;
  AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
  if (!codec) {
    fprintf(stderr, "Não foi possível encontrar um decodificador para o codec "
                    "de vídeo no arquivo de entrada\n");
    avformat_close_input(&inputFormatCtx);
    return 1;
  }

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

  // Cria o fluxo de vídeo no arquivo de saída
  AVStream *videoStream = avformat_new_stream(outputFormatCtx, codec);
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

  // Inicializa o contexto de conversão de cores
  struct SwsContext *swsCtx = sws_getContext(
      codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width,
      codecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

  // Inicializa o pacote de dados de entrada e o quadro de saída
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Não foi possível alocar o quadro de vídeo\n");
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }
  AVFrame *rgbFrame = av_frame_alloc();
  if (!rgbFrame) {
    fprintf(stderr, "Não foi possível alocar o quadro de vídeo\n");
    av_frame_free(frame);
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }
  rgbFrame->format = AV_PIX_FMT_RGB24;
  rgbFrame->width = codecCtx->width;
  rgbFrame->height = codecCtx->height;
  if (av_frame_get_buffer(rgbFrame, 0) < 0) {
    fprintf(stderr, "Não foi possível alocar dados para o quadro de vídeo\n");
    av_frame_free(&frame);
    av_frame_free(&rgbFrame);
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&inputFormatCtx);
    avformat_free_context(outputFormatCtx);
    return 1;
  }

  // Calcula a duração de cada frame
  int64_t frameDuration =
      av_rescale_q(1, inputFormatCtx->streams[videoStreamIndex]->time_base,
                   (AVRational){1, 1000});

  // Grava os frames no arquivo de saída durante 10 segundos
  int64_t endTime = av_gettime_relative() + 10000000;
  while (av_read_frame(inputFormatCtx, &packet) >= 0) {
    if (packet.stream_index == videoStreamIndex) {
      // Decodifica o frame
      int ret = avcodec_send_packet(codecCtx, &packet);
      if (ret < 0) {
        fprintf(stderr,
                "Erro ao enviar o pacote de vídeo para o decodificador\n");
        av_packet_unref(&packet);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&inputFormatCtx);
        avformat_free_context(outputFormatCtx);
        return 1;
      }
      while (ret >= 0) {
        ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        } else if (ret < 0) {
          fprintf(stderr,
                  "Erro ao receber o quadro de vídeo do decodificador\n");
          av_packet_unref(&packet);
          av_frame_free(&frame);
          av_frame_free(&rgbFrame);
          sws_freeContext(swsCtx);
          avcodec_free_context(&codecCtx);
          avformat_close_input(&inputFormatCtx);
          avformat_free_context(outputFormatCtx);
          return 1;
        }

        // Converte o frame para o formato RGB
        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
                  rgbFrame->data, rgbFrame->linesize);

        // Escreve o frame no arquivo de saída
        rgbFrame->pts = av_rescale_q(frame->pts, codecCtx->time_base,
                                     outputFormatCtx->streams[0]->time_base);
        av_interleaved_write_frame(outputFormatCtx, &packet);
        av_packet_unref(&packet);

        // Verifica se já gravou 10 segundos
        if (av_gettime_relative() >= endTime) {
          break;
        }
      }
    } else {
      av_packet_unref(&packet);
    }

    // Verifica se já gravou 10 segundos
    if (av_gettime_relative() >= endTime) {
      break;
    }
  }

  // Finaliza a gravação do arquivo de saída
  av_write_trailer(outputFormatCtx);

  // Libera os recursos alocados
  av_frame_free(&frame);
  av_frame_free(&rgbFrame);
  sws_freeContext(swsCtx);
  avcodec_free_context(&codecCtx);
  avformat_close_input(&inputFormatCtx);
  avformat_free_context(outputFormatCtx);

  return 0;
}
