Para gravar 10 segundos de vídeo no formato mp4 h264 usando libffmpeg e capturando o vídeo da webcam (/dev/video0), você pode usar o seguinte comando na linha de comando do seu sistema operacional:

ffmpeg -f v4l2 -i /dev/video0 -t 10 -c:v libx264 -preset ultrafast -c:a aac output.mp4


Gravar vídeo com libffmpeg

Model: Default (GPT-3.5)


Usando libffmpeg grave 10 segundos de vídeo no formato mp4 h264 pegando o /dev/video0

Para gravar 10 segundos de vídeo no formato mp4 h264 usando libffmpeg e capturando o vídeo da webcam (/dev/video0), você pode usar o seguinte comando na linha de comando do seu sistema operacional:

r
Copy code
ffmpeg -f v4l2 -i /dev/video0 -t 10 -c:v libx264 -preset ultrafast -c:a aac output.mp4
Aqui está o que cada argumento faz:

-f v4l2: define o formato de entrada como "Video4Linux2", que é o formato de entrada usado por muitas webcams no Linux.

-i /dev/video0: especifica a fonte de entrada de vídeo como a webcam conectada ao dispositivo /dev/video0.

-t 10: define a duração da gravação para 10 segundos.

-c:v libx264: define o codec de vídeo como libx264, que é um codec de vídeo popular para compressão de vídeo H.264.

-preset ultrafast: define a configuração de velocidade do codec de vídeo como ultrafast, que é uma das configurações mais rápidas disponíveis.

-c:a aac: define o codec de áudio como aac, que é um codec de áudio popular para compressão de áudio.

output.mp4: especifica o nome do arquivo de saída e o formato do arquivo de saída como mp4. O arquivo de saída será salvo no diretório atual.

Certifique-se de ter o libffmpeg instalado no seu sistema antes de executar o comando acima. Além disso, você pode precisar ajustar o nome do dispositivo de vídeo ou o codec de vídeo, dependendo do seu sistema operacional e da webcam que está usando.


Para gravar 10 segundos de vídeo no formato mp4 h264 usando a linguagem C e capturando o vídeo da webcam (/dev/video0) sem áudio, você pode usar a biblioteca libavformat e libavcodec da ffmpeg. Aqui está um exemplo de código:

