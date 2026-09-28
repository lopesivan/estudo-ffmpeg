Ótimo! Nesse caso, sua imagem é colorida com canais RGB intercalados em img->data, ou seja, cada pixel ocupa 3 bytes (um para R, G e B).

Para aplicar detecção de bordas como no exemplo anterior, é melhor primeiro converter a imagem para tons de cinza, e depois aplicar o operador de Sobel.

Passo 1: Conversão de RGB para Escala de Cinza

A conversão comum é:

gray = 0.299 * R + 0.587 * G + 0.114 * B

Passo 2: Aplicar o filtro de bordas (como já mostrei)

Agora segue o código completo com ambos os passos:

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    int length; // altura
    int width;  // largura
    uint8_t* data; // vetor RGBRGBRGB...
} Image;

void rgb_to_grayscale(Image* img, uint8_t* gray) {
    int size = img->length * img->width;
    for (int i = 0; i < size; i++) {
        int r = img->data[i * 3];
        int g = img->data[i * 3 + 1];
        int b = img->data[i * 3 + 2];
        gray[i] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
    }
}

void sobel_edge_detection(uint8_t* gray, int width, int height, uint8_t* output) {
    int gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    int gy[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sumX = 0;
            int sumY = 0;

            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = gray[(y + ky) * width + (x + kx)];
                    sumX += pixel * gx[ky + 1][kx + 1];
                    sumY += pixel * gy[ky + 1][kx + 1];
                }
            }

            int magnitude = sqrt(sumX * sumX + sumY * sumY);
            if (magnitude > 255) magnitude = 255;

            output[y * width + x] = (uint8_t)magnitude;
        }
    }
}

Como usar:

Image img;
// suponha que img.length, img.width e img.data já estejam preenchidos
int totalPixels = img.length * img.width;

uint8_t* gray = malloc(totalPixels);
uint8_t* edges = malloc(totalPixels);

rgb_to_grayscale(&img, gray);
sobel_edge_detection(gray, img.width, img.length, edges);

// Agora 'edges' contém a imagem com bordas em escala de cinza

Quer ajuda para fazer um exemplo que realmente mostre a imagem (por exemplo, com uma biblioteca como stb_image ou OpenCV)?


