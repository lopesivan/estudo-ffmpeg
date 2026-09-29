#include "video_reader.hpp"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "Uso: %s <video>\n", argv[0]);

    return EXIT_FAILURE;
  }

  const char *filename = argv[1];

  if (!glfwInit()) {
    std::fprintf(stderr, "Nao foi possivel inicializar GLFW\n");

    return EXIT_FAILURE;
  }

  VideoReaderState video{};

  if (!video_reader_open(&video, filename)) {
    std::fprintf(stderr, "Nao foi possivel abrir o video: %s\n", filename);

    glfwTerminate();
    return EXIT_FAILURE;
  }

  GLFWwindow *window = glfwCreateWindow(video.width, video.height,
                                        "Video Reader", nullptr, nullptr);

  if (window == nullptr) {
    std::fprintf(stderr, "Nao foi possivel criar a janela GLFW\n");

    video_reader_close(&video);
    glfwTerminate();

    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);

  glfwSwapInterval(1);

  GLuint texture = 0;

  glGenTextures(1, &texture);

  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  const std::size_t frame_size = static_cast<std::size_t>(video.width) *
                                 static_cast<std::size_t>(video.height) * 4;

  std::uint8_t *frame_buffer = nullptr;

  constexpr std::size_t alignment = 128;

  if (posix_memalign(reinterpret_cast<void **>(&frame_buffer), alignment,
                     frame_size) != 0) {
    std::fprintf(stderr, "Nao foi possivel alocar o frame buffer\n");

    glDeleteTextures(1, &texture);

    glfwDestroyWindow(window);

    video_reader_close(&video);

    glfwTerminate();

    return EXIT_FAILURE;
  }

  bool first_frame = true;

  while (!glfwWindowShouldClose(window)) {
    std::int64_t pts = AV_NOPTS_VALUE;

    if (!video_reader_read_frame(&video, frame_buffer, &pts)) {
      break;
    }

    if (first_frame) {
      glfwSetTime(0.0);
      first_frame = false;
    }

    if (pts != AV_NOPTS_VALUE) {
      const double frame_time = static_cast<double>(pts) *
                                static_cast<double>(video.time_base.num) /
                                static_cast<double>(video.time_base.den);

      while (frame_time > glfwGetTime() && !glfwWindowShouldClose(window)) {
        const double remaining = frame_time - glfwGetTime();

        glfwWaitEventsTimeout(remaining);
      }
    }

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, video.width, video.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);

    int window_width = 0;
    int window_height = 0;

    glfwGetFramebufferSize(window, &window_width, &window_height);

    glViewport(0, 0, window_width, window_height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);

    glLoadIdentity();

    glOrtho(0.0, static_cast<double>(window_width),
            static_cast<double>(window_height), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    const double video_aspect =
        static_cast<double>(video.width) / static_cast<double>(video.height);

    const double window_aspect =
        static_cast<double>(window_width) / static_cast<double>(window_height);

    double draw_width = 0.0;
    double draw_height = 0.0;

    if (window_aspect > video_aspect) {
      draw_height = static_cast<double>(window_height);

      draw_width = draw_height * video_aspect;
    } else {
      draw_width = static_cast<double>(window_width);

      draw_height = draw_width / video_aspect;
    }

    const double x = (static_cast<double>(window_width) - draw_width) / 2.0;

    const double y = (static_cast<double>(window_height) - draw_height) / 2.0;

    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texture);

    glBegin(GL_QUADS);

    glTexCoord2d(0.0, 0.0);
    glVertex2d(x, y);

    glTexCoord2d(1.0, 0.0);
    glVertex2d(x + draw_width, y);

    glTexCoord2d(1.0, 1.0);
    glVertex2d(x + draw_width, y + draw_height);

    glTexCoord2d(0.0, 1.0);
    glVertex2d(x, y + draw_height);

    glEnd();

    glDisable(GL_TEXTURE_2D);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  std::free(frame_buffer);

  glDeleteTextures(1, &texture);

  glfwDestroyWindow(window);

  video_reader_close(&video);

  glfwTerminate();

  return EXIT_SUCCESS;
}
