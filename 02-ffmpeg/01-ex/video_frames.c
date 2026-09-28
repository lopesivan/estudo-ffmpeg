#include <libavcodec/avcodec.h> /* Biblioteca para lidar com codecs de vídeo/áudio */
#include <libavformat/avformat.h> /* Biblioteca para manipular formatos de arquivo multimídia */
#include <libavutil/imgutils.h> /* Funções auxiliares para imagens */
#include <libswscale/swscale.h> /* Biblioteca para converter formatos de pixel e redimensionamento */

#include "frames.h" /* Biblioteca personalizada para gerenciar IDs de frame */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMBER_OF_FRAMES 5

/* Estrutura que representa uma imagem RGB mantida em memória */
typedef struct
{
    int id;         /* Identificador lógico do frame */
    int largura;    /* Largura da imagem em pixels */
    int altura;     /* Altura da imagem em pixels */
    uint8_t *dados; /* Ponteiro para os dados RGB (tamanho: largura * altura *
                       3) */
} ImagemRGB;

/* Inicializa o pool de frame_ids usando uma lista encadeada (list.c).
 * Isso simula uma "fila de recursos" disponíveis para controlar alocação.
 */
void init_frame_pool(List *pool, int total)
{
    /* Inicializa a lista e define 'free' como função
                              de liberação dos dados */
    list_init(pool, free);
    for (int i = total - 1; i >= 0; i--)
    {
        int *f = malloc(sizeof(int)); /* Aloca um inteiro na heap */
        *f = i;
        /* Insere no início da lista (ordem reversa) */
        list_ins_next(pool, NULL, f);
    }
}

/* Salva uma imagem RGB em formato PPM (imagem bruta sem compressão) no disco
 */
void salvar_imagem_em_arquivo(const ImagemRGB *img)
{
    char nome[64];
    snprintf(nome, sizeof(nome), "imagem_%03d.ppm",
             img->id); /* Gera nome do arquivo */

    printf("save: imagem_%03d.ppm\n", img->id); /* Imprime nome do arquivo */

    FILE *f = fopen(nome, "wb");
    fprintf(f, "P6\n%d %d\n255\n", img->largura,
            img->altura); /* Cabeçalho do formato PPM */

    /* Grava linha por linha os dados RGB */
    for (int y = 0; y < img->altura; y++)
    {
        fwrite(img->dados + y * img->largura * 3, 1, img->largura * 3, f);
    }
    fclose(f);
}

/* desenha um retangulo na imagem: */
void draw_rectangle(ImagemRGB *img)
{
    printf("pintando pixel da imagem_%03d.ppm\n", img->id);

    int w = img->largura;
    int h = img->altura;
    int linesize = w * 3;

    int kapa = 40;

    // Aloca matriz de linhas
    uint8_t **matrix = malloc(h * sizeof(uint8_t *));
    if (!matrix)
        return;

    matrix[0] = &img->dados[0];
    for (int i = 1; i < h; i++)
        matrix[i] = matrix[i - 1] + linesize;

    for (int i = 0; i < h / 2 - kapa; i++)
        for (int j = 0; j < linesize; j = j + 3)
        {
            matrix[i][j + 0] = matrix[i][j + 0] / 2 - 0;  // R
            matrix[i][j + 1] = matrix[i][j + 1] / 2 - 55; // G
            matrix[i][j + 2] = matrix[i][j + 2] / 2 - 0;  // B
        }

    for (int i = h / 2 + kapa; i < h; i++)
        for (int j = 0; j < linesize; j = j + 3)
        {
            matrix[i][j + 0] = matrix[i][j + 0] / 2 - 55; // R
            matrix[i][j + 1] = matrix[i][j + 1] / 2 - 0;  // G
            matrix[i][j + 2] = matrix[i][j + 2] / 2 - 0;  // B
        }
    free(matrix);
}

/* Função principal do programa */
int main(int argc, char *argv[])
{
    /* Verifica se o caminho do vídeo foi passado como argumento */
    if (argc < 2)
    {
        printf("Uso: %s arquivo.mp4\n", argv[0]);
        return -1;
    }

    AVFormatContext *pFormatCtx = NULL; /* Contexto do arquivo de mídia */
    AVCodecContext *pCodecCtx = NULL;   /* Contexto do codec de vídeo */
    AVFrame *pFrame = NULL,
            *pFrameRGB = NULL; /* Frames original e convertido para RGB */
    AVPacket packet;           /* Pacote de dados lido do vídeo */
    struct SwsContext *sws_ctx = NULL; /* Contexto de conversão de cor */

    /* Abre o arquivo de vídeo */
    avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    avformat_find_stream_info(pFormatCtx,
                              NULL); /* Lê as informações do arquivo */

    int videoStream = -1;
    /* Procura o índice do stream de vídeo no arquivo */
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codecpar->codec_type ==
            AVMEDIA_TYPE_VIDEO)
            videoStream = i;

    if (videoStream == -1)
        return -1; /* Se nenhum stream de vídeo for encontrado, encerra o
                      programa */

    /* Encontra o decodificador apropriado */
    const AVCodec *pCodec = avcodec_find_decoder(
        pFormatCtx->streams[videoStream]->codecpar->codec_id);

    /* Inicializa o contexto do codec e copia os parâmetros do stream */
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx,
                                  pFormatCtx->streams[videoStream]->codecpar);
    avcodec_open2(pCodecCtx, pCodec, NULL); /* Abre o codec */

    pFrame = av_frame_alloc();    /* Aloca estrutura para frame original */
    pFrameRGB = av_frame_alloc(); /* Aloca estrutura para frame convertido */

    int largura = pCodecCtx->width;
    int altura = pCodecCtx->height;

    printf("imagem: (%d, %d)\n", largura, altura);

    // arredonda largura para múltiplo de 16
    int largura_alinhada = (largura + 15) & ~15;

    int numBytes =
        av_image_alloc(pFrameRGB->data, pFrameRGB->linesize, largura_alinhada,
                       altura, AV_PIX_FMT_RGB24, 1);

    /* int numBytes = av_image_alloc(pFrameRGB->data, pFrameRGB->linesize, */
    /* largura, altura, AV_PIX_FMT_RGB24, 1); */

    if (numBytes < 0)
    {
        fprintf(stderr, "Erro ao alocar imagem RGB\n");
        exit(1);
    }

    /* Inicializa o contexto de conversão de cores (de YUV para RGB) */
    sws_ctx =
        sws_getContext(largura, altura, pCodecCtx->pix_fmt, largura, altura,
                       AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    List frame_pool;         /* Pool de frame_ids disponíveis */
    List imagens_em_memoria; /* Lista de imagens capturadas na memória */
    init_frame_pool(
        &frame_pool,
        NUMBER_OF_FRAMES); /* Cria pool com NUMBER_OF_FRAMES slots */
    list_init(&imagens_em_memoria, free); /* Inicializa lista para armazenar
                                             ponteiros de ImagemRGB */

    /* Estima o número total de frames do vídeo */
    int64_t total_frames = pFormatCtx->streams[videoStream]->nb_frames;
    if (total_frames == 0)
    {
        double duration = pFormatCtx->duration / (double)AV_TIME_BASE;
        AVRational framerate = pFormatCtx->streams[videoStream]->r_frame_rate;
        total_frames =
            (int64_t)(duration *
                      av_q2d(framerate)); /* fallback: duração * FPS */
    }

    int intervalo =
        total_frames / NUMBER_OF_FRAMES; /* Define espaçamento para capturar
                                            NUMBER_OF_FRAMES amostras */
    if (intervalo < 1)
        intervalo = 1; /* Garante intervalo mínimo válido */

    int frame_count = 0;
    /* Loop de leitura do vídeo frame a frame */
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        if (packet.stream_index == videoStream)
        {
            avcodec_send_packet(pCodecCtx, &packet);
            if (avcodec_receive_frame(pCodecCtx, pFrame) == 0)
            {
                /* Salva o frame somente se for múltiplo do intervalo */
                if (frame_count % intervalo == 0)
                {

                    int id = alloc_frame(
                        &frame_pool); /* Aloca um frame_id disponível */
                    if (id != -1)
                    {
                        printf("frame: %d\n", frame_count);

                        /* Converte frame original para RGB */
                        sws_scale(sws_ctx,
                                  (uint8_t const *const *)pFrame->data,
                                  pFrame->linesize, 0, altura,
                                  pFrameRGB->data, pFrameRGB->linesize);

                        /* Cria e preenche estrutura de imagem RGB */
                        ImagemRGB *img = malloc(sizeof(ImagemRGB));
                        img->id = id;
                        img->largura = largura;
                        img->altura = altura;
                        img->dados = malloc(largura * altura * 3);

                        printf("load[%d] (%d, %d)\n", img->id, img->largura,
                               img->altura);

                        /* Copia os dados RGB do frame convertido para o
                         * buffer da imagem */
                        for (int y = 0; y < altura; y++)
                        {
                            memcpy(img->dados + y * largura * 3,
                                   pFrameRGB->data[0] +
                                       y * pFrameRGB->linesize[0],
                                   largura * 3);
                        }

                        list_ins_next(&imagens_em_memoria, NULL,
                                      img); /* Insere na lista */
                    }
                }
                frame_count++;
            }
        }
        av_packet_unref(&packet); /* Libera recursos do pacote */
    }

    printf("%s\n", "--------------------------");

    /* Após o loop: salva cada imagem armazenada em memória */
    ListElmt *elmt = list_head(&imagens_em_memoria);
    while (elmt != NULL)
    {
        ImagemRGB *img = (ImagemRGB *)list_data(elmt);

        printf("memory id[%d] (%d, %d)\n", img->id, img->largura,
               img->altura);

        draw_rectangle(img);
        salvar_imagem_em_arquivo(img); /* Salva imagem .ppm */

        free(img->dados); /* Libera dados RGB */

        /* Devolve o frame_id ao pool */
        free_frame(&frame_pool, img->id);

        elmt = list_next(elmt);
    }

    /* Liberação geral dos recursos */
    list_destroy(&imagens_em_memoria);
    list_destroy(&frame_pool);

    av_freep(
        &pFrameRGB->data[0]); // 🔥 OBRIGATÓRIO quando se usa av_image_alloc
    av_frame_free(&pFrame);
    av_frame_free(&pFrameRGB);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    sws_freeContext(sws_ctx); /* Libera contexto de conversão */

    return 0;
}
