cmake_minimum_required(VERSION 3.6)
project(RnNoise LANGUAGES C)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
SET(CMAKE_BUILD_TYPE "Release")
set(RN_NOISE_SRC
        src/denoise.c
        src/celt_lpc.c
        src/pitch.c
        src/rnn.c
        src/rnn_data.c
        examples/rnnoise_demo.c)

include_directories(include)
add_executable(rnnoise_demo ${RN_NOISE_SRC})
target_link_libraries(rnnoise_demo -lm)