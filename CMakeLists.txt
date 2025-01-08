cmake_minimum_required(VERSION 3.31)
project(sfml_video)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML
        GIT_TAG 2.6.2
        EXCLUDE_FROM_ALL SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(sfml_video
        main.cpp
        VideoPlayer.cpp
        AudioPlayer.cpp
)

target_link_libraries(sfml_video PRIVATE
        sfml-system sfml-graphics sfml-window sfml-audio
        avcodec avformat avdevice avfilter avutil swresample swscale
)

target_compile_options(sfml_video PRIVATE -Wall -Wextra)