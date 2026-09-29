#!/usr/bin/env bash
set -euo pipefail

uppdir=..

# Nome do link simbólico fixo
SYMLINK="arquivo.mp4"

# Arquivos para alternar (ciclo: FILE1 -> FILE2 -> FILE3 -> FILE1 -> ...)
FILE1="${uppdir}/assets/drink.mp4"
FILE2="${uppdir}/assets/arquivo.mp4"
FILE3="${uppdir}/assets/output.mp4"

FILES=("$FILE1" "$FILE2" "$FILE3")

# Verifica se todos os arquivos existem
for file in "${FILES[@]}"; do
    if [ ! -e "$file" ]; then
        echo "Erro: Arquivo não encontrado: $file" >&2
        exit 1
    fi
done

# Resolve caminhos absolutos uma vez
ABS_FILES=()
for file in "${FILES[@]}"; do
    ABS_FILES+=("$(realpath "$file")")
done

# Decide o próximo alvo
if [ -L "$SYMLINK" ]; then
    # readlink -f falha se o alvo não existir; usamos sem -f e resolvemos
    # manualmente contra o diretório do link
    CURRENT_TARGET=$(readlink "$SYMLINK" || true)

    if [ -n "$CURRENT_TARGET" ]; then
        # Se for relativo, resolve a partir do diretório onde está o link
        case "$CURRENT_TARGET" in
            /*) ABS_CURRENT=$(realpath -m "$CURRENT_TARGET") ;;
            *) ABS_CURRENT=$(realpath -m "$(dirname "$SYMLINK")/$CURRENT_TARGET") ;;
        esac
    else
        ABS_CURRENT=""
    fi

    # Encontra o índice do alvo atual e avança para o próximo
    NEXT_IDX=0
    FOUND=0
    for i in "${!ABS_FILES[@]}"; do
        if [ "$ABS_CURRENT" == "${ABS_FILES[$i]}" ]; then
            NEXT_IDX=$(((i + 1) % ${#ABS_FILES[@]}))
            FOUND=1
            break
        fi
    done

    # Se não encontrou (link quebrado ou apontando para outro lugar),
    # começa do primeiro
    if [ "$FOUND" -eq 0 ]; then
        NEXT_IDX=0
    fi

    NEW_TARGET="${FILES[$NEXT_IDX]}"
    rm -f "$SYMLINK"
else
    # Primeira execução
    NEW_TARGET="${FILES[0]}"
fi

# Cria novo link simbólico
ln -s "$NEW_TARGET" "$SYMLINK"
echo "Link atualizado para: $(basename "$NEW_TARGET")"

exit 0
