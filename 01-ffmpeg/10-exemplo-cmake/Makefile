.PHONY: configure build run clean

configure:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
		cmake -S . -B build -G Ninja \
  			-DCMAKE_BUILD_TYPE=Release \
  			-DCMAKE_POLICY_VERSION_MINIMUM=3.5

build:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
		cmake --build build -j"$(nproc)"

run: build/video-app
	./build/video-app

clean:
	rm -rf build output.mp4

play: output.mp4
	ffplay output.mp4

info:output.mp4
	ffprobe output.mp4


