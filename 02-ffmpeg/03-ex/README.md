Vou criar um programa em C que usa apenas a biblioteca FFmpeg
para extrair frames equidistantes de um vídeo e salvá-los
como PNG.Agora vou criar um Makefile para facilitar a
compilação:Criei um programa completo em C que usa apenas
a biblioteca FFmpeg para extrair frames equidistantes de
um vídeo e salvá-los como PNG. Aqui estão as principais
características:

## Funcionalidades:
- **Extração equidistante**: Calcula automaticamente os intervalos para extrair o número desejado de frames
- **Conversão para PNG**: Usa o encoder PNG do FFmpeg para salvar os frames
- **Conversão de formato**: Converte automaticamente diferentes formatos de pixel para RGB24
- **Nomenclatura sequencial**: Salva os frames como `frame_001.png`, `frame_002.png`, etc.

## Como usar:

1. **Instalar dependências** (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install libavformat-dev libavcodec-dev libswscale-dev libavutil-dev
```

2. **Compilar**:
```bash
make
```

3. **Executar**:
```bash
./frame_extractor video.mp4 10
```

Isso extrairá 10 frames equidistantes do arquivo `video.mp4`
e os salvará como PNG.

## Exemplo de uso:
```bash
# Extrair 5 frames de um vídeo
./frame_extractor meu_video.avi 5

# Extrair 20 frames de um vídeo MP4
./frame_extractor filme.mp4 20
```

O programa usa **apenas FFmpeg** como solicitado, aproveitando suas bibliotecas para:
- `libavformat`: Leitura do arquivo de vídeo
- `libavcodec`: Decodificação do vídeo e codificação PNG
- `libswscale`: Conversão entre formatos de pixel
- `libavutil`: Utilitários gerais

Os frames são distribuídos uniformemente ao longo da
duração total do vídeo, garantindo uma amostragem
representativa do conteúdo.
