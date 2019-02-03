cmake_minimum_required(VERSION 3.5)
project(RnNoise LANGUAGES C)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
SET(CMAKE_BUILD_TYPE "Release")

# specify the cross compiler
#SET(CMAKE_C_COMPILER   /opt/gcc-armv7l-meego-linux-gnueabi-2016.01_linux/cross/bin/armv7l-meego-linux-gnueabi-gcc)
#SET(CMAKE_CXX_COMPILER /opt/gcc-armv7l-meego-linux-gnueabi-2016.01_linux/cross/bin/armv7l-meego-linux-gnueabi-g++)

# where is the target environment 
#SET(CMAKE_FIND_ROOT_PATH  /opt/gcc-armv7l-meego-linux-gnueabi-2016.01_linux/cross/armv7l-meego-linux-gnueabi/sys-root/)

# specify the compiler flag
#SET(CMAKE_C_FLAGS  "-std=c99 -O3 -mfpu=neon")
#SET(CMAKE_CXX_FLAGS  "-std=c++11 -O3 -mfpu=neon")

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
