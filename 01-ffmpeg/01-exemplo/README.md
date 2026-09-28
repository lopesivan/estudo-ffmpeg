Você pode usar a biblioteca libavformat do FFMPEG para obter um quadro específico de um arquivo de vídeo em C. Aqui está um exemplo de código que usa a biblioteca FFMPEG para obter o quadro em um determinado tempo (em segundos) de um arquivo de vídeo:


 I  $ pkg-config  --list-all| grep libavformat
libavformat                                 libavformat - FFmpeg container format library
  main ?:6  /workspace/estudo-cansi/ffmpeg/01
 I  $ pkg-config  --list-all| grep libavutil
libavutil                                   libavutil - FFmpeg utility library
  main ?:6  /workspace/estudo-cansi/ffmpeg/01
 I  $ pkg-config  --list-all| grep libswscale
libswscale                                  libswscale - FFmpeg image rescaling library




Este código lê um arquivo de vídeo usando a biblioteca FFmpeg, e procura por um quadro no tempo especificado. Quando o quadro é encontrado, ele é escalado para o formato RGB24 e salvo em um arquivo PPM. O código utiliza as bibliotecas libavformat, libavcodec, libavutil e libswscale do FFmpeg.
