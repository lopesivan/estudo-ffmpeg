# Explicação do Programa

Este programa **captura vídeo da webcam em `/dev/video0` por 10 segundos, codifica em H.264 e salva como `output.mp4`**, usando duas bibliotecas diferentes:

- **V4L2** (Video4Linux2) — API do kernel Linux para falar diretamente com a câmera
- **FFmpeg** (libavcodec, libavformat, libswscale) — para codificar e empacotar o vídeo

Vou destrinchar por partes.

---

## 🎯 Visão geral do fluxo

```
/dev/video0 (YUYV)  →  DQBUF  →  sws_scale  →  YUV420P
                                                 ↓
                                          encoder H.264
                                                 ↓
                                          pacote H.264
                                                 ↓
                                            muxer MP4
                                                 ↓
                                          output.mp4
```

A cada frame: **pega da câmera → converte cor → codifica → grava no MP4**.

---

## 📦 Parte 1 — Abrir e configurar a câmera (V4L2)

### Abre o dispositivo

```c
fd = open(DEVICE_PATH, O_RDWR | O_NONBLOCK);
```

Abre `/dev/video0` como arquivo. Em Linux, dispositivos são acessados assim. O `O_NONBLOCK` faz com que leituras não travem — se não houver frame disponível, retorna `EAGAIN` em vez de bloquear.

### Pergunta o que a câmera suporta

```c
ioctl(fd, VIDIOC_QUERYCAP, &cap);
```

`ioctl` é como você "conversa" com o driver da câmera. `VIDIOC_QUERYCAP` pergunta: *"o que você sabe fazer?"*. A resposta (`cap`) diz se o dispositivo:
- Captura vídeo (`V4L2_CAP_VIDEO_CAPTURE`)
- Faz streaming com buffers mapeados (`V4L2_CAP_STREAMING`)

Se faltar algum, o programa desiste.

### Define o formato da imagem

```c
fmt.fmt.pix.width       = 640;
fmt.fmt.pix.height      = 480;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
ioctl(fd, VIDIOC_S_FMT, &fmt);
```

Pede para a câmera entregar frames **640×480 em YUYV** (YUV 4:2:2 empacotado, 2 bytes por pixel). O driver pode ajustar se não suportar — por isso o programa lê de volta `fmt.fmt.pix.width/height` depois.

### Define o framerate

```c
parm.parm.capture.timeperframe.numerator   = 1;
parm.parm.capture.timeperframe.denominator = FRAME_RATE;
ioctl(fd, VIDIOC_S_PARM, &parm);
```

Pede **30 fps** (`1/30` s por frame).

---

## 🔄 Parte 2 — Pipeline de buffers (o coração do V4L2)

Aqui está o que costuma confundir. **V4L2 não te dá um ponteiro para o frame da câmera**. Em vez disso, ele funciona como uma **fila circular de buffers compartilhados** entre driver e aplicação.

### Passo 1 — Pede buffers ao driver

```c
req.count  = 4;
req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_MMAP;
ioctl(fd, VIDIOC_REQBUFS, &req);
```

Diz: *"me dá 4 buffers para captura, mapeáveis em memória"*. O driver aloca 4 regiões internas.

### Passo 2 — Descobre o tamanho e mapeia cada um

```c
for (i = 0; i < req.count; i++) {
    buf.index = i;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);          // descobre offset e length
    buffers[i].length = buf.length;
    buffers[i].start  = mmap(NULL, buf.length, // mapeia no meu espaço
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, buf.m.offset);
}
```

- `VIDIOC_QUERYBUF` pergunta: *"onde está o buffer i e qual seu tamanho?"*
- `mmap` faz o **mesmo pedaço de memória física** aparecer no seu processo. Agora você pode ler `buffers[i].start[...]` como um array normal.

### Passo 3 — Enfileira os buffers vazios

```c
for (i = 0; i < req.count; i++) {
    buf.index = i;
    ioctl(fd, VIDIOC_QBUF, &buf);   // devolve buffer vazio para o driver
}
```

Diz ao driver: *"estes 4 buffers estão disponíveis; encha com frames quando tiver"*.

### Passo 4 — Liga o streaming

```c
ioctl(fd, VIDIOC_STREAMON, &type);
```

A câmera **começa a capturar** e preenche os buffers em sequência.

### Passo 5 — O ciclo do loop

Dentro do `while`:

```c
ioctl(fd, VIDIOC_DQBUF, &buf);   // pega um buffer CHEIO (com frame)
// ... processa os dados ...
ioctl(fd, VIDIOC_QBUF, &buf);    // devolve o buffer VAZIO
```

É um **ping-pong**: você pega um frame pronto (`DQBUF`), processa, e devolve o buffer para a câmera reutilizar (`QBUF`).

> 💡 Se você esquecer de devolver (`QBUF`), a câmera fica sem buffers e o vídeo congela. Por isso o `QBUF` está no fim do loop.

---

## 🎨 Parte 3 — Converter YUYV para YUV420P

O frame que sai da câmera está em **YUYV422** (4:2:2 packed), mas o encoder H.264 que escolhemos espera **YUV420P** (4:2:0 planar). São formatos diferentes.

```c
src_slice[0]  = (uint8_t*)buffers[buf.index].start;
src_stride[0] = fmt.fmt.pix.width * 2;   // YUYV = 2 bytes/pixel

sws_scale(sws_ctx,
          (const uint8_t* const*)src_slice,
          src_stride,
          0, fmt.fmt.pix.height,
          yuv_frame->data, yuv_frame->linesize);
```

`sws_scale` faz a conversão **pixel a pixel** e também redimensiona se necessário. Aqui não redimensiona (640×480 → 640×480), só converte cor.

O `sws_ctx` foi criado antes:

```c
sws_ctx = sws_getContext(
    fmt.fmt.pix.width, fmt.fmt.pix.height, AV_PIX_FMT_YUYV422,
    enc_ctx->width,    enc_ctx->height,    enc_ctx->pix_fmt,
    SWS_BILINEAR, NULL, NULL, NULL);
```

*"Converta de 640×480 YUYV422 para 640×480 YUV420P, com filtro bilinear"*.

### Por que YUV420P e não YUYV direto?

Formatos YUV:
- **YUYV422**: cada linha tem [Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...] — U e V compartilhados entre pares de pixels, **horizontalmente** apenas.
- **YUV420P**: três planos separados — `Y` completo, `U` e `V` com **metade da resolução** em ambas as direções.

O H.264 (e o x264) aceitam 4:4:4, 4:2:2 e 4:2:0, mas **4:2:0 é o mais comum** — melhor compressão com pouca perda visual. Escolhemos esse.

---

## 🎥 Parte 4 — Codificar com H.264

### Configuração do encoder

```c
enc_ctx->codec_id     = AV_CODEC_ID_H264;
enc_ctx->width        = 640;
enc_ctx->height       = 480;
enc_ctx->time_base    = (AVRational){1, 30};
enc_ctx->framerate    = (AVRational){30, 1};
enc_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
enc_ctx->bit_rate     = 1000000;      // 1 Mbps
enc_ctx->gop_size     = 12;           // keyframe a cada 12 frames
enc_ctx->max_b_frames = 0;            // sem frames B
enc_ctx->profile      = AV_PROFILE_H264_HIGH;
enc_ctx->level        = 40;
avcodec_open2(enc_ctx, encoder, NULL);
```

- **`time_base = 1/30`**: significa *"1 unidade de PTS = 1/30 segundo"*. Então `pts=0` é o instante 0, `pts=1` é 1/30 s, etc.
- **`gop_size = 12`**: a cada 12 frames força um **keyframe** (I-frame). Keyframes são pontos de referência para busca.
- **`bit_rate = 1 Mbps`**: taxa alvo de bits por segundo.
- **`profile = HIGH`**: um perfil do H.264 que aceita B-frames, CABAC, etc. (usamos `max_b_frames=0` então não usamos B-frames aqui).
- **`level = 40`**: nível 4.0 — suporta até 1080p30.

### O loop de codificação

A cada frame:

```c
yuv_frame->pts = av_rescale_q(frame_count, (AVRational){1, 30}, enc_ctx->time_base);
avcodec_send_frame(enc_ctx, yuv_frame);

while (1) {
    ret = avcodec_receive_packet(enc_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
    // ... envia pkt para o muxer
}
```

**API moderna do FFmpeg** (≥ 3.1): `send_frame` alimenta o encoder, `receive_packet` retira o resultado. O encoder pode **guardar frames** internamente (ex.: para análise de movimento) e devolver 0, 1 ou mais pacotes por frame enviado — daí o `while`.

O `pts` diz ao encoder **quando** esse frame acontece na linha do tempo.

---

## 📼 Parte 5 — Empacotar no MP4

```c
avformat_alloc_output_context2(&out_ctx, NULL, "mp4", OUTPUT_FILENAME);
out_stream = avformat_new_stream(out_ctx, NULL);
avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
out_stream->time_base = enc_ctx->time_base;
avio_open(&out_ctx->pb, OUTPUT_FILENAME, AVIO_FLAG_WRITE);
avformat_write_header(out_ctx, NULL);
```

Um **MP4 é um container**: ele guarda vários streams (vídeo, áudio, legendas) com metadados (índices, duração, sincronização). O muxer do FFmpeg faz isso.

- `avformat_alloc_output_context2` cria o contexto do muxer.
- `avformat_new_stream` cria um **stream** dentro do MP4.
- `avcodec_parameters_from_context` copia as configurações do encoder (largura, altura, codec) para o stream.
- `avio_open` abre o arquivo real.
- `avformat_write_header` escreve o **cabeçalho** do MP4 (metadados iniciais).

### Por que `av_packet_rescale_ts`?

O encoder trabalha com `time_base = 1/30`, o stream do MP4 usa `1/30` também, mas o FFmpeg exige conversão explícita porque diferentes streams podem ter bases diferentes:

```c
av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
```

### `av_interleaved_write_frame`

Escreve o pacote garantindo que os streams estejam **entrelaçados** (para arquivos com áudio + vídeo, ordenados por tempo). Para vídeo só, funciona como `av_write_frame`.

---

## 🏁 Parte 6 — Finalização

### Duração

```c
if ((av_gettime_relative() - start_time) / 1000000 >= DURATION) break;
```

`av_gettime_relative()` retorna microssegundos desde algum ponto. Quando passar de 10 s, para.

### Flush do encoder

```c
avcodec_send_frame(enc_ctx, NULL);
while (avcodec_receive_packet(enc_ctx, pkt) >= 0) { ... }
```

Enviar `NULL` diz: *"não tenho mais frames, me dê tudo que guardou"*. Sem isso, os últimos frames ficam presos no buffer interno do x264 e são perdidos.

### Escreve o trailer e fecha

```c
av_write_trailer(out_ctx);    // índice final do MP4
avio_closep(&out_ctx->pb);    // fecha arquivo
avformat_free_context(out_ctx);
```

O **trailer** contém o índice (moov atom), essencial para o MP4 ser reproduzível.

### Limpeza

```c
VIDIOC_STREAMOFF       // desliga câmera
munmap(...)            // desfaz mmap dos buffers
close(fd)              // fecha dispositivo
sws_freeContext
av_frame_free
av_packet_free
avcodec_free_context
```

Libera tudo que foi alocado.

---

## 🧠 Resumo conceitual

| Camada | O que faz | Biblioteca |
|--------|-----------|------------|
| **Driver** | Captura bruta da câmera | V4L2 (kernel) |
| **Buffer** | Fila circular de frames | V4L2 mmap |
| **Cor** | YUYV → YUV420P | libswscale |
| **Codificação** | YUV420P → H.264 | libavcodec (x264) |
| **Container** | H.264 → MP4 | libavformat |
| **Arquivo** | bytes no disco | libavio |

O programa é essencialmente um **pipeline**: cada estágio transforma os dados, e a complexidade está em **sincronizar** as camadas (buffer do driver, conversão de formato, fila do encoder, ordem do muxer).

---

## ⚠️ Observações sobre o código

1. **Timing**: a duração é medida com `av_gettime_relative()` (relógio de parede), não pelo número de frames. Se a câmera atrasar, o vídeo terá menos de 300 frames (10 s × 30 fps), mas a duração real de captura será 10 s.

2. **`VIDIOC_DQBUF` com `O_NONBLOCK`**: se não houver frame, retorna `EAGAIN` — o código faz `usleep(1000)` e tenta de novo. Isso evita busy-wait.

3. **Frame YUV420P reutilizado**: `av_frame_make_writable` garante que o buffer interno não está sendo usado em outro lugar antes de escrever. Sem isso, `av_frame_get_buffer` uma vez só seria perigoso se o encoder ainda estivesse lendo o frame anterior.

4. **`V4L2_PIX_FMT_YUYV` fixo**: nem toda câmera suporta YUYV. Câmeras modernas frequentemente entregam **MJPEG** ou **H.264** já comprimido. O código precisaria de um fallback (tentar MJPEG e decodificar) para ser universal.

5. **Sem áudio**: MP4 com só vídeo é válido, mas incomum em captura real.

6. **Sem tratamento de `av_frame_make_writable` no loop**: o código chama, mas o `if` está correto — se falhar, `break`.

---

## 🎓 O que este programa ensina

1. **Como falar direto com hardware no Linux** (V4L2 + ioctl)
2. **Como funciona gerenciamento de buffers em streaming** (mmap + fila circular)
3. **Como converter formatos de pixel** (sws_scale)
4. **Como usar a API moderna de encode do FFmpeg** (send/receive)
5. **Como empacotar vídeo em container** (muxer)
6. **Por que "flush" e "trailer" importam** (sem eles, perde frames ou arquivo corrompido)

Se quiser, posso aprofundar em qualquer uma dessas camadas — por exemplo, como o `sws_scale` faz a interpolação, ou como o muxer MP4 organiza os átomos (`ftyp`, `moov`, `mdat`).
