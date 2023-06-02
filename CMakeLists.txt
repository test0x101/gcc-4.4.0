cmake_minimum_required(VERSION 3.20)

project(gcc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include_directories(gcc)
include_directories(../include)
include_directories(../fixincludes)
include_directories(../libcpp)
include_directories(../libcpp/include)

include_directories(../gcc/)
include_directories(../gcc/c)
include_directories(../gcc/config)
include_directories(../gcc/config/i386)
include_directories(../libiberty)


file(GLOB GCC_SRCS
        gcc/*.h
        gcc/*.c
        gcc/*.cc
        gcc/*.def


        ../libiberty/*.h
        ../libiberty/*.c
        ../libiberty/*.cc

        ../gcc/*.h
        ../include/*.h
        ../fixincludes/*.h
        ../libcpp/*.h
        ../libcc1/*.h
        ../gcc/*.h
        ../gcc/c/*.h

        ../gcc/config/*.h
        ../gcc/config/i386/*.h

        ../gcc/ginclude/*.h


        ../include/*.c
        ../fixincludes/*.c
        ../libcpp/*.c
        ../libcc1/*.c
        ../gcc/*.c
        ../gcc/c/*.c
        ../gcc/config/*.c
        ../gcc/config/i386/*.c
        ../gcc/ginclude/*.c

        ../include/*.cc
        ../fixincludes/*.cc
        ../libcpp/*.cc

        ../gcc/*.cc
        ../gcc/c/*.cc


        ../gcc/config/*.cc
        ../gcc/config/i386/*.cc

        ../gcc/ginclude/*.cc


        ../gcc/*.def
        )


SET_SOURCE_FILES_PROPERTIES(${GCC_SRCS} PROPERTIES LANGUAGE CXX)

add_executable(gcc
        ${GCC_SRCS}
        )

add_definitions(-DRTX_CODE)
