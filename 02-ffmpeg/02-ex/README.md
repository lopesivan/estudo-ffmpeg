Beleza! Vamos criar um **efeito visual alternativo** para substituir o efeito de ondas do seu `waves.txt`. A ideia será:

**“Círculos concêntricos pulsando com o tempo”**, como se fossem ondas circulares partindo do centro da tela.

---

### Novo `waves.txt` com círculos pulsantes

```txt
[0]format=rgba,
geq=
r='st(0, 100 + 50*sin(T*2)); st(1, X-W/2); st(2, Y-H/2);
    255 * mod( floor( sqrt(ld(1)*ld(1) + ld(2)*ld(2)) / ld(0) ), 2 )':
g='st(3, 120 + 40*cos(T*1.5)); st(4, X-W/2); st(5, Y-H/2);
    180 * mod( floor( sqrt(ld(4)*ld(4) + ld(5)*ld(5)) / ld(3) ), 2 )':
b='st(6, 140 + 30*sin(T*1.2)); st(7, X-W/2); st(8, Y-H/2);
    150 * mod( floor( sqrt(ld(7)*ld(7) + ld(8)*ld(8)) / ld(6) ), 2 )'
```

---

### Explicação do efeito:

* `X - W/2` e `Y - H/2`: centraliza as coordenadas no meio da tela.
* `sqrt(...)`: calcula a **distância radial** do ponto ao centro.
* `mod(floor(... / raio_variável), 2)`: cria **anéis concêntricos** que mudam com o tempo (`T`) via `sin` e `cos`.

O resultado: **círculos que aparecem e desaparecem como ondas em uma superfície d'água**.

---

### Como usar:

1. Salve o novo conteúdo como `waves.txt`.
2. Execute o mesmo comando:

```bash
ffmpeg -f lavfi -i color=black:1920x1080:r=30 \
  -filter_complex_script waves.txt \
  -frames:v 90 output.mp4
```

3. Veja o resultado em `output.mp4`.

---

Se quiser que o centro **exploda em cores** ou tenha um **efeito de vórtice ou fractal**, posso montar outros scripts. Quer experimentar uma espiral agora?

