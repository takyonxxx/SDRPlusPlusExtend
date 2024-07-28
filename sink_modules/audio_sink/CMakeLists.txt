cmake_minimum_required(VERSION 3.13)
project(audio_sink)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Include directory for RtAudio
    target_include_directories(audio_sink PRIVATE "C:/Users/turka/vcpkg/installed/x64-windows/include/rtaudio")

    # Library directory for RtAudio
    target_link_directories(audio_sink PRIVATE "C:/Users/turka/vcpkg/installed/x64-windows/lib")

    # Link the RtAudio library
    target_link_libraries(audio_sink PRIVATE rtaudio)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RTAUDIO REQUIRED rtaudio)

    target_include_directories(audio_sink PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_directories(audio_sink PRIVATE ${RTAUDIO_LIBRARY_DIRS})
    target_link_libraries(audio_sink PRIVATE ${RTAUDIO_LIBRARIES})
endif()
