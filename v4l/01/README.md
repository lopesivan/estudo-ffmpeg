Este código abre o dispositivo de vídeo "/dev/video0", define o
formato de vídeo para 640x480 pixels com taxa de quadros de 30
fps e aloca 4 buffers de memória para a captura de vídeo. Em
seguida, inicia a transmissão de vídeo, captura frames da câmera
até atingir 10 segundos de duração e interrompe a transmissão de
vídeo.

Note que este código é apenas uma base para a captura de vídeo
usando C. Dependendo do tipo de câmera e do sistema operacional

instala lib
```{bash}
sudo apt-get install libv4l-dev
```

Testando com ffmpeg para verificar se dispositivo está livre.

```{bash}
ffmpeg -f v4l2 -input_format mjpeg -video_size 640x480 -i /dev/video0 -c:v libx264 -crf 23 -preset ultrafast video.mp4
```

