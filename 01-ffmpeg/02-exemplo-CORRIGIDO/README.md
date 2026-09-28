Trocar o critério de parada de "tempo de execução do programa" para "tempo de vídeo decorrido", usando o `pts` do pacote convertido pela `time_base` do stream:**O que mudou:**

- Removi `av_gettime_relative()` como critério de parada — não precisa mais de `libavutil/time.h` para isso (mas mantenha o include, não atrapalha).
- `startPts` guarda o `pts` do primeiro pacote de vídeo lido.
- A cada pacote de vídeo, calculo `elapsedSeconds = (pts atual - pts inicial) * time_base`, usando `av_q2d` para converter o `AVRational` da `time_base` em segundos por unidade de `pts`.
- Esse cálculo é feito **antes** do `av_packet_rescale_ts`, porque depois dele o `pts` já estaria na `time_base` de saída, misturando as duas escalas.
- Quando `elapsedSeconds >= 10.0`, para o laço sem escrever aquele pacote.

Se aplicar no seu `main.c`:

```bash
sed -i 's/int64_t endTime = av_gettime_relative.*$//' main.c
```

— mas como a mudança é maior que uma linha, é mais seguro pegar o `main.c` gerado no arquivo acima e substituir o seu por completo.
