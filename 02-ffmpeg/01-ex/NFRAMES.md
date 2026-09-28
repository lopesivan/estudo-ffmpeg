
```{bash}
$ ffprobe -count_frames -show_entries stream=nb_frames -v quiet -of csv=print_section=0 arquivo.mp4
1149
1982

$ ffprobe -count_frames -show_entries stream=nb_frames -v quiet   arquivo.mp4
[STREAM]
nb_frames=1149
[/STREAM]
[STREAM]
nb_frames=1982
[/STREAM]
```



O comando `ffprobe` está mostrando dois valores porque
o arquivo MP4 contém dois fluxos (streams) diferentes -
normalmente um fluxo de vídeo e outro de áudio. Cada um
tem seu próprio número de "frames" (quadros para vídeo,
pacotes para áudio).

### Para obter **apenas os frames do vídeo**, você precisa especificar o stream de vídeo:

```bash
ffprobe -count_frames -select_streams v:0 -show_entries
stream=nb_frames -v quiet -of csv=print_section=0 arquivo.mp4
```

### Explicação dos parâmetros adicionados:
- `-select_streams v:0`: Seleciona apenas o primeiro stream
de vídeo (índice 0).

### Se quiser simplificar ainda mais:
```bash
ffprobe -v error -count_frames -select_streams v:0
-show_entries stream=nb_frames -of default=nw=1:nk=1
arquivo.mp4
```

Isso retornará apenas o número de frames do vídeo (exemplo:
`1149`).

### Por que acontece?
Arquivos de vídeo geralmente contêm:
1. **Stream de vídeo** (quadros visuais)
2. **Stream de áudio** (pacotes de áudio, que o `ffprobe`
também conta como "frames" por motivos técnicos)

O `ffprobe` lista todos os streams por padrão, mas você
pode filtrar com `-select_streams`. 😊
