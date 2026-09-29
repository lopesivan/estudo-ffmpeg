#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

int main (int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf (stderr, "Uso: %s <arquivo de entrada> <arquivo de saída>\n", argv[0]);
        return 1;
    }

    // FFmpeg >= 4.0: registro é automático
    avformat_network_init();

    AVFormatContext *inputFormatCtx = NULL;
    if (avformat_open_input (&inputFormatCtx, argv[1], NULL, NULL) < 0)
    {
        fprintf (stderr, "Não foi possível abrir o arquivo de entrada '%s'\n", argv[1]);
        return 1;
    }

    if (avformat_find_stream_info (inputFormatCtx, NULL) < 0)
    {
        fprintf (stderr, "Não foi possível recuperar as informações do arquivo de entrada\n");
        avformat_close_input (&inputFormatCtx);
        return 1;
    }

    int videoStreamIndex = -1;
    for (int i = 0; i < (int)inputFormatCtx->nb_streams; i++)
    {
        if (inputFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex < 0)
    {
        fprintf (stderr, "Não foi possível encontrar um fluxo de vídeo no arquivo de entrada\n");
        avformat_close_input (&inputFormatCtx);
        return 1;
    }

    AVCodecParameters *codecParams = inputFormatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder (codecParams->codec_id);
    if (!codec)
    {
        fprintf (stderr, "Não foi possível encontrar um decodificador para o codec de vídeo\n");
        avformat_close_input (&inputFormatCtx);
        return 1;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3 (codec);
    if (!codecCtx)
    {
        fprintf (stderr, "Não foi possível alocar o contexto do codec de vídeo\n");
        avformat_close_input (&inputFormatCtx);
        return 1;
    }
    if (avcodec_parameters_to_context (codecCtx, codecParams) < 0)
    {
        fprintf (stderr, "Não foi possível configurar o contexto do codec de vídeo\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        return 1;
    }
    if (avcodec_open2 (codecCtx, codec, NULL) < 0)
    {
        fprintf (stderr, "Não foi possível abrir o codec de vídeo\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        return 1;
    }

    AVFormatContext *outputFormatCtx = NULL;
    if (avformat_alloc_output_context2 (&outputFormatCtx, NULL, NULL, argv[2]) < 0)
    {
        fprintf (stderr, "Não foi possível criar o arquivo de saída '%s'\n", argv[2]);
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        return 1;
    }

    AVStream *videoStream = avformat_new_stream (outputFormatCtx, NULL);
    if (!videoStream)
    {
        fprintf (stderr, "Não foi possível criar o fluxo de vídeo no arquivo de saída\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    if (avcodec_parameters_from_context (videoStream->codecpar, codecCtx) < 0)
    {
        fprintf (stderr, "Não foi possível configurar o codec de vídeo no arquivo de saída\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    if (!(outputFormatCtx->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open (&outputFormatCtx->pb, argv[2], AVIO_FLAG_WRITE) < 0)
        {
            fprintf (stderr, "Não foi possível abrir o arquivo de saída '%s'\n", argv[2]);
            avcodec_free_context (&codecCtx);
            avformat_close_input (&inputFormatCtx);
            avformat_free_context (outputFormatCtx);
            return 1;
        }
    }

    if (avformat_write_header (outputFormatCtx, NULL) < 0)
    {
        fprintf (stderr, "Não foi possível escrever o cabeçalho do arquivo de saída\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    struct SwsContext *swsCtx = sws_getContext (codecCtx->width, codecCtx->height,
                                                codecCtx->pix_fmt,
                                                codecCtx->width, codecCtx->height,
                                                AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    if (!swsCtx)
    {
        fprintf (stderr, "Não foi possível criar o contexto de conversão\n");
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet)
    {
        fprintf (stderr, "Não foi possível alocar o pacote\n");
        sws_freeContext (swsCtx);
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        fprintf (stderr, "Não foi possível alocar o quadro de vídeo\n");
        av_packet_free (&packet);
        sws_freeContext (swsCtx);
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    AVFrame *rgbFrame = av_frame_alloc();
    if (!rgbFrame)
    {
        fprintf (stderr, "Não foi possível alocar o quadro RGB\n");
        av_frame_free (&frame);
        av_packet_free (&packet);
        sws_freeContext (swsCtx);
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width  = codecCtx->width;
    rgbFrame->height = codecCtx->height;
    if (av_frame_get_buffer (rgbFrame, 0) < 0)
    {
        fprintf (stderr, "Não foi possível alocar dados para o quadro RGB\n");
        av_frame_free (&frame);
        av_frame_free (&rgbFrame);
        av_packet_free (&packet);
        sws_freeContext (swsCtx);
        avcodec_free_context (&codecCtx);
        avformat_close_input (&inputFormatCtx);
        avformat_free_context (outputFormatCtx);
        return 1;
    }

    int64_t endTime = av_gettime_relative() + 10000000;  // 10 segundos

    while (av_read_frame (inputFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == videoStreamIndex)
        {
            int ret = avcodec_send_packet (codecCtx, packet);
            if (ret < 0)
            {
                fprintf (stderr, "Erro ao enviar o pacote para o decodificador\n");
                av_packet_unref (packet);
                break;
            }
            while (ret >= 0)
            {
                ret = avcodec_receive_frame (codecCtx, frame);
                if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0)
                {
                    fprintf (stderr, "Erro ao receber o quadro do decodificador\n");
                    break;
                }

                sws_scale (swsCtx,
                           (const uint8_t * const *)frame->data,
                           frame->linesize, 0, frame->height,
                           rgbFrame->data, rgbFrame->linesize);

                // AQUI: em um exemplo real, você codificaria rgbFrame
                // com um encoder e escreveria o pacote resultante.
                // Este exemplo apenas mantém a estrutura original.

                if (av_gettime_relative() >= endTime)
                    break;
            }
            av_packet_unref (packet);
        }
        else
        {
            av_packet_unref (packet);
        }

        if (av_gettime_relative() >= endTime)
            break;
    }

    av_write_trailer (outputFormatCtx);

    av_frame_free (&frame);
    av_frame_free (&rgbFrame);
    av_packet_free (&packet);
    sws_freeContext (swsCtx);
    avcodec_free_context (&codecCtx);
    avformat_close_input (&inputFormatCtx);
    if (outputFormatCtx->pb)
        avio_closep (&outputFormatCtx->pb);
    avformat_free_context (outputFormatCtx);

    avformat_network_deinit();
    return 0;
}
