ffmpeg -f v4l2 -input_format yuyv422 -framerate 30 -video_size 640x480 -i /dev/video0 -t 10 -c:v libx264 -preset ultrafast output.mp4


Este comando usa a biblioteca libffmpeg para capturar o vídeo da webcam (/dev/video0) e gravar 10 segundos de vídeo sem áudio no formato mp4 h264 em um arquivo de saída. Ele usa o codec libx264 para codificar o vídeo e o preset ultrafast para maximizar a velocidade de codificação. Você pode ajustar os parâmetros de entrada para alterar a resolução, o formato e a taxa de quadros do vídeo capturado.
