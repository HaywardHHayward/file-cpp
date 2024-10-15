cmake_minimum_required(VERSION 3.29)
project(file)

set(CMAKE_CXX_STANDARD 20)


add_executable(file src/main.cpp
        src/vle/gbsequence.cpp
        src/vle/gbsequence.hpp
        src/vle.hpp
        src/vle/unicode.hpp
        src/vle/unicode/Utf8Sequence.cpp
        src/vle/unicode/Utf8Sequence.hpp
        src/vle/unicode/Utf16Sequence.cpp
        src/vle/unicode/Utf16Sequence.hpp
)
