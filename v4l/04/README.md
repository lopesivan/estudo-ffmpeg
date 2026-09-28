Revisando o código, percebi que esqueci de enquadrar o buffer antes de iniciar a captura. Vamos corrigir o problema adicionando o seguinte trecho antes de iniciar a captura:

```
if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
    perror("Cannot enqueue buffer");
    close(outputFile);
    munmap(buffer, buf.length);
    close(fd);
    return 1;
}
```

