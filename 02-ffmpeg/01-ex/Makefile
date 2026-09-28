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
	eog imagem_000.ppm

# Target para limpeza dos arquivos gerados
clean:
	$(MAKE_CMD) clean
	rm imagem*.ppm

