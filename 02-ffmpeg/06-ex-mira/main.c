#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

#define DEVICE_PATH "/dev/video0"
#define OUTPUT_FILENAME "output.mp4"
#define OVERLAY_PNG "miras/mira-drone.png" /* gerado por ./gerar_mira.sh */
#define FRAME_RATE 30
#define DURATION 10
#define CAPTURE_W 640
#define CAPTURE_H 480
#define NUM_BUFFERS 4

typedef struct {
    void* start;
    size_t length;
} MappedBuffer;

/* Estado do grafo de filtros: uma entrada (frame da câmera) -> overlay da
 * mira (carregada do PNG estático) -> uma saída (frame já composto). */
typedef struct {
    AVFilterGraph* graph;
    AVFilterContext* buffersrc_ctx;
    AVFilterContext* buffersink_ctx;
} FilterState;

/* Monta o grafo:
 *   buffer (in) --------------------\
 *                                    overlay -> format=yuv420p -> buffersink
 *   movie=mira.png,format=rgba (wm) /
 *
 * O filtro "movie" abre o PNG usando o demuxer image2 + decoder png
 * internos do FFmpeg, então não precisamos decodificar a mira à mão: o
 * próprio libavfilter cuida disso, uma única vez, na configuração do grafo.
 */
static int init_overlay_filter(FilterState* fs, int width, int height,
                               enum AVPixelFormat pix_fmt,
                               AVRational time_base,
                               const char* overlay_png_path)
{
    char args[512];
    char filter_descr[512];
    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    int ret = 0;
    
    fs->graph = avfilter_graph_alloc();
    
    if (!outputs || !inputs || !fs->graph) {
        fprintf(stderr, "Erro ao alocar estruturas do filtro\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* Parâmetros do frame que vamos empurrar para dentro do grafo (pad "in") */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",
             width, height, pix_fmt, time_base.num, time_base.den);
             
    ret = avfilter_graph_create_filter(&fs->buffersrc_ctx, buffersrc, "in",
                                       args, NULL, fs->graph);
                                       
    if (ret < 0) {
        fprintf(stderr, "Erro ao criar o buffer source do filtro\n");
        goto end;
    }
    
    ret = avfilter_graph_create_filter(&fs->buffersink_ctx, buffersink, "out",
                                       NULL, NULL, fs->graph);
                                       
    if (ret < 0) {
        fprintf(stderr, "Erro ao criar o buffersink do filtro\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(fs->buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
                              
    if (ret < 0) {
        fprintf(stderr, "Erro ao configurar o formato de saída do filtro\n");
        goto end;
    }
    
    /* Liga o pad de saída do nosso código ao pad "in" do grafo textual, e o
     * pad "out" do grafo textual ao buffersink que vamos ler depois. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = fs->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    
    inputs->name = av_strdup("out");
    inputs->filter_ctx = fs->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    
    if (!outputs->name || !inputs->name) {
        fprintf(stderr, "Erro ao alocar nomes dos pads do filtro\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* format=rgba na mira garante que o canal alfa do PNG seja respeitado.
     * format=yuv420p em [main] normaliza a entrada antes do overlay.
     * overlay=(W-w)/2:(H-h)/2 centraliza a mira automaticamente, não importa
     * o tamanho do PNG nem o tamanho do vídeo.
     * format=yuv420p no final garante que o encoder H.264 receba o formato
     * que ele espera. */
    ret = snprintf(
              filter_descr, sizeof(filter_descr),
              "movie='%s',format=rgba[wm];"
              "[in]format=yuv420p[main];"
              "[main][wm]overlay=(W-w)/2:(H-h)/2:format=auto[ov];"
              "[ov]format=yuv420p[out]",
              overlay_png_path);
              
    if (ret < 0 || (size_t)ret >= sizeof(filter_descr)) {
        fprintf(stderr, "Caminho do PNG longo demais para o buffer do filtro\n");
        ret = AVERROR(ENAMETOOLONG);
        goto end;
    }
    
    ret = avfilter_graph_parse_ptr(fs->graph, filter_descr, &inputs, &outputs,
                                   NULL);
                                   
    if (ret < 0) {
        fprintf(stderr,
                "Erro ao interpretar o grafo de filtros (mira.png existe? "
                "filtro 'movie' e decoder 'png' disponíveis?)\n");
        goto end;
    }
    
    ret = avfilter_graph_config(fs->graph, NULL);
    
    if (ret < 0) {
        fprintf(stderr, "Erro ao configurar o grafo de filtros\n");
        goto end;
    }
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

int main(void)
{
    int ret = 0;
    int fd = -1;
    
    MappedBuffer buffers[NUM_BUFFERS];
    memset(buffers, 0, sizeof(buffers));
    
    AVFormatContext* ctx_out = NULL;
    AVStream* stream = NULL;
    const AVCodec* codec = NULL;
    AVCodecContext* codec_ctx = NULL;
    struct SwsContext* sws_ctx = NULL;
    AVFrame* frame = NULL;      /* YUV420P antes do overlay */
    AVFrame* filt_frame = NULL; /* YUV420P depois do overlay, pronto p/ encoder */
    AVPacket* pkt = NULL;
    FilterState filt = {0};
    
    // =================================================================
    // 1. Abre o dispositivo V4L2
    // =================================================================
    fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
    
    if (fd == -1) {
        fprintf(stderr, "Could not open device %s: %s\n", DEVICE_PATH,
                strerror(errno));
        return 1;
    }
    
    struct v4l2_capability cap;
    
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        fprintf(stderr, "VIDIOC_QUERYCAP: %s\n", strerror(errno));
        goto cleanup;
    }
    
    __u32 caps = cap.capabilities;
    
    if (caps & V4L2_CAP_DEVICE_CAPS) {
        caps = cap.device_caps;
    }
    
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        goto cleanup;
    }
    
    if (!(caps & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        goto cleanup;
    }
    
    // =================================================================
    // 2. Formato YUYV 640x480
    // =================================================================
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = CAPTURE_W;
    fmt.fmt.pix.height = CAPTURE_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        fprintf(stderr, "VIDIOC_S_FMT: %s\n", strerror(errno));
        goto cleanup;
    }
    
    fprintf(stderr, "Formato da câmera: %ux%u, fourcc=0x%08X\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);
            
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
        fprintf(stderr,
                "Driver negociou formato diferente do pedido (esperado YUYV, "
                "recebido fourcc=0x%08X)\n",
                fmt.fmt.pix.pixelformat);
        goto cleanup;
    }
    
    // =================================================================
    // 3. Framerate
    // =================================================================
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = FRAME_RATE;
        ioctl(fd, VIDIOC_S_PARM, &parm);
    }
    
    // =================================================================
    // 4. Pipeline V4L2: REQBUFS -> QUERYBUF -> mmap -> QBUF -> STREAMON
    // =================================================================
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        fprintf(stderr, "VIDIOC_REQBUFS: %s\n", strerror(errno));
        goto cleanup;
    }
    
    if (req.count < 2) {
        fprintf(stderr, "Buffers insuficientes (%u)\n", req.count);
        goto cleanup;
    }
    
    if (req.count > NUM_BUFFERS) {
        fprintf(stderr,
                "Driver alocou mais buffers (%u) do que o suportado (%d)\n",
                req.count, NUM_BUFFERS);
        goto cleanup;
    }
    
    for (unsigned i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            fprintf(stderr, "VIDIOC_QUERYBUF[%u]: %s\n", i, strerror(errno));
            goto cleanup;
        }
        
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);
                                
        if (buffers[i].start == MAP_FAILED) {
            fprintf(stderr, "mmap[%u]: %s\n", i, strerror(errno));
            buffers[i].start = NULL;
            goto cleanup;
        }
    }
    
    for (unsigned i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            fprintf(stderr, "VIDIOC_QBUF[%u]: %s\n", i, strerror(errno));
            goto cleanup;
        }
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        fprintf(stderr, "VIDIOC_STREAMON: %s\n", strerror(errno));
        goto cleanup;
    }
    
    // =================================================================
    // 5. Encoder H.264
    // =================================================================
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    
    if (!codec) {
        fprintf(stderr, "Could not find encoder H.264\n");
        goto cleanup;
    }
    
    codec_ctx = avcodec_alloc_context3(codec);
    
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
        goto cleanup;
    }
    
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->width = fmt.fmt.pix.width;
    codec_ctx->height = fmt.fmt.pix.height;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->time_base = (AVRational) {
        1, FRAME_RATE
    };
    codec_ctx->framerate = (AVRational) {
        FRAME_RATE, 1
    };
    codec_ctx->bit_rate = 1000000;
    codec_ctx->gop_size = 12;
    codec_ctx->max_b_frames = 0;
    codec_ctx->profile = AV_PROFILE_H264_HIGH;
    codec_ctx->level = 40;
    
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open encoder\n");
        goto cleanup;
    }
    
    // =================================================================
    // 6. Contexto de saída MP4
    // =================================================================
    if (avformat_alloc_output_context2(&ctx_out, NULL, "mp4", OUTPUT_FILENAME) <
            0 ||
            !ctx_out) {
        fprintf(stderr, "Could not allocate output context\n");
        goto cleanup;
    }
    
    stream = avformat_new_stream(ctx_out, NULL);
    
    if (!stream) {
        fprintf(stderr, "Could not create video stream\n");
        goto cleanup;
    }
    
    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        goto cleanup;
    }
    
    stream->time_base = codec_ctx->time_base;
    
    if (!(ctx_out->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ctx_out->pb, OUTPUT_FILENAME, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open output file\n");
            goto cleanup;
        }
    }
    
    if (avformat_write_header(ctx_out, NULL) < 0) {
        fprintf(stderr, "Could not write header\n");
        goto cleanup;
    }
    
    // =================================================================
    // 7. Frame YUV420P (pré-overlay) + sws_ctx + grafo de filtros
    // =================================================================
    frame = av_frame_alloc();
    
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        goto cleanup;
    }
    
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    
    if (av_frame_get_buffer(frame, 32) < 0) {
        fprintf(stderr, "Could not allocate frame buffer\n");
        goto cleanup;
    }
    
    filt_frame = av_frame_alloc();
    
    if (!filt_frame) {
        fprintf(stderr, "Could not allocate filtered frame\n");
        goto cleanup;
    }
    
    // YUYV (640x480) -> YUV420P (640x480)
    sws_ctx =
        sws_getContext(fmt.fmt.pix.width, fmt.fmt.pix.height, AV_PIX_FMT_YUYV422,
                       codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                       SWS_BILINEAR, NULL, NULL, NULL);
                       
    if (!sws_ctx) {
        fprintf(stderr, "Could not create sws context\n");
        goto cleanup;
    }
    
    if (init_overlay_filter(&filt, codec_ctx->width, codec_ctx->height,
                            codec_ctx->pix_fmt, codec_ctx->time_base,
                            OVERLAY_PNG) < 0) {
        fprintf(stderr, "Could not initialize overlay filter graph\n");
        goto cleanup;
    }
    
    pkt = av_packet_alloc();
    
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        goto cleanup;
    }
    
    // =================================================================
    // 8. Loop principal: captura -> escala -> overlay -> encode -> grava
    // =================================================================
    int64_t start_time = av_gettime_relative();
    int frame_count = 0;
    
    uint8_t* src_slice[4] = {NULL, NULL, NULL, NULL};
    int src_stride[4] = {0, 0, 0, 0};
    
    while (1) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            if (errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            
            fprintf(stderr, "VIDIOC_DQBUF: %s\n", strerror(errno));
            break;
        }
        
        // -------- YUYV -> YUV420P --------
        src_slice[0] = (uint8_t*)buffers[buf.index].start;
        src_stride[0] = fmt.fmt.pix.width * 2; // YUYV = 2 bytes/pixel
        
        if (av_frame_make_writable(frame) < 0) {
            ioctl(fd, VIDIOC_QBUF, &buf);
            break;
        }
        
        sws_scale(sws_ctx, (const uint8_t* const*)src_slice, src_stride, 0,
                  fmt.fmt.pix.height, frame->data, frame->linesize);
                  
        frame->pts = av_rescale_q(frame_count, (AVRational) {
            1, FRAME_RATE
        },
        codec_ctx->time_base);
        
        // Devolve o buffer mmapado ao driver assim que os dados já foram
        // copiados pelo sws_scale — não precisamos mais dele.
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            fprintf(stderr, "VIDIOC_QBUF: %s\n", strerror(errno));
            break;
        }
        
        // -------- Overlay da mira --------
        // AV_BUFFERSRC_FLAG_KEEP_REF garante que 'frame' continue válido e
        // reaproveitável na próxima iteração, mesmo depois de entregue ao grafo.
        if (av_buffersrc_add_frame_flags(filt.buffersrc_ctx, frame,
                                         AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            fprintf(stderr, "Erro ao enviar frame ao filtro\n");
            break;
        }
        
        while (1) {
            int fret = av_buffersink_get_frame(filt.buffersink_ctx, filt_frame);
            
            if (fret == AVERROR(EAGAIN) || fret == AVERROR_EOF) {
                break;
            }
            
            if (fret < 0) {
                fprintf(stderr, "Erro ao receber frame filtrado\n");
                goto cleanup;
            }
            
            // -------- Encode --------
            if (avcodec_send_frame(codec_ctx, filt_frame) < 0) {
                fprintf(stderr, "Could not send frame\n");
                av_frame_unref(filt_frame);
                goto cleanup;
            }
            
            av_frame_unref(filt_frame);
            
            while (1) {
                ret = avcodec_receive_packet(codec_ctx, pkt);
                
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                
                if (ret < 0) {
                    fprintf(stderr, "Could not encode frame\n");
                    goto cleanup;
                }
                
                pkt->stream_index = stream->index;
                av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
                
                if (av_interleaved_write_frame(ctx_out, pkt) < 0) {
                    fprintf(stderr, "Could not write packet\n");
                    av_packet_unref(pkt);
                    goto cleanup;
                }
                
                av_packet_unref(pkt);
            }
            
            frame_count++;
        }
        
        if ((av_gettime_relative() - start_time) / 1000000 >= DURATION) {
            break;
        }
    }
    
    // =================================================================
    // 9. Flush do encoder
    // =================================================================
    avcodec_send_frame(codec_ctx, NULL);
    
    while (1) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        
        if (ret < 0) {
            break;
        }
        
        pkt->stream_index = stream->index;
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        av_interleaved_write_frame(ctx_out, pkt);
        av_packet_unref(pkt);
    }
    
    av_write_trailer(ctx_out);
    fprintf(stderr, "OK: %d frames gravados em %s\n", frame_count,
            OUTPUT_FILENAME);
    ret = 0;
    
cleanup:

    if (fd >= 0) {
        enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &t);
        
        for (unsigned i = 0; i < NUM_BUFFERS; i++) {
            if (buffers[i].start && buffers[i].start != MAP_FAILED) {
                munmap(buffers[i].start, buffers[i].length);
            }
        }
        
        close(fd);
    }
    
    if (filt.graph) {
        avfilter_graph_free(&filt.graph);
    }
    
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    
    if (ctx_out) {
        if (ctx_out->pb) {
            avio_closep(&ctx_out->pb);
        }
        
        avformat_free_context(ctx_out);
    }
    
    return ret;
}
