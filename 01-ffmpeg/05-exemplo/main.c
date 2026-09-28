#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define VIDEO_DEVICE "/dev/video0"
#define OUTPUT_FILE "output.mp4"
#define DURATION 10 // em segundos

int main(int argc, char *argv[]) {
    AVFormatContext *fmt_ctx = NULL;
    AVOutputFormat *output_fmt = NULL;
    AVStream *video_stream = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVPacket pkt;
    int ret = 0, i = 0;
    int frame_count = 0;
    int64_t start_time = 0;
    struct timeval tv;

    // Inicializa o formato de saída
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, OUTPUT_FILE);
    if (!fmt_ctx) {
        fprintf(stderr, "Erro ao inicializar o formato de saída.\n");
        return -1;
    }
    output_fmt = fmt_ctx->oformat;

    // Abre a câmera de vídeo
    AVInputFormat *input_fmt = av_find_input_format("v4l2");
    if (avformat_open_input(&fmt_ctx, VIDEO_DEVICE, input_fmt, NULL) != 0) {
        fprintf(stderr, "Erro ao abrir a câmera de vídeo.\n");
        return -1;
    }

    // Localiza o codec de vídeo
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Não foi possível encontrar o codec de vídeo H.264.\n");
        return -1;
    }

    // Cria o fluxo de vídeo
    video_stream = avformat_new_stream(fmt_ctx, codec);
    if (!video_stream) {
        fprintf(stderr, "Erro ao criar o fluxo de vídeo.\n");
        return -1;
    }
    codec_ctx = video_stream->codec;

    // Configura o codec de vídeo
    codec_ctx->codec_id = codec->id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = 640; // largura da imagem
    codec_ctx->height = 480; // altura da imagem
    codec_ctx->time_base = (AVRational){1, 25}; // taxa de quadros (25 fps)
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Abre o codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Erro ao abrir o codec de vídeo.\n");
        return -1;
    }

    // Cria um frame de imagem
    frame = av_frame_alloc();
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Erro ao criar o frame de imagem.\n");
        return -1;
    }

    // Configura o contexto do formato de saída
    if (!(output_fmt->flags

