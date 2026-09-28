#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int save_frame_as_png(AVFrame* frame, int width, int height, int frame_number)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "frame_%03d.png", frame_number);

    // Configurar codec PNG
    const AVCodec* png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec)
    {
        fprintf(stderr, "Codec PNG não encontrado\n");
        return -1;
    }

    AVCodecContext* png_ctx = avcodec_alloc_context3(png_codec);
    if (!png_ctx)
    {
        fprintf(stderr, "Erro ao alocar contexto do codec PNG\n");
        return -1;
    }

    png_ctx->width = width;
    png_ctx->height = height;
    png_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    png_ctx->time_base = (AVRational){1, 1};

    if (avcodec_open2(png_ctx, png_codec, NULL) < 0)
    {
        fprintf(stderr, "Erro ao abrir codec PNG\n");
        avcodec_free_context(&png_ctx);
        return -1;
    }

    // Criar frame RGB
    AVFrame* rgb_frame = av_frame_alloc();
    if (!rgb_frame)
    {
        fprintf(stderr, "Erro ao alocar frame RGB\n");
        avcodec_free_context(&png_ctx);
        return -1;
    }

    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = width;
    rgb_frame->height = height;

    if (av_frame_get_buffer(rgb_frame, 32) < 0)
    {
        fprintf(stderr, "Erro ao alocar buffer do frame RGB\n");
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    // Converter para RGB
    struct SwsContext* sws_ctx =
        sws_getContext(width, height, frame->format, width, height,
                       AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    if (!sws_ctx)
    {
        fprintf(stderr, "Erro ao criar contexto de conversão\n");
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
              height, rgb_frame->data, rgb_frame->linesize);

    // Codificar como PNG
    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Erro ao alocar packet\n");
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    int ret = avcodec_send_frame(png_ctx, rgb_frame);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao enviar frame para encoder\n");
        av_packet_free(&pkt);
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    ret = avcodec_receive_packet(png_ctx, pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Erro ao receber packet do encoder\n");
        av_packet_free(&pkt);
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    // Salvar arquivo
    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        fprintf(stderr, "Erro ao criar arquivo %s\n", filename);
        av_packet_free(&pkt);
        sws_freeContext(sws_ctx);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&png_ctx);
        return -1;
    }

    fwrite(pkt->data, 1, pkt->size, file);
    fclose(file);

    printf("Frame salvo: %s\n", filename);

    // Limpeza
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    av_frame_free(&rgb_frame);
    avcodec_free_context(&png_ctx);

    return 0;
}

int extract_frames(const char* input_file, int num_frames)
{
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    const AVCodec* codec = NULL;
    AVFrame* frame = NULL;
    AVPacket* pkt = NULL;
    int video_stream_idx = -1;
    int ret;

    // Abrir arquivo de entrada
    if (avformat_open_input(&fmt_ctx, input_file, NULL, NULL) < 0)
    {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", input_file);
        return -1;
    }

    // Encontrar informações dos streams
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        fprintf(stderr, "Erro ao encontrar informações dos streams\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Encontrar stream de vídeo
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1)
    {
        fprintf(stderr, "Stream de vídeo não encontrado\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Encontrar decoder
    codec = avcodec_find_decoder(
        fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Decoder não encontrado\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Alocar contexto do codec
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Erro ao alocar contexto do codec\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Copiar parâmetros do codec
    if (avcodec_parameters_to_context(
            codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar) < 0)
    {
        fprintf(stderr, "Erro ao copiar parâmetros do codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Abrir codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "Erro ao abrir codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Alocar frame e packet
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt)
    {
        fprintf(stderr, "Erro ao alocar frame/packet\n");
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // Calcular duração total e intervalo entre frames
    AVStream* video_stream = fmt_ctx->streams[video_stream_idx];
    int64_t duration = video_stream->duration;
    if (duration == AV_NOPTS_VALUE)
    {
        duration = fmt_ctx->duration * video_stream->time_base.den /
                   (video_stream->time_base.num * AV_TIME_BASE);
    }

    int64_t frame_interval = duration / (num_frames + 1);

    printf("Duração do vídeo: %ld frames\n", duration);
    printf("Extraindo %d frames com intervalo de %ld\n", num_frames,
           frame_interval);

    int frames_extracted = 0;
    int64_t current_pts = 0;
    int64_t next_target_pts = frame_interval;

    // Ler e decodificar frames
    while (av_read_frame(fmt_ctx, pkt) >= 0 && frames_extracted < num_frames)
    {
        if (pkt->stream_index == video_stream_idx)
        {
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0)
            {
                fprintf(stderr, "Erro ao enviar packet para decoder\n");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf(stderr, "Erro ao receber frame do decoder\n");
                    goto cleanup;
                }

                current_pts = frame->pts;

                // Verificar se é hora de extrair este frame
                if (current_pts >= next_target_pts)
                {
                    if (save_frame_as_png(frame, codec_ctx->width,
                                          codec_ctx->height,
                                          frames_extracted + 1) == 0)
                    {
                        frames_extracted++;
                        next_target_pts =
                            frame_interval * (frames_extracted + 1);
                        printf("Progresso: %d/%d frames extraídos\n",
                               frames_extracted, num_frames);
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    printf("\nExtração concluída! %d frames salvos como PNG.\n",
           frames_extracted);

cleanup:
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Uso: %s <arquivo_video> <numero_frames>\n", argv[0]);
        printf("Exemplo: %s video.mp4 10\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    int num_frames = atoi(argv[2]);

    if (num_frames <= 0)
    {
        fprintf(stderr, "Número de frames deve ser maior que 0\n");
        return 1;
    }

    printf("Extraindo %d frames de: %s\n", num_frames, input_file);

    return extract_frames(input_file, num_frames);
}
