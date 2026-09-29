cmake_minimum_required(VERSION 3.15)   # subi de 3.14 → 3.15 (requisito do seu projeto antigo)
project(video-app C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ----------------------------------------
# Opções de build para debug
# ----------------------------------------
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------------------------
# Conan: encontra o pacote algorithms_with_c
# ----------------------------------------
find_package(algorithms_with_c REQUIRED)

# Caminhos do pacote (mesma lógica do projeto antigo)
if(DEFINED algorithms_with_c_PACKAGE_FOLDER_RELEASE)
  set(ALG_BASE "${algorithms_with_c_PACKAGE_FOLDER_RELEASE}")
else()
  set(ALG_BASE "${algorithms_with_c_PACKAGE_FOLDER}")
endif()

set(ALG_INCLUDE "${ALG_BASE}/include")
set(ALG_SRC     "${ALG_BASE}/src")

# ----------------------------------------
# FFmpeg local (o que você já tem)
# ----------------------------------------
add_subdirectory(lib/FFmpeg)

# ----------------------------------------
# Fontes do app
# ----------------------------------------
list(APPEND SOURCES
    video_frames.c
    # Descomente os .c do algorithms_with_c conforme precisar:
    ${ALG_SRC}/list.c
    # ${ALG_SRC}/queue.c
    # ${ALG_SRC}/stack.c
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 ${SOURCES})

# ----------------------------------------
# Includes
# ----------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE ${ALG_INCLUDE})

# ----------------------------------------
# Bibliotecas
# ----------------------------------------
list(APPEND EXTRA_LIBS "-lm")

target_link_libraries(${PROJECT_NAME} PRIVATE
    FFmpeg
    ${EXTRA_LIBS}
)
