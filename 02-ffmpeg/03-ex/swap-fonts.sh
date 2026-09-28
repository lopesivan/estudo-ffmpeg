#!/usr/bin/env bash

uppdir=../../..

# Nome do link simbólico fixo
SYMLINK="font.ttf"

# Arquivos para alternar
FILE1=${uppdir}/assets/fonts/SauceCodeProNerdFontMono-Regular.ttf
FILE2=${uppdir}/assets/fonts/DejaVuSans.ttf

# Verifica se os arquivos existem
for file in "$FILE1" "$FILE2"; do
    if [ ! -e "$file" ]; then
        echo "Erro: Arquivo não encontrado: $file"
        exit 1
    fi
done

# Verifica qual é o alvo atual do link
if [ -L "$SYMLINK" ]; then
    CURRENT_TARGET=$(readlink -f "$SYMLINK")

    # Resolve caminhos absolutos para comparação
    ABS_FILE1=$(realpath "$FILE1")
    ABS_FILE2=$(realpath "$FILE2")

    # Decide o novo alvo
    if [ "$CURRENT_TARGET" == "$ABS_FILE1" ]; then
        NEW_TARGET="$FILE2"
    elif [ "$CURRENT_TARGET" == "$ABS_FILE2" ]; then
        NEW_TARGET="$FILE1"
    else
        NEW_TARGET="$FILE1" # Padrão se não corresponder
    fi

    # Remove link antigo
    rm "$SYMLINK"
else
    NEW_TARGET="$FILE1" # Primeira execução
fi

# Cria novo link simbólico
ln -s "$NEW_TARGET" "$SYMLINK"
echo "Link atualizado para: $(basename "$NEW_TARGET")"

exit 0
