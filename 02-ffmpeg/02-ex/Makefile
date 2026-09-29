
SCRIPRS = ./waves.txt
OUTPUT  = output.mp4
NFRAMES = 200

FFMPEG_OPT = \
			 -f lavfi \
			 -i color=black:1920x1080:r=30  \
			 -filter_complex_script $(SCRIPRS) \
			 -frames:v $(NFRAMES)

$(OUTPUT):
	ffmpeg $(FFMPEG_OPT) $(OUTPUT)

play:
	ffplay -loop 30 $(OUTPUT)

clean:
	rm $(OUTPUT)

