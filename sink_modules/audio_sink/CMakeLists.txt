cmake_minimum_required(VERSION 3.13)
project(audio_sink)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    #set(CMAKE_TOOLCHAIN_FILE "C:/Users/turka/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/MarenCompEng/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    # Find and link RtAudio via vcpkg
    find_package(RtAudio CONFIG REQUIRED)

    target_include_directories(audio_sink PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_libraries(audio_sink PRIVATE RtAudio::rtaudio)

else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(RTAUDIO REQUIRED rtaudio)

    target_include_directories(audio_sink PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_directories(audio_sink PRIVATE ${RTAUDIO_LIBRARY_DIRS})
    target_link_libraries(audio_sink PRIVATE ${RTAUDIO_LIBRARIES})

endif ()
