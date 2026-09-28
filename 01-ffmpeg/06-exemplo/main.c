#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>

#define OUTPUT_H264_FILENAME "output.h264"
#define OUTPUT_MP4_FILENAME "output.mp4"

int main (int argc, char **argv)
{
    int ret;
    AVFormatContext *pFormatCtx = NULL;
    AVInputFormat *pInputFmt = NULL;
    AVStream *pVideoStream = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVPacket packet;
    AVFrame *pFrame = NULL;
    int videoStreamIndex = -1;
    int frameCount = 0;
    int64_t start_time = 0;
    int64_t end_time = 0;
    int64_t duration = 0;
    int64_t target_duration = 10000000; // 10 seconds in microseconds
    int64_t last_pts = AV_NOPTS_VALUE;
    int64_t next_pts = 0;
    int64_t pts_diff = 0;
    FILE *pH264File = NULL;
    FILE *pMp4File = NULL;
    AVOutputFormat *pOutputFmtH264 = NULL;
    AVOutputFormat *pOutputFmtMP4 = NULL;
    AVFormatContext *pFormatCtxH264 = NULL;
    AVFormatContext *pFormatCtxMP4 = NULL;
    AVStream *pStreamH264 = NULL;
    AVStream *pStreamMP4 = NULL;
    AVCodecContext *pCodecCtxH264 = NULL;
    AVCodecContext *pCodecCtxMP4 = NULL;
    AVCodec *pCodecH264 = NULL;
    AVCodec *pCodecMP4 = NULL;
    AVDictionary *opts = NULL;

    // Register all formats and codecs
    av_register_all();

    // Open input file
    if ((ret = avformat_open_input (&pFormatCtx, "/dev/video0", pInputFmt,
                                    NULL)) < 0)
    {
        fprintf (stderr, "Could not open input file\n");
        goto end;
    }

    // Find the first video stream
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            pVideoStream = pFormatCtx->streams[i];
            videoStreamIndex = i;
            break;
        }
    }

    if (!pVideoStream)
    {
        fprintf (stderr, "Could not find video stream\n");
        goto end;
    }

    // Get the codec context for the video stream
    pCodecCtx = avcodec_alloc_context3 (NULL);
    if (!pCodecCtx)
    {
        fprintf (stderr, "Could not allocate codec context\n");
        goto end;
    }
    ret = avcodec_parameters_to_context (pCodecCtx, pVideoStream->codecpar);
    if (ret < 0)
    {
        fprintf (stderr, "Could not copy codec parameters to codec context\n");
        goto end;
    }

    // Find the codec for the video stream
    pCodec = avcodec_find_decoder (pCodecCtx->codec_id);
    if (!pCodec)
    {
        fprintf (stderr, "Could not find codec\n

    }

    // Open codec
    if ((ret = avcodec_open2 (pCodecCtx, pCodec, &opts)) < 0)
    {
        fprintf (stderr, "Could not open codec\n");
        goto end;
    }

    // Allocate frame buffer
    pFrame = av_frame_alloc();
    if (!pFrame)
    {
        fprintf (stderr, "Could not allocate frame buffer\n");
        goto end;
    }

    // Open output file for H.264
    pH264File = fopen (OUTPUT_H264_FILENAME, "wb");
    if (!pH264File)
    {
        fprintf (stderr, "Could not open output file %s\n", OUTPUT_H264_FILENAME);
        goto end;
    }

    // Initialize H.264 output format
    pOutputFmtH264 = av_guess_format ("h264", NULL, NULL);
    if (!pOutputFmtH264)
    {
        fprintf (stderr, "Could not guess H.264 output format\n");
        goto end;
    }

    // Create output format context for H.264
    ret = avformat_alloc_output_context2 (&pFormatCtxH264, pOutputFmtH264, NULL,
                                          NULL);
    if (ret < 0)
    {
        fprintf (stderr, "Could not create output format context for H.264\n");
        goto end;
    }

    // Create output stream for H.264
    pStreamH264 = avformat_new_stream (pFormatCtxH264, NULL);
    if (!pStreamH264)
    {
        fprintf (stderr, "Could not create output stream for H.264\n");
        goto end;
    }

    // Get codec context for H.264
    pCodecCtxH264 = avcodec_alloc_context3 (NULL);
    if (!pCodecCtxH264)
    {
        fprintf (stderr, "Could not allocate codec context for H.264\n");
        goto end;
    }
    pCodecH264 = avcodec_find_encoder (pOutputFmtH264->video_codec);
    if (!pCodecH264)
    {
        fprintf (stderr, "Could not find encoder for H.264\n");
        goto end;
    }
    pCodecCtxH264->codec_id = pOutputFmtH264->video_codec;
    pCodecCtxH264->width = pCodecCtx->width;
    pCodecCtxH264->height = pCodecCtx->height;
    pCodecCtxH264->time_base = pCodecCtx->time_base;
    pCodecCtxH264->pix_fmt = pCodecCtx->pix_fmt;
    pCodecCtxH264->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ((ret = avcodec_open2 (pCodecCtxH264, pCodecH264, NULL)) < 0)
    {
        fprintf (stderr, "Could not open codec for H.264\n");
        goto end;
    }

    // Copy codec parameters from input stream to output stream
    ret = avcodec_parameters_from_context (pStreamH264->codecpar, pCodecCtxH264);
    if (ret < 0)
    {
        fprintf (stderr,
                 "Could not copy codec parameters to output stream for H.264\n");
        goto end;
    }

    // Write header to output format context for H.264
    ret = avformat_write_header (pFormatCtxH264, NULL);
    if (ret < 0)
    {
        fprintf (stderr, "Could not write header to output format context for H.264\n");
        goto end;
    }

    // Open output file for MP4
    pMp4File = fopen (OUTPUT_MP4_FILENAME, "wb");
    if (!pMp4File)
    {
        fprintf (stderr, "Could not open output file %s\n", OUTPUT_MP4_FILENAME);


        goto end;
    }

    // Initialize MP4 output format
    pOutputFmtMP4 = av_guess_format ("mp4", NULL, NULL);
    if (!pOutputFmtMP4)
    {
        fprintf (stderr, "Could not guess MP4 output format\n");
        goto end;
    }

    // Create output format context for MP4
    ret = avformat_alloc_output_context2 (&pFormatCtxMP4, pOutputFmtMP4, NULL,
                                          NULL);
    if (ret < 0)
    {
        fprintf (stderr, "Could not create output format context for MP4\n");
        goto end;
    }

    // Create output stream for MP4
    pStreamMP4 = avformat_new_stream (pFormatCtxMP4, NULL);
    if (!pStreamMP4)
    {
        fprintf (stderr, "Could not create output stream for MP4\n");
        goto end;
    }

    // Get codec context for MP4
    pCodecCtxMP4 = avcodec_alloc_context3 (NULL);
    if (!pCodecCtxMP4)
    {
        fprintf (stderr, "Could not allocate codec context for MP4\n");
        goto end;
    }
    pCodecMP4 = avcodec_find_encoder (pOutputFmtMP4->video_codec);
    if (!pCodecMP4)
    {
        fprintf (stderr, "Could not find encoder for MP4\n");
        goto end;
    }
    pCodecCtxMP4->codec_id = pOutputFmtMP4->video_codec;
    pCodecCtxMP4->width = pCodecCtx->width;
    pCodecCtxMP4->height = pCodecCtx->height;
    pCodecCtxMP4->time_base = pCodecCtx->time_base;
    pCodecCtxMP4->pix_fmt = pCodecCtx->pix_fmt;
    if ((ret = avcodec_open2 (pCodecCtxMP4, pCodecMP4, NULL)) < 0)
    {
        fprintf (stderr, "Could not open codec for MP4\n");
        goto end;
    }

    // Copy codec parameters from input stream to output stream
    ret = avcodec_parameters_from_context (pStreamMP4->codecpar, pCodecCtxMP4);
    if (ret < 0)
    {
        fprintf (stderr, "Could not copy codec parameters to output stream for MP4\n");
        goto end;
    }

    // Write header to output format context for MP4
    ret = avformat_write_header (pFormatCtxMP4, NULL);
    if (ret < 0)
    {
        fprintf (stderr, "Could not write header to output format context for MP4\n");
        goto end;
    }

    // Read frames and write to output files
    while (av_read_frame (pFormatCtx, &packet) >= 0)
    {
        if (packet.stream_index == videoStreamIndex)
        {
            // Decode video frame
            ret = avcodec_send_packet (pCodecCtx, &packet);
            if (ret < 0)
            {
                fprintf (stderr, "Error sending packet to decoder\n");
                goto end;
            }
            while (ret >= 0)
            {
                ret = avcodec_receive_frame (pCodecCtx, pFrame);
                if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf (stderr, "Error receiving frame from decoder\n");
                    goto end;
                }

                // Calculate start time
                if (start_time == 0)
                {
                    start_time = av_gettime();
                }

                // Calculate end time and duration
                end_time = av_gettime();
                duration = end_time - start_time;

                // Write frame to H.264 file
                pFrame
                ->pts = next_pts;
                next_pts += av_rescale_q (1, pCodecCtx->time_base, pStreamH264->time_base);
                ret = avcodec_send_frame (pCodecCtxH264, pFrame);
                if (ret < 0)
                {
                    fprintf (stderr, "Error sending frame to H.264 encoder\n");
                    goto end;
                }
                while (ret >= 0)
                {
                    ret = avcodec_receive_packet (pCodecCtxH264, &packet);
                    if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
                    {
                        break;
                    }
                    else if (ret < 0)
                    {
                        fprintf (stderr, "Error receiving packet from H.264 encoder\n");
                        goto end;
                    }

                    // Write packet to H.264 file
                    av_packet_rescale_ts (&packet, pCodecCtxH264->time_base,
                                          pStreamH264->time_base);
                    packet.stream_index = pStreamH264->index;
                    av_write_frame (pFormatCtxH264, &packet);

                    // Write packet to MP4 file
                    av_packet_rescale_ts (&packet, pCodecCtxH264->time_base, pStreamMP4->time_base);
                    packet.stream_index = pStreamMP4->index;
                    av_write_frame (pFormatCtxMP4, &packet);

                    // Increment frame count
                    frameCount++;

                    // Calculate next PTS
                    if (last_pts != AV_NOPTS_VALUE)
                    {
                        pts_diff = packet.pts - last_pts;
                        if (pts_diff > 0)
                        {
                            next_pts += pts_diff;
                        }
                        else
                        {
                            next_pts += av_rescale_q (1, pStreamH264->time_base, AV_TIME_BASE_Q);
                        }
                    }
                    last_pts = packet.pts;

                    // Check if we've reached the target duration
                    if (duration >= target_duration)
                    {
                        goto end;
                    }

                    av_packet_unref (&packet);
                }
            }
        }
        av_packet_unref (&packet);
    }

    // Write trailer to output format context for H.264
    av_write_trailer (pFormatCtxH264);

    // Write trailer to output format context for MP4
    av_write_trailer (pFormatCtxMP4);

    printf ("Wrote %d frames to %s and %s\n", frameCount, OUTPUT_H264_FILENAME,
            OUTPUT_MP4_FILENAME);

end:
    // Free resources
    if (pCodecCtxH264)
    {
        avcodec_free_context (&pCodecCtxH264);
    }
    if (pCodecCtxMP4)
    {
        avcodec_free_context (&pCodecCtxMP4);
    }
    if (pCodecCtx)
    {
        avcodec_free_context (&pCodecCtx);
    }
    if (pFrame)
    {
        av_frame_free (&pFrame);
    }
    if (pFormatCtx)
    {
        avformat_close_input (&pFormatCtx);
    }
    if (pH264File)
    {
        fclose (pH264File);
    }
    if (pMp4File)
    {
        fclose (pMp4File);
    }
    if (pFormatCtxH264)
    {
        avformat_free_context (pFormatCtxH264);
    }
    if (pFormatCtxMP4)
    {
        avformat_free_context (pFormatCtxMP4);
    }

    return ret < 0;

}

//
/* Este código usa a biblioteca libffmpeg para ler do dispositivo /dev/video0 e gravar 10 segundos de vídeo sem áudio em dois arquivos de saída: output.h264 e output.mp4. O vídeo é codificado em H.264 usando o codec x264 e, em seguida, os pac */
