
SCRIPRS = ./waves.txt
OUTPUT  = output.mp4
NFRAMES = 30

FFMPEG_OPT = \
			 -f lavfi \
			 -i color=black:1920x1080:r=30  \
			 -filter_complex_script $(SCRIPRS) \
			 -frames:v $(NFRAMES)

$(OUTPUT):
	ffmpeg $(FFMPEG_OPT) $(OUTPUT)

clean:
	rm $(OUTPUT)

