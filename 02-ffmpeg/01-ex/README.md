**`extract_frames.c`** extrai amostras de frames de um vídeo e salva cada uma como imagem `.ppm`, com um retângulo decorativo desenhado por cima.

**Uso:** `./extract_frames video.mp4`

**Passo a passo:**

1. Abre o vídeo e encontra o primeiro stream de vídeo dentro dele.
2. Abre o decodificador correspondente ao codec desse stream (H.264, etc).
3. Calcula quantos frames o vídeo tem no total (usa `nb_frames` do container, ou estima por `duração × fps` se o container não informar isso).
4. Divide esse total por `NUMBER_OF_FRAMES` (5) para descobrir de quantos em quantos frames deve capturar uma amostra — por exemplo, num vídeo de 500 frames, captura 1 a cada 100.
5. Percorre o vídeo frame a frame decodificando. Sempre que o índice do frame bate com o intervalo calculado:
   - Converte o frame decodificado (formato nativo do codec, geralmente YUV) para RGB24 via `sws_scale`.
   - Copia esses dados RGB para uma estrutura `ImagemRGB` própria, guardada em uma lista em memória.
6. Depois de ler o vídeo inteiro, para cada imagem guardada:
   - Desenha um retângulo escurecido/colorido no meio da imagem (`draw_rectangle` — pinta metade de cima puxando pro verde, metade de baixo puxando pro vermelho, deixando uma faixa sem pintar no centro).
   - Salva como `imagem_000.ppm`, `imagem_001.ppm`, etc.

**Sobre o "pool de frames":** o `frame_pool`/`frames.h` é um controle próprio de IDs (0 a 4) usando uma lista encadeada — funciona como um limitador que garante no máximo `NUMBER_OF_FRAMES` (5) imagens guardadas em memória ao mesmo tempo, mesmo que o cálculo de intervalo selecione mais que isso por algum motivo. Cada imagem recebe um ID desse pool e, ao ser salva no final, o ID volta pro pool (embora nesse fluxo específico o pool nunca seja reaproveitado durante a execução, já que tudo é salvo só no final).

**Resultado prático:** rodando com um vídeo, você fica com até 5 arquivos `.ppm` (`imagem_000.ppm` a `imagem_004.ppm`), cada um sendo um frame do vídeo espaçado ao longo da duração total, com uma faixa colorida sobreposta no centro da imagem.
