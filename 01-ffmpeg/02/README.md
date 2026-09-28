Para gravar 10 segundos de vídeo utilizando a biblioteca FFmpeg em C, podemos utilizar a mesma estrutura de código usada para decodificar um arquivo de vídeo, mas com algumas mudanças. Primeiro, precisamos abrir o arquivo de saída e configurar o codificador de vídeo para o formato desejado. Em seguida, vamos escrever 10 segundos de quadros de vídeo para o arquivo de saída. O código ficaria mais ou menos assim:


./main ./video.mp4 output.mp4
