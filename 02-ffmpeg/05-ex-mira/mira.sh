#!/usr/bin/env bash
# Gera mira.png: retícula estilo HUD futurista, fundo transparente.
#
# Uso:
#   ./gerar_mira.sh                 # gera mira.png, 300x300
#   ./gerar_mira.sh 400             # gera mira.png, 400x400
#   ./gerar_mira.sh 400 minha.png   # gera minha.png, 400x400
set -euo pipefail

SIZE=${1:-300}
OUT=${2:-mira.png}

# Cor principal (verde-ciano estilo HUD) e uma variante mais fraca para o
# círculo interno. ImageMagick aceita alfa no próprio hex: #RRGGBBAA.
COR="#00ffaa"
COR_FRACA="#00ffaa66"

# ImageMagick 7 usa o binário 'magick'; a instalação via apt no Ubuntu/Debian
# ainda costuma trazer só 'convert' (IM6). Detecta qual está disponível.
CMD=magick
command -v magick >/dev/null 2>&1 || CMD=convert

if ! command -v "$CMD" >/dev/null 2>&1; then
    echo "ImageMagick não encontrado. Instale com: sudo apt install imagemagick" >&2
    exit 1
fi

CX=$(( SIZE / 2 ))
CY=$(( SIZE / 2 ))

R1=$(( SIZE * 30 / 100 ))    # raio do círculo externo
R2=$(( SIZE * 18 / 100 ))    # raio do círculo interno (mais fino, mais fraco)
GAP=$(( SIZE * 6  / 100 ))   # folga sem linha no centro da cruz
ARM=$(( SIZE * 46 / 100 ))   # até onde os braços da cruz se estendem
TICK=$(( SIZE * 10 / 100 ))  # tamanho das marcas de canto (formato de mira de rifle)
MARG=$(( SIZE * 7  / 100 ))  # distância das marcas de canto até a borda da imagem
SW=$(( SIZE / 100 + 2 ))     # espessura das linhas, escala com o tamanho

"$CMD" -size "${SIZE}x${SIZE}" xc:none \
  -fill none \
  -strokewidth "$SW" -stroke "$COR" \
    -draw "circle $CX,$CY $CX,$((CY - R1))" \
  -strokewidth 1 -stroke "$COR_FRACA" \
    -draw "circle $CX,$CY $CX,$((CY - R2))" \
  -strokewidth "$SW" -stroke "$COR" \
    -draw "line $((CX - ARM)),$CY $((CX - GAP)),$CY" \
    -draw "line $((CX + GAP)),$CY $((CX + ARM)),$CY" \
    -draw "line $CX,$((CY - ARM)) $CX,$((CY - GAP))" \
    -draw "line $CX,$((CY + GAP)) $CX,$((CY + ARM))" \
  -draw "line $MARG,$MARG $((MARG + TICK)),$MARG" \
  -draw "line $MARG,$MARG $MARG,$((MARG + TICK))" \
  -draw "line $((SIZE - MARG)),$MARG $((SIZE - MARG - TICK)),$MARG" \
  -draw "line $((SIZE - MARG)),$MARG $((SIZE - MARG)),$((MARG + TICK))" \
  -draw "line $MARG,$((SIZE - MARG)) $((MARG + TICK)),$((SIZE - MARG))" \
  -draw "line $MARG,$((SIZE - MARG)) $MARG,$((SIZE - MARG - TICK))" \
  -draw "line $((SIZE - MARG)),$((SIZE - MARG)) $((SIZE - MARG - TICK)),$((SIZE - MARG))" \
  -draw "line $((SIZE - MARG)),$((SIZE - MARG)) $((SIZE - MARG)),$((SIZE - MARG - TICK))" \
  -fill "$COR" -stroke none \
    -draw "circle $CX,$CY $((CX + 2)),$CY" \
  "$OUT"

echo "Gerado: $OUT (${SIZE}x${SIZE})"
