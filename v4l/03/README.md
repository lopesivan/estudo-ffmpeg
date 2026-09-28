
Este exemplo captura vídeo em resolução de 640x480 pixels no formato YUYV e salva o vídeo em um arquivo chamado `output_video.raw`. O vídeo capturado é salvo no formato YUYV bruto, portanto, você precisa converter o arquivo para um formato mais comum, como MP4, usando uma ferramenta como o FFmpeg.

Para compilar o código, você pode usar o seguinte comando:

```bash
gcc video_capture.c -o video_capture
```

