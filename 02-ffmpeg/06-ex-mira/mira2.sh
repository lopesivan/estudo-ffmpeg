#!/usr/bin/env bash

set -euo pipefail

# Uso:
#
#   ./gerar_mira.sh [estilo] [tamanho] [saida]
#
# Exemplos:
#
#   ./gerar_mira.sh
#   ./gerar_mira.sh classica 300
#   ./gerar_mira.sh sniper 512 sniper.png
#   ./gerar_mira.sh radar 512 radar.png
#

ESTILO="${1:-classica}"
SIZE="${2:-300}"
OUT="${3:-mira-${ESTILO}.png}"

# ------------------------------------------------------------------------------
# ImageMagick
# ------------------------------------------------------------------------------

if command -v magick >/dev/null 2>&1; then
    CMD=(magick)
elif command -v convert >/dev/null 2>&1; then
    CMD=(convert)
else
    echo "ImageMagick nao encontrado." >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# Validação
# ------------------------------------------------------------------------------

if ! [[ "$SIZE" =~ ^[0-9]+$ ]] || ((SIZE < 64)); then
    echo "Tamanho invalido: $SIZE" >&2
    echo "Use um inteiro maior ou igual a 64." >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# Geometria básica
# ------------------------------------------------------------------------------

CX=$((SIZE / 2))
CY=$((SIZE / 2))

SW=$((SIZE / 150))
((SW < 1)) && SW=1

SW2=$((SW * 2))

R10=$((SIZE * 10 / 100))
R15=$((SIZE * 15 / 100))
R20=$((SIZE * 20 / 100))
R25=$((SIZE * 25 / 100))
R30=$((SIZE * 30 / 100))
R35=$((SIZE * 35 / 100))
R40=$((SIZE * 40 / 100))
R45=$((SIZE * 45 / 100))

# ------------------------------------------------------------------------------
# Cores
# ------------------------------------------------------------------------------

case "$ESTILO" in
    classica)
        COR="#00ffaa"
        COR_FRACA="#00ffaa66"
        ;;

    sniper)
        COR="#ffffff"
        COR_FRACA="#ffffff55"
        ;;

    radar)
        COR="#00ff00"
        COR_FRACA="#00ff0055"
        ;;

    predador)
        COR="#ff4400"
        COR_FRACA="#ff440066"
        ;;

    drone)
        COR="#00ccff"
        COR_FRACA="#00ccff55"
        ;;

    bussola)
        COR="#ffcc00"
        COR_FRACA="#ffcc0066"
        ;;

    scanner)
        COR="#00ff00"
        COR_FRACA="#00ff0044"
        ;;

    retro)
        COR="#ffffff"
        COR_FRACA="#ffffff55"
        ;;

    coruja)
        COR="#ffaa00"
        COR_FRACA="#ffaa0066"
        ;;

    xadrez)
        COR="#cc00ff"
        COR_FRACA="#cc00ff55"
        ;;

    *)
        echo "Estilo desconhecido: $ESTILO" >&2
        echo "Estilos disponiveis:" >&2
        echo "  classica sniper radar predador drone" >&2
        echo "  bussola scanner retro coruja xadrez" >&2
        exit 1
        ;;
esac

# ------------------------------------------------------------------------------
# Clássica
# ------------------------------------------------------------------------------

case "$ESTILO" in

    classica)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "circle $CX,$CY $CX,$((CY - R30))" \
            -draw "line $((CX - R45)),$CY $((CX - R10)),$CY" \
            -draw "line $((CX + R10)),$CY $((CX + R45)),$CY" \
            -draw "line $CX,$((CY - R45)) $CX,$((CY - R10))" \
            -draw "line $CX,$((CY + R10)) $CX,$((CY + R45))" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Sniper
        # ------------------------------------------------------------------------------

    sniper)

        DOT=$((SIZE / 100))
        ((DOT < 1)) && DOT=1

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW" \
            -draw "circle $CX,$CY $CX,$((CY - R30))" \
            -draw "line $((CX - R45)),$CY $((CX - R10 / 2)),$CY" \
            -draw "line $((CX + R10 / 2)),$CY $((CX + R45)),$CY" \
            -draw "line $CX,$((CY - R45)) $CX,$((CY - R10 / 2))" \
            -draw "line $CX,$((CY + R10 / 2)) $CX,$((CY + R45))" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $((CX - R20)),$CY $((CX - R20 + DOT)),$CY" \
            -draw "circle $((CX + R20)),$CY $((CX + R20 + DOT)),$CY" \
            -draw "circle $CX,$((CY - R20)) $((CX + DOT)),$((CY - R20))" \
            -draw "circle $CX,$((CY + R20)) $((CX + DOT)),$((CY + R20))" \
            -draw "circle $CX,$CY $((CX + DOT)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Radar
        # ------------------------------------------------------------------------------

    radar)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR_FRACA" \
            -strokewidth "$SW" \
            -draw "circle $CX,$CY $CX,$((CY - R15))" \
            -draw "circle $CX,$CY $CX,$((CY - R30))" \
            -draw "circle $CX,$CY $CX,$((CY - R45))" \
            -draw "line $((CX - R45)),$CY $((CX + R45)),$CY" \
            -draw "line $CX,$((CY - R45)) $CX,$((CY + R45))" \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "line $CX,$CY $((CX + R35)),$((CY - R25))" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Predador
        # ------------------------------------------------------------------------------

    predador)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "polyline \
                $((CX - R30)),$((CY - R20)) \
                $((CX - R30)),$((CY - R30)) \
                $((CX - R20)),$((CY - R30))" \
            -draw "polyline \
                $((CX + R20)),$((CY - R30)) \
                $((CX + R30)),$((CY - R30)) \
                $((CX + R30)),$((CY - R20))" \
            -draw "polyline \
                $((CX - R30)),$((CY + R20)) \
                $((CX - R30)),$((CY + R30)) \
                $((CX - R20)),$((CY + R30))" \
            -draw "polyline \
                $((CX + R20)),$((CY + R30)) \
                $((CX + R30)),$((CY + R30)) \
                $((CX + R30)),$((CY + R20))" \
            -stroke "$COR_FRACA" \
            -strokewidth "$SW" \
            -draw "circle $CX,$CY $CX,$((CY - R10))" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Drone
        # ------------------------------------------------------------------------------

    drone)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "rectangle \
                $((CX - R30)),$((CY - R20)) \
                $((CX + R30)),$((CY + R20))" \
            -stroke "$COR_FRACA" \
            -strokewidth "$SW" \
            -draw "line $((CX - R40)),$CY $((CX - R10)),$CY" \
            -draw "line $((CX + R10)),$CY $((CX + R40)),$CY" \
            -draw "line $CX,$((CY - R35)) $CX,$((CY - R10))" \
            -draw "line $CX,$((CY + R10)) $CX,$((CY + R35))" \
            -stroke "$COR" \
            -draw "circle $CX,$CY $CX,$((CY - R10))" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Bússola
        # ------------------------------------------------------------------------------

    bussola)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "circle $CX,$CY $CX,$((CY - R40))" \
            -draw "line $CX,$((CY - R45)) $CX,$((CY - R15))" \
            -draw "line $CX,$((CY + R15)) $CX,$((CY + R45))" \
            -draw "line $((CX - R45)),$CY $((CX - R15)),$CY" \
            -draw "line $((CX + R15)),$CY $((CX + R45)),$CY" \
            -fill "$COR" \
            -stroke none \
            -draw "polygon \
                $CX,$((CY - R30)) \
                $((CX - R10 / 2)),$CY \
                $CX,$((CY - R10)) \
                $((CX + R10 / 2)),$CY" \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Scanner
        # ------------------------------------------------------------------------------

    scanner)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR_FRACA" \
            -strokewidth "$SW" \
            -draw "circle $CX,$CY $CX,$((CY - R40))" \
            -draw "circle $CX,$CY $CX,$((CY - R25))" \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "arc \
                $((CX - R40)),$((CY - R40)) \
                $((CX + R40)),$((CY + R40)) \
                200,340" \
            -draw "line $((CX - R35)),$CY $((CX - R10)),$CY" \
            -draw "line $((CX + R10)),$CY $((CX + R35)),$CY" \
            -fill "$COR" \
            -stroke none \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Retro
        # ------------------------------------------------------------------------------

    retro)

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "rectangle \
                $((CX - R25)),$((CY - R25)) \
                $((CX + R25)),$((CY + R25))" \
            -draw "line $((CX - R40)),$CY $((CX - R10)),$CY" \
            -draw "line $((CX + R10)),$CY $((CX + R40)),$CY" \
            -draw "line $CX,$((CY - R40)) $CX,$((CY - R10))" \
            -draw "line $CX,$((CY + R10)) $CX,$((CY + R40))" \
            -stroke "$COR_FRACA" \
            -strokewidth "$SW" \
            -draw "rectangle \
                $((CX - R10)),$((CY - R10)) \
                $((CX + R10)),$((CY + R10))" \
            -fill "$COR" \
            -stroke none \
            -draw "rectangle \
                $((CX - SW)),$((CY - SW)) \
                $((CX + SW)),$((CY + SW))" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Coruja
        # ------------------------------------------------------------------------------

    coruja)

        EYE_X=$((SIZE * 16 / 100))
        EYE_R=$((SIZE * 10 / 100))
        PUPIL=$((SIZE * 3 / 100))

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "circle \
                $((CX - EYE_X)),$CY \
                $((CX - EYE_X)),$((CY - EYE_R))" \
            -draw "circle \
                $((CX + EYE_X)),$CY \
                $((CX + EYE_X)),$((CY - EYE_R))" \
            -stroke "$COR_FRACA" \
            -draw "line $((CX - R40)),$CY $((CX - R30)),$CY" \
            -draw "line $((CX + R30)),$CY $((CX + R40)),$CY" \
            -fill "$COR" \
            -stroke none \
            -draw "circle \
                $((CX - EYE_X)),$CY \
                $((CX - EYE_X + PUPIL)),$CY" \
            -draw "circle \
                $((CX + EYE_X)),$CY \
                $((CX + EYE_X + PUPIL)),$CY" \
            -draw "polygon \
                $CX,$((CY + R10)) \
                $((CX - R10 / 2)),$CY \
                $((CX + R10 / 2)),$CY" \
            "$OUT"
        ;;

        # ------------------------------------------------------------------------------
        # Xadrez
        # ------------------------------------------------------------------------------

    xadrez)

        CELL=$((SIZE * 8 / 100))

        "${CMD[@]}" \
            -size "${SIZE}x${SIZE}" \
            xc:none \
            -fill none \
            -stroke "$COR" \
            -strokewidth "$SW2" \
            -draw "rectangle \
                $((CX - R30)),$((CY - R30)) \
                $((CX + R30)),$((CY + R30))" \
            -draw "line $((CX - R40)),$CY $((CX + R40)),$CY" \
            -draw "line $CX,$((CY - R40)) $CX,$((CY + R40))" \
            -fill "$COR_FRACA" \
            -stroke none \
            -draw "rectangle \
                $((CX - CELL)),$((CY - CELL)) \
                $CX,$CY" \
            -draw "rectangle \
                $CX,$CY \
                $((CX + CELL)),$((CY + CELL))" \
            -fill "$COR" \
            -draw "circle $CX,$CY $((CX + SW2)),$CY" \
            "$OUT"
        ;;

esac

# ------------------------------------------------------------------------------
# Resultado
# ------------------------------------------------------------------------------

if [[ ! -f "$OUT" ]]; then
    echo "Erro ao gerar: $OUT" >&2
    exit 1
fi

echo "$OUT"
