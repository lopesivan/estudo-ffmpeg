// ffmpeg_job.hpp
#pragma once

// -----------------------------------------------------------------------------
// Este arquivo define:
//
// 1) Um "Job" (descrição declarativa da tarefa FFmpeg)
// 2) Um algoritmo genérico que transforma o Job em argv
// 3) Um adaptador que produz um UnixCommand executável
//
// Filosofia EOP:
//     dados  -> algoritmo -> sequência -> execução
//
// FFmpegJob NÃO executa nada.
// Ele apenas descreve a intenção da operação.
// -----------------------------------------------------------------------------

#include "unix_command.hpp"
#include <optional>

// -----------------------------------------------------------------------------
// Estrutura de dados pura.
// Representa uma tarefa de transcodificação/conversão FFmpeg.
//
// Não possui comportamento.
// Não possui método run().
// Não conhece shell.
// Não conhece POSIX.
//
// Apenas modela os parâmetros da operação.
// -----------------------------------------------------------------------------
struct FFmpegJob
{

    // -------------------------------------------------------------------------
    // Arquivo de entrada.
    //
    // Exemplo:
    //     "video.mp4"
    // -------------------------------------------------------------------------
    std::string input;

    // -------------------------------------------------------------------------
    // Arquivo de saída.
    //
    // Exemplo:
    //     "saida.avi"
    // -------------------------------------------------------------------------
    std::string output;

    // -------------------------------------------------------------------------
    // Codec de vídeo.
    //
    // Exemplo:
    //     "libx264"
    //     "libvpx"
    // -------------------------------------------------------------------------
    std::optional<std::string> videoCodec;

    // -------------------------------------------------------------------------
    // Codec de áudio.
    //
    // Exemplo:
    //     "aac"
    //     "libvorbis"
    // -------------------------------------------------------------------------
    std::optional<std::string> audioCodec;

    // -------------------------------------------------------------------------
    // Bitrate de vídeo em kbps.
    //
    // Exemplo:
    //     2500 -> "2500k"
    // -------------------------------------------------------------------------
    std::optional<int> videoBitrateK;

    // -------------------------------------------------------------------------
    // Bitrate de áudio em kbps.
    //
    // Exemplo:
    //     192 -> "192k"
    // -------------------------------------------------------------------------
    std::optional<int> audioBitrateK;

    // -------------------------------------------------------------------------
    // Framerate do vídeo.
    //
    // Exemplo:
    //     30
    //     60
    // -------------------------------------------------------------------------
    std::optional<int> videoFramerate;

    // -------------------------------------------------------------------------
    // Formato/container.
    //
    // Exemplo:
    //     "mp4"
    //     "matroska"
    // -------------------------------------------------------------------------
    std::optional<std::string> format;

    // -------------------------------------------------------------------------
    // Espaço para argumentos arbitrários.
    //
    // Permite expandir sem modificar a estrutura principal.
    //
    // Exemplo:
    //     "-preset"
    //     "slow"
    //
    //     "-movflags"
    //     "+faststart"
    // -------------------------------------------------------------------------
    std::vector<std::string> extraArgs;
};

// -----------------------------------------------------------------------------
// Algoritmo EOP.
//
// Transforma FFmpegJob -> sequência de argumentos.
//
// O algoritmo NÃO assume container específico.
// Ele escreve em qualquer OutputIterator compatível.
//
// Exemplo:
//
//     std::vector<std::string> args;
//     make_args(job, std::back_inserter(args));
//
// Ou:
//
//     std::ostream_iterator<std::string>
//
// Conceito importante:
//     Job -> argv
//
// -----------------------------------------------------------------------------
template <StringOutputIterator Out>
void make_args(const FFmpegJob& job, Out out)
{

    // -------------------------------------------------------------------------
    // "-y"
    //
    // Sobrescreve arquivo de saída automaticamente.
    // Evita prompt interativo do FFmpeg.
    // -------------------------------------------------------------------------
    *out++ = "-y";

    // -------------------------------------------------------------------------
    // Arquivo de entrada.
    //
    // FFmpeg usa:
    //
    //     -i arquivo
    // -------------------------------------------------------------------------
    *out++ = "-i";
    *out++ = job.input;

    // -------------------------------------------------------------------------
    // Codec de vídeo.
    //
    // Exemplo gerado:
    //
    //     -c:v libx264
    // -------------------------------------------------------------------------
    if(job.videoCodec)
    {
        *out++ = "-c:v";
        *out++ = *job.videoCodec;
    }

    // -------------------------------------------------------------------------
    // Bitrate de vídeo.
    //
    // Exemplo:
    //
    //     -b:v 2500k
    // -------------------------------------------------------------------------
    if(job.videoBitrateK)
    {
        *out++ = "-b:v";
        *out++ = std::to_string(*job.videoBitrateK) + "k";
    }

    // -------------------------------------------------------------------------
    // Framerate.
    //
    // Exemplo:
    //
    //     -r 60
    // -------------------------------------------------------------------------
    if(job.videoFramerate)
    {
        *out++ = "-r";
        *out++ = std::to_string(*job.videoFramerate);
    }

    // -------------------------------------------------------------------------
    // Codec de áudio.
    //
    // Exemplo:
    //
    //     -c:a aac
    // -------------------------------------------------------------------------
    if(job.audioCodec)
    {
        *out++ = "-c:a";
        *out++ = *job.audioCodec;
    }

    // -------------------------------------------------------------------------
    // Bitrate de áudio.
    //
    // Exemplo:
    //
    //     -b:a 192k
    // -------------------------------------------------------------------------
    if(job.audioBitrateK)
    {
        *out++ = "-b:a";
        *out++ = std::to_string(*job.audioBitrateK) + "k";
    }

    // -------------------------------------------------------------------------
    // Formato/container.
    //
    // Exemplo:
    //
    //     -f mp4
    // -------------------------------------------------------------------------
    if(job.format)
    {
        *out++ = "-f";
        *out++ = *job.format;
    }

    // -------------------------------------------------------------------------
    // Argumentos extras arbitrários.
    //
    // append_args é um algoritmo genérico reutilizável.
    //
    // Exemplo:
    //
    //     "-preset"
    //     "slow"
    // -------------------------------------------------------------------------
    append_args(out, job.extraArgs);

    // -------------------------------------------------------------------------
    // Arquivo final de saída.
    // -------------------------------------------------------------------------
    *out++ = job.output;
}

// -----------------------------------------------------------------------------
// Adaptador de conveniência.
//
// Converte:
//
//     FFmpegJob -> UnixCommand
//
// A ideia é separar:
//
//     descrição da tarefa
//     geração de argumentos
//     execução do processo
//
// Isso permite:
//
//     POSIX
//     Boost.Process
//     logging
//     dry-run
//     serialização
//
// sem alterar FFmpegJob.
// -----------------------------------------------------------------------------
inline UnixCommand make_ffmpeg_command(
    const FFmpegJob& job,

    // -------------------------------------------------------------------------
    // Permite trocar o binário:
    //
    //     ffmpeg
    //     avconv
    //     /usr/local/bin/ffmpeg
    // -------------------------------------------------------------------------
    const std::string& ffmpegBinary = "/usr/bin/ffmpeg")
{

    // -------------------------------------------------------------------------
    // make_unix_command:
    //
    // 1) chama make_args(job, ...)
    // 2) constrói argv
    // 3) cria UnixCommand
    //
    // Resultado:
    //
    //     UnixCommand executável
    // -------------------------------------------------------------------------
    return make_unix_command(ffmpegBinary, job);
}
