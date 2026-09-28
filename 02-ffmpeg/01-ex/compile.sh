#!/usr/bin/env bash

gcc -I/usr/include/x86_64-linux-gnu -o video_frames.o -c -I/workspace/estudo-cansi/include video_frames.c
gcc -I/usr/include/x86_64-linux-gnu -o /workspace/estudo-cansi/source/list.o -c -I/workspace/estudo-cansi/include /workspace/estudo-cansi/source/list.c
gcc -I/usr/include/x86_64-linux-gnu -o /workspace/estudo-cansi/source/frames.o -c -I/workspace/estudo-cansi/include /workspace/estudo-cansi/source/frames.c

gcc -o video_frames.exe \
    video_frames.o /workspace/estudo-cansi/source/list.o /workspace/estudo-cansi/source/frames.o \
    -lavformat -lavcodec -lavutil -lswscale
