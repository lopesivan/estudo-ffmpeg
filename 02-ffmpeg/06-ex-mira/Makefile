.PHONY: configure build run clean

configure:
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
		cmake -S . -B build -G Ninja \
  			-DCMAKE_BUILD_TYPE=Release \
  			-DCMAKE_POLICY_VERSION_MINIMUM=3.5

build:
	./mira.sh
	PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig \
		cmake --build build -j"$(nproc)"

run: build/app
	./build/app

clean:
	rm -rf build *.png output.mp4

play: output.mp4
	ffplay output.mp4

info: output.mp4
	ffprobe output.mp4


MIRA_SCRIPT  := ./mira2.sh
MIRA_DIR     := miras
MIRA_SIZE    := 300
MIRA_ESTILOS := classica sniper radar predador drone bussola scanner retro coruja xadrez

.PHONY: miras

miras:
	mkdir -p $(MIRA_DIR)
	for estilo in $(MIRA_ESTILOS); do \
		$(MIRA_SCRIPT) "$$estilo" "$(MIRA_SIZE)" "$(MIRA_DIR)/mira-$$estilo.png"; \
	done
