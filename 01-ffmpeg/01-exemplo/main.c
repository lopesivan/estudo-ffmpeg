#include <libavcodec/avcodec.h>   /* API de codecs da FFmpeg */
#include <libavformat/avformat.h> /* API de formatos/container de vídeo */
#include <libavutil/imgutils.h>   /* Funções utilitárias para imagens */
#include <libswscale/swscale.h> /* Biblioteca de conversão de formatos de pixel */

#include <stdio.h>
#include <time.h>

/* Ativa o log com: make CFLAGS+=-DVERBOSE
 * ou defina VERBOSE 1 na linha abaixo para deixar sempre ligado. */
#ifndef VERBOSE
#define VERBOSE 0
#endif

#if VERBOSE
#define LOG(fmt, ...)                                                        \
    do                                                                       \
    {                                                                        \
        struct timespec _ts;                                                 \
        clock_gettime(CLOCK_MONOTONIC, &_ts);                                \
        fprintf(stderr, "[%5ld.%03ld] %s:%d: " fmt "\n", (long)_ts.tv_sec,   \
                _ts.tv_nsec / 1000000L, __func__, __LINE__, ##__VA_ARGS__);  \
    } while (0)
#else
#define LOG(fmt, ...)                                                        \
    do                                                                       \
    {                                                                        \
    } while (0)
#endif

int main()
{
    AVFormatContext *formatCtx = NULL;
    AVCodecContext *codecCtx = NULL;
    const AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVPacket packet;
    int videoStreamIndex = -1;
    int64_t time_secs = 10; // Tempo em segundos
    int64_t timestamp = 0;
    int gotFrame = 0;
    struct SwsContext *swsCtx = NULL;
    uint8_t *buffer = NULL;
    int bufferWidth, bufferHeight;
    int ret;
    int packetCount = 0;
    int frameCount = 0;

    LOG("iniciando, avcodec=%s avformat=%s", av_version_info(),
        av_version_info());

    // Abre o arquivo de vídeo
    LOG("abrindo video.mp4");
    ret = avformat_open_input(&formatCtx, "video.mp4", NULL, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao abrir o arquivo de vídeo\n");
        return 1;
    }
    LOG("arquivo aberto: formato=%s duração=%lldus", formatCtx->iformat->name,
        (long long)formatCtx->duration);

    // Obtém as informações do formato
    ret = avformat_find_stream_info(formatCtx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao obter as informações do formato\n");
        return 1;
    }
    LOG("stream info obtida: %d streams", formatCtx->nb_streams);

    // Procura o fluxo de vídeo
    for (int i = 0; i < formatCtx->nb_streams; i++)
    {
        LOG("stream[%d] codec_type=%d", i,
            formatCtx->streams[i]->codecpar->codec_type);
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1)
    {
        fprintf(stderr, "Não foi encontrado nenhum fluxo de vídeo\n");
        return 1;
    }
    LOG("stream de vídeo encontrado: index=%d", videoStreamIndex);

    // Obtém o decodificador de vídeo
    codec = avcodec_find_decoder(
        formatCtx->streams[videoStreamIndex]->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr,
                "Não foi possível encontrar o decodificador de vídeo\n");
        return 1;
    }
    LOG("decodificador encontrado: %s", codec->name);

    // Inicializa o contexto do codec
    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        fprintf(stderr, "Não foi possível alocar o contexto do codec\n");
        return 1;
    }
    ret = avcodec_parameters_to_context(
        codecCtx, formatCtx->streams[videoStreamIndex]->codecpar);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao inicializar o contexto do codec\n");
        return 1;
    }
    ret = avcodec_open2(codecCtx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao abrir o codec\n");
        return 1;
    }
    LOG("codec aberto: %dx%d pix_fmt=%d", codecCtx->width, codecCtx->height,
        codecCtx->pix_fmt);

    // Inicializa o frame
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Não foi possível alocar o frame\n");
        return 1;
    }

    // Define o tempo de início da leitura
    timestamp = (int64_t)(time_secs * AV_TIME_BASE);
    LOG("procurando timestamp >= %lld (time=%llds)", (long long)timestamp,
        (long long)time_secs);

    // Lê os pacotes de vídeo até encontrar o quadro desejado
    while (av_read_frame(formatCtx, &packet) >= 0)
    {
        packetCount++;
        if (packet.stream_index == videoStreamIndex)
        {
            LOG("pacote #%d do stream de vídeo, size=%d pts=%lld",
                packetCount, packet.size, (long long)packet.pts);

            ret = avcodec_send_packet(codecCtx, &packet);
            if (ret < 0)
            {
                fprintf(stderr,
                        "Erro ao enviar o pacote para o decodificador\n");
                av_packet_unref(&packet);
                break;
            }

            while (!gotFrame)
            {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    LOG("sem frame pronto ainda (EAGAIN/EOF)");
                    break;
                }
                if (ret < 0)
                {
                    fprintf(stderr,
                            "Erro ao receber o quadro decodificado\n");
                    av_packet_unref(&packet);
                    av_frame_free(&frame);
                    return 1;
                }

                frameCount++;
                LOG("frame #%d decodificado: pts=%lld %dx%d", frameCount,
                    (long long)frame->pts, frame->width, frame->height);

                // Verifica se o quadro está no tempo desejado
                if (frame->pts >= timestamp)
                {
                    LOG("frame no tempo desejado (pts=%lld >= %lld), gerando "
                        "frame.ppm",
                        (long long)frame->pts, (long long)timestamp);

                    // Inicializa o contexto de escala
                    swsCtx = sws_getContext(frame->width, frame->height,
                                            frame->format, frame->width,
                                            frame->height, AV_PIX_FMT_RGB24,
                                            SWS_BILINEAR, NULL, NULL, NULL);
                    if (!swsCtx)
                    {
                        fprintf(stderr,
                                "Erro ao inicializar o contexto de escala\n");
                        av_packet_unref(&packet);
                        av_frame_free(&frame);
                        return 1;
                    }

                    // Aloca o buffer para a imagem
                    bufferWidth = frame->width;
                    bufferHeight = frame->height;
                    buffer = av_malloc(bufferWidth * bufferHeight * 3);
                    LOG("buffer alocado: %dx%d (%d bytes)", bufferWidth,
                        bufferHeight, bufferWidth * bufferHeight * 3);

                    // Escala a imagem para o formato RGB24
                    sws_scale(swsCtx, (const uint8_t *const *)frame->data,
                              frame->linesize, 0, frame->height, &buffer,
                              (const int[]){bufferWidth * 3});
                    LOG("sws_scale concluído");

                    // Salva a imagem em um arquivo PPM
                    FILE *fp = fopen("frame.ppm", "wb");
                    fprintf(fp, "P6\n%d %d\n255\n", bufferWidth,
                            bufferHeight);
                    fwrite(buffer, 1, bufferWidth * bufferHeight * 3, fp);
                    fclose(fp);
                    LOG("frame.ppm gravado (%d pacotes lidos, %d frames "
                        "decodificados)",
                        packetCount, frameCount);

                    // Libera os recursos utilizados
                    sws_freeContext(swsCtx);
                    av_packet_unref(&packet);
                    av_frame_free(&frame);
                    av_freep(&buffer);
                    avcodec_close(codecCtx);
                    avformat_close_input(&formatCtx);
                    return 0;
                }
            }

            av_packet_unref(&packet);
        }
    }

    // Se o quadro desejado não foi encontrado, libera os recursos utilizados
    // e retorna com erro
    LOG("fim do arquivo sem achar o timestamp (%d pacotes, %d frames)",
        packetCount, frameCount);
    av_frame_free(&frame);
    avcodec_close(codecCtx);
    avformat_close_input(&formatCtx);
    fprintf(stderr, "O quadro desejado não foi encontrado\n");
    return 1;
}
