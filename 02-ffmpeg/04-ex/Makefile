# Define o arquivo Makefile principal a ser incluído
MAKEFILE := frames.mak

# Obtém o diretório raiz do repositório Git
GIT_ROOT := $(shell git rev-parse --show-toplevel)

# Define o comando base para execução
MAKE_CMD = ALGOWC_TOPDIR=$(GIT_ROOT) $(MAKE) -f $(MAKEFILE)

# Declara os targets que não correspondem a arquivos
.PHONY: all clean

# Target principal que será executado por padrão
all:
	$(MAKE_CMD)

bear:
	$@ -- $(MAKE) -f $(MAKEFILE)

# Target para rodar programa
run:
	$(MAKE_CMD) $@

# testa memória do programa
free:
	$(MAKE_CMD) $@

# número de frames de vídeo do arquivo de vídeo
frames:
	$(MAKE_CMD) $@

show:
	sxiv webcam_frame_*.png

ls:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig pkg-config --list-all| fzf

# Target para limpeza dos arquivos gerados
clean:
	$(MAKE_CMD) clean
	rm webcam_frame_*.png

install-deps:
	@echo "Para instalar as dependências no Ubuntu/Debian:"
	@echo "sudo apt update"
	@echo "sudo apt install libpng-dev v4l-utils"
	@echo ""
	@echo "Para instalar no CentOS/RHEL/Fedora:"
	@echo "sudo yum install libpng-devel v4l-utils"
	@echo "ou"
	@echo "sudo dnf install libpng-devel v4l-utils"

test-webcam:
	@echo "Testando dispositivos de vídeo disponíveis:"
	@ls -la /dev/video*
	@echo ""
	@echo "Para testar sua webcam com v4l2:"
	@echo "v4l2-ctl --list-devices"
	@echo "v4l2-ctl -d /dev/video0 --list-formats-ext"

