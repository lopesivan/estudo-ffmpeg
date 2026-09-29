# ============================================================
# Configurações
# ============================================================
BUILD_DIR       := build
CONAN_BUILD_DIR := build
BUILD_TYPE      := Release
PKG_CONFIG_PATH := /usr/lib/x86_64-linux-gnu/pkgconfig
CONAN_TOOLCHAIN := $(CONAN_BUILD_DIR)/generators/conan_toolchain.cmake

.PHONY: all init config configure build run clean play info

all: build

init:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	conan install . \
		--output-folder=$(CONAN_BUILD_DIR) \
		--build=missing \
		-s build_type=$(BUILD_TYPE)

configure:
	@if [ ! -f "$(CONAN_TOOLCHAIN)" ]; then \
		echo "Toolchain do Conan não encontrado. Execute 'make init' primeiro."; \
		exit 1; \
	fi
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	cmake -S . -B $(CONAN_BUILD_DIR) -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE=$(CONAN_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	cmake --build $(CONAN_BUILD_DIR) -j"$$(nproc)"

run: $(CONAN_BUILD_DIR)/video-app
	./$(CONAN_BUILD_DIR)/video-app

clean:
	rm -rf $(CONAN_BUILD_DIR) output.mp4

play: output.mp4
	ffplay output.mp4

info: output.mp4
	ffprobe output.mp4
