cmake_minimum_required(VERSION 3.29)
project(file)

set(CMAKE_CXX_STANDARD 20)

add_library(file_code STATIC src/vle/gbsequence.cpp
        src/vle/gbsequence.hpp
        src/vle.hpp
        src/vle/unicode.hpp
        src/vle/unicode/Utf8Sequence.cpp
        src/vle/unicode/Utf8Sequence.hpp
        src/vle/unicode/Utf16Sequence.cpp
        src/vle/unicode/Utf16Sequence.hpp)

add_executable(file src/main.cpp)
target_link_libraries(file file_code)
