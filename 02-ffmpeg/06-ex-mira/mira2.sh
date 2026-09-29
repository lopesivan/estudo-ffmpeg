#!/usr/bin/env bash
# gerar_mira.sh <estilo> [tamanho] [saida]
set -euo pipefail

ESTILO=${1:-classica}
SIZE=${2:-300}
OUT=${3:-mira-${ESTILO}.png}

CMD=magick
command -v magick >/dev/null 2>&1 || CMD=convert

CX=$(( SIZE / 2 ))
CY=$(( SIZE / 2 ))
SW=$(( SIZE / 100 + 2 ))

# Cores por estilo
case "$ESTILO" in
    classica)  COR="#00ffaa"; COR_FRACA="#00ffaa66" ;;
    sniper)    COR="#ffffff"; COR_FRACA="#ffffff44" ;;
    radar)     COR="#00ff00"; COR_FRACA="#00ff0044" ;;
    predador)  COR="#ff4400"; COR_FRACA="#ff440066" ;;
    drone)     COR="#00ccff"; COR_FRACA="#00ccff44" ;;
    bussola)   COR="#ffcc00"; COR_FRACA="#ffcc0066" ;;
    scanner)   COR="#00ff00"; COR_FRACA="#00ff0044" ;;
    retro)     COR="#ffffff"; COR_FRACA="#ffffff44" ;;
    coruja)    COR="#ffaa00"; COR_FRACA="#ffaa0066" ;;
    xadrez)    COR="#cc00ff"; COR_FRACA="#cc00ff44" ;;
    *)         echo "Estilo desconhecido: $ESTILO" >&2; exit 1 ;;
esac

# Desenha conforme estilo (cada um chama $CMD com -draw ...)
case "$ESTILO" in
    classica)
        "$CMD" -size "${SIZE}x${SIZE}" xc:none \
          -fill none -strokewidth "$SW" -stroke "$COR" \
            -draw "circle $CX,$CY $CX,$((CY - SIZE*30/100))" \
          -draw "line $((CX - SIZE*46/100)),$CY $((CX - SIZE*6/100)),$CY" \
          -draw "line $((CX + SIZE*6/100)),$CY $((CX + SIZE*46/100)),$CY" \
          -draw "line $CX,$((CY - SIZE*46/100)) $CX,$((CY - SIZE*6/100))" \
          -draw "line $CX,$((CY + SIZE*6/100)) $CX,$((CY + SIZE*46/100))" \
          -fill "$COR" -stroke none \
            -draw "circle $CX,$CY $((CX + 2)),$CY" \
          "$OUT"
        ;;
    sniper)
        # cruz longa e fina + mil-dots
        "$CMD" -size "${SIZE}x${SIZE}" xc:none \
          -fill none -strokewidth 1 -stroke "$COR" \
            -draw "line $((CX - SIZE*45/100)),$CY $((CX - SIZE*3/100)),$CY" \
            -draw "line $((CX + SIZE*3/100)),$CY $((CX + SIZE*45/100)),$CY" \
            -draw "line $CX,$((CY - SIZE*45/100)) $CX,$((CY - SIZE*3/100))" \
            -draw "line $CX,$((CY + SIZE*3/100)) $CX,$((CY + SIZE*45/100))" \
          -strokewidth "$SW" \
            -draw "line $((CX - SIZE*45/100)),$((CY - SIZE*10/100)) $((CX + SIZE*45/100)),$((CY - SIZE*10/100))" \
            -draw "line $((CX - SIZE*45/100)),$((CY + SIZE*10/100)) $((CX + SIZE*45/100)),$((CY + SIZE*10/100))" \
          -fill "$COR" -stroke none \
            -draw "circle $CX,$CY $((CX + 1)),$CY" \
          "$OUT"
        ;;
    # ... outros estilos
esac

echo "Gerado: $OUT (${SIZE}x${SIZE}, estilo=$ESTILO)"

