# 002 - erro de alinhamento



Você pode adicionar o campo `linesize` na struct `ImagemRGB` da seguinte forma:

```c
typedef struct
{
    int id;         /* Identificador lógico do frame */
    int largura;    /* Largura da imagem em pixels */
    int altura;     /* Altura da imagem em pixels */
    int linesize;   /* Número de bytes por linha (com padding, se houver) */
    uint8_t *dados; /* Ponteiro para os dados RGB (tamanho: linesize * altura) */
} ImagemRGB;
```

### Observações:

* O campo `linesize` é importante porque os dados de imagem podem ter padding no final de cada linha para atender requisitos de alinhamento.
* Ao copiar ou processar a imagem, use `linesize` em vez de assumir que cada linha tem exatamente `largura * 3` bytes.

Quer que eu mostre como preencher essa struct com dados do `AVFrame`?


Ótima pergunta — com a substituição por `av_image_alloc`, **você não precisa mais usar `av_image_fill_arrays`**, porque a função `av_image_alloc` **já preenche `pFrameRGB->data` e `pFrameRGB->linesize` automaticamente**.

Portanto, o valor de `linesize` continua disponível da mesma forma:

```c
int linesize = pFrameRGB->linesize[0];
```

Ou seja, mesmo com a nova versão:

```c
int numBytes = av_image_alloc(pFrameRGB->data, pFrameRGB->linesize,
                              largura_alinhada, altura, AV_PIX_FMT_RGB24, 1);

if (numBytes < 0)
{
    fprintf(stderr, "Erro ao alocar imagem RGB\n");
    exit(1);
}

int linesize = pFrameRGB->linesize[0]; // <-- Continua funcionando aqui
```

✅ **Resumo**: Nada muda na forma como você acessa o `linesize`. Ele continua vindo de `pFrameRGB->linesize[0]`, mesmo com o uso de `av_image_alloc`.

Quer que eu te mostre como usar esse `linesize` para copiar os dados corretamente ou salvar a imagem em disco?
