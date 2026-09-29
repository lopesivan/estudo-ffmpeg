#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>

#define VIDEO_DEVICE "/dev/video0"
#define OUTPUT_FILE  "output.mp4"
#define DURATION     10   // segundos
#define FPS          25
#define WIDTH        640
#define HEIGHT       480

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    
    AVFormatContext* input_ctx  = NULL;   // câmera (v4l2)
    AVFormatContext* output_ctx = NULL;   // arquivo de saída
    const AVOutputFormat* output_fmt = NULL;
    const AVInputFormat*  input_fmt  = NULL;
    AVStream*       video_stream = NULL;
    AVCodecContext* enc_ctx      = NULL;  // encoder H.264 (saída)
    AVCodecContext* dec_ctx      = NULL;  // decoder da câmera (entrada)
    const AVCodec*  encoder      = NULL;
    const AVCodec*  decoder      = NULL;
    struct SwsContext* sws_ctx   = NULL;
    AVFrame*        frame        = NULL;  // frame já convertido, pronto p/ encoder
    AVFrame*        raw_frame    = NULL;  // frame decodificado da câmera
    AVPacket*       pkt          = NULL;  // pacote codificado (saída)
    AVPacket*       in_pkt       = NULL;  // pacote lido da câmera (entrada)
    int ret = 0;
    int frame_count = 0;
    int64_t start_time = 0;
    int in_video_stream_index = -1;
    
    // ---------------------------------------------------------------
    // 1. Abre a câmera (entrada v4l2)
    // ---------------------------------------------------------------
    avdevice_register_all();   // <-- OBRIGATÓRIO antes de av_find_input_format()
    
    input_fmt = av_find_input_format("v4l2");
    
    if (!input_fmt) {
        fprintf(stderr, "Formato de entrada v4l2 não disponível.\n");
        return -1;
    }
    
    // Opções da câmera: resolução e framerate
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "video_size", "640x480", 0);
    av_dict_set(&opts, "framerate",  "25",      0);
    
    if (avformat_open_input(&input_ctx, VIDEO_DEVICE, input_fmt, &opts) != 0) {
        fprintf(stderr, "Erro ao abrir a câmera %s\n", VIDEO_DEVICE);
        av_dict_free(&opts);
        return -1;
    }
    
    av_dict_free(&opts);
    
    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Erro ao obter informações do stream da câmera.\n");
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    // Localiza o stream de vídeo dentro da entrada da câmera
    for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
        if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            in_video_stream_index = (int)i;
            break;
        }
    }
    
    if (in_video_stream_index < 0) {
        fprintf(stderr, "Nenhum stream de vídeo encontrado na câmera.\n");
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    // ---------------------------------------------------------------
    // 1b. Abre o decoder correspondente ao formato entregue pela câmera
    //     (tipicamente MJPEG ou YUYV422, dependendo do driver v4l2)
    // ---------------------------------------------------------------
    AVCodecParameters* in_codecpar = input_ctx->streams[in_video_stream_index]->codecpar;
    decoder = avcodec_find_decoder(in_codecpar->codec_id);
    
    if (!decoder) {
        fprintf(stderr, "Decoder não encontrado para o formato da câmera (codec_id=%d).\n",
                in_codecpar->codec_id);
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    dec_ctx = avcodec_alloc_context3(decoder);
    
    if (!dec_ctx) {
        fprintf(stderr, "Erro ao alocar o contexto do decoder da câmera.\n");
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    if (avcodec_parameters_to_context(dec_ctx, in_codecpar) < 0) {
        fprintf(stderr, "Erro ao configurar o contexto do decoder da câmera.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    if (avcodec_open2(dec_ctx, decoder, NULL) < 0) {
        fprintf(stderr, "Erro ao abrir o decoder da câmera.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    // ---------------------------------------------------------------
    // 2. Cria o contexto de saída
    // ---------------------------------------------------------------
    if (avformat_alloc_output_context2(&output_ctx, NULL, NULL, OUTPUT_FILE) < 0
            || !output_ctx) {
        fprintf(stderr, "Erro ao criar o contexto de saída.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        return -1;
    }
    
    output_fmt = output_ctx->oformat;
    
    // ---------------------------------------------------------------
    // 3. Encontra o encoder H.264
    // ---------------------------------------------------------------
    encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    
    if (!encoder) {
        fprintf(stderr, "Encoder H.264 não encontrado.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    // ---------------------------------------------------------------
    // 4. Cria o stream de vídeo e o contexto do codec de saída
    // ---------------------------------------------------------------
    video_stream = avformat_new_stream(output_ctx, NULL);
    
    if (!video_stream) {
        fprintf(stderr, "Erro ao criar o stream de vídeo.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    enc_ctx = avcodec_alloc_context3(encoder);
    
    if (!enc_ctx) {
        fprintf(stderr, "Erro ao alocar o contexto do codec.\n");
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    enc_ctx->codec_id   = AV_CODEC_ID_H264;
    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->width      = WIDTH;
    enc_ctx->height     = HEIGHT;
    enc_ctx->time_base  = (AVRational) {
        1, FPS
    };
    enc_ctx->framerate  = (AVRational) {
        FPS, 1
    };
    enc_ctx->pix_fmt    = AV_PIX_FMT_YUV420P;
    enc_ctx->bit_rate   = 400000;
    enc_ctx->gop_size   = 12;
    enc_ctx->max_b_frames = 0;
    
    if (output_fmt->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // ---------------------------------------------------------------
    // 5. Abre o codec de saída
    // ---------------------------------------------------------------
    if (avcodec_open2(enc_ctx, encoder, NULL) < 0) {
        fprintf(stderr, "Erro ao abrir o codec de vídeo.\n");
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    // Copia os parâmetros do codec para o stream de saída
    if (avcodec_parameters_from_context(video_stream->codecpar, enc_ctx) < 0) {
        fprintf(stderr, "Erro ao copiar parâmetros do codec.\n");
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    video_stream->time_base = enc_ctx->time_base;
    
    // ---------------------------------------------------------------
    // 5b. Contexto de conversão: formato/resolução da câmera -> YUV420P
    //     no tamanho esperado pelo encoder. Como pix_fmt do decoder só é
    //     conhecido após decodificar o primeiro frame, sws_ctx é criado
    //     dinamicamente dentro do laço, na primeira vez que for preciso.
    // ---------------------------------------------------------------
    
    // ---------------------------------------------------------------
    // 6. Abre o arquivo de saída
    // ---------------------------------------------------------------
    if (!(output_fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, OUTPUT_FILE, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Erro ao abrir o arquivo de saída %s\n", OUTPUT_FILE);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&input_ctx);
            avformat_free_context(output_ctx);
            return -1;
        }
    }
    
    if (avformat_write_header(output_ctx, NULL) < 0) {
        fprintf(stderr, "Erro ao escrever o cabeçalho do arquivo de saída.\n");
        avio_closep(&output_ctx->pb);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&input_ctx);
        avformat_free_context(output_ctx);
        return -1;
    }
    
    // ---------------------------------------------------------------
    // 7. Aloca frames e pacotes
    // ---------------------------------------------------------------
    frame = av_frame_alloc();
    
    if (!frame) {
        fprintf(stderr, "Erro ao alocar o frame.\n");
        goto cleanup;
    }
    
    frame->format = enc_ctx->pix_fmt;
    frame->width  = enc_ctx->width;
    frame->height = enc_ctx->height;
    
    if (av_frame_get_buffer(frame, 32) < 0) {
        fprintf(stderr, "Erro ao alocar buffer do frame.\n");
        goto cleanup;
    }
    
    raw_frame = av_frame_alloc();
    
    if (!raw_frame) {
        fprintf(stderr, "Erro ao alocar o frame de captura.\n");
        goto cleanup;
    }
    
    pkt = av_packet_alloc();
    
    if (!pkt) {
        fprintf(stderr, "Erro ao alocar o pacote.\n");
        goto cleanup;
    }
    
    in_pkt = av_packet_alloc();
    
    if (!in_pkt) {
        fprintf(stderr, "Erro ao alocar o pacote de entrada.\n");
        goto cleanup;
    }
    
    // ---------------------------------------------------------------
    // 8. Loop principal: captura -> decode -> escala -> encode -> grava
    //    por DURATION segundos
    // ---------------------------------------------------------------
    start_time = av_gettime_relative();
    int64_t pts = 0;
    
    while (1) {
        ret = av_read_frame(input_ctx, in_pkt);
        
        if (ret < 0) {
            break; // câmera fechou ou erro de leitura
        }
        
        if (in_pkt->stream_index != in_video_stream_index) {
            av_packet_unref(in_pkt);
            continue;
        }
        
        // Envia o pacote cru da câmera ao decoder
        ret = avcodec_send_packet(dec_ctx, in_pkt);
        av_packet_unref(in_pkt);
        
        if (ret < 0) {
            fprintf(stderr, "Erro ao enviar pacote da câmera ao decoder.\n");
            continue;
        }
        
        // Um pacote da câmera pode gerar zero ou mais frames decodificados
        while (1) {
            ret = avcodec_receive_frame(dec_ctx, raw_frame);
            
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                fprintf(stderr, "Erro ao decodificar frame da câmera.\n");
                goto cleanup;
            }
            
            // Cria o contexto de escala/conversão na primeira vez, agora
            // que já sabemos o pix_fmt real entregue pelo decoder
            if (!sws_ctx) {
                sws_ctx = sws_getContext(
                              raw_frame->width, raw_frame->height, (enum AVPixelFormat)raw_frame->format,
                              enc_ctx->width, enc_ctx->height, enc_ctx->pix_fmt,
                              SWS_BILINEAR, NULL, NULL, NULL);
                              
                if (!sws_ctx) {
                    fprintf(stderr, "Erro ao criar contexto de conversão de pixel.\n");
                    goto cleanup;
                }
            }
            
            if (av_frame_make_writable(frame) < 0) {
                goto cleanup;
            }
            
            // Converte o frame da câmera (formato/resolução nativos) para
            // YUV420P no tamanho do encoder
            sws_scale(sws_ctx, (const uint8_t* const*)raw_frame->data, raw_frame->linesize,
                      0, raw_frame->height, frame->data, frame->linesize);
                      
            frame->pts = pts++;
            
            // Envia o frame convertido ao encoder
            ret = avcodec_send_frame(enc_ctx, frame);
            
            if (ret < 0) {
                fprintf(stderr, "Erro ao enviar frame ao encoder.\n");
                goto cleanup;
            }
            
            // Recebe pacotes codificados
            while (ret >= 0) {
                ret = avcodec_receive_packet(enc_ctx, pkt);
                
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Erro ao receber pacote do encoder.\n");
                    goto cleanup;
                }
                
                pkt->stream_index = video_stream->index;
                av_packet_rescale_ts(pkt, enc_ctx->time_base, video_stream->time_base);
                
                ret = av_interleaved_write_frame(output_ctx, pkt);
                av_packet_unref(pkt);
                
                if (ret < 0) {
                    fprintf(stderr, "Erro ao gravar pacote no arquivo.\n");
                    goto cleanup;
                }
            }
            
            frame_count++;
        }
        
        // Verifica tempo decorrido
        if ((av_gettime_relative() - start_time) / 1000000 >= DURATION) {
            break;
        }
    }
    
    // Flush do encoder
    avcodec_send_frame(enc_ctx, NULL);
    
    while (avcodec_receive_packet(enc_ctx, pkt) >= 0) {
        pkt->stream_index = video_stream->index;
        av_packet_rescale_ts(pkt, enc_ctx->time_base, video_stream->time_base);
        av_interleaved_write_frame(output_ctx, pkt);
        av_packet_unref(pkt);
    }
    
    av_write_trailer(output_ctx);
    fprintf(stderr, "Gravação concluída: %d frames em %s\n", frame_count, OUTPUT_FILE);
    
cleanup:

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    
    av_frame_free(&frame);
    av_frame_free(&raw_frame);
    av_packet_free(&pkt);
    av_packet_free(&in_pkt);
    avcodec_free_context(&enc_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&input_ctx);
    
    if (output_ctx) {
        if (output_ctx->pb) {
            avio_closep(&output_ctx->pb);
        }
        
        avformat_free_context(output_ctx);
    }
    
    return 0;
}
