#include "ffmpeg_job.hpp"

int main()
{
    FFmpegJob job;
    job.input         = "file_example_MP3_5MG.mp3";
    job.output        = "file_example_MP3_5MG.ogg";
    job.audioCodec    = "libvorbis";
    job.audioBitrateK = 192;

    UnixCommand cmd = make_ffmpeg_command (job);

    return cmd.run();
}
