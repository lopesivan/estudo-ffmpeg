BUILD_TYPE ?= Release
BUILD_DIR := build
CONAN_BUILD_DIR := $(BUILD_DIR)/$(BUILD_TYPE)
CONAN_TOOLCHAIN := $(CONAN_BUILD_DIR)/generators/conan_toolchain.cmake

TARGET ?= video_frames
JOBS ?= $(shell nproc)

.PHONY: all init configure build run clean rebuild

all: build

init:
	conan profile detect --force
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	conan install . \
		--build=missing \
		-s build_type=$(BUILD_TYPE)

configure: init
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	cmake \
		-S . \
		-B $(CONAN_BUILD_DIR) \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_TOOLCHAIN_FILE=$(CONAN_TOOLCHAIN)

build:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
	cmake --build $(CONAN_BUILD_DIR) -j$(JOBS)

run: $(CONAN_BUILD_DIR)/video-app arquivo.mp4
	$(CONAN_BUILD_DIR)/video-app arquivo.mp4

clean:
	rm -rf $(BUILD_DIR) *.ppm

rebuild: clean build
