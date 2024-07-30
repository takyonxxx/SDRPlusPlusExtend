cmake_minimum_required(VERSION 3.13)
project(hackrf_source)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  #set(CMAKE_TOOLCHAIN_FILE "C:/Users/turka/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
  set(CMAKE_TOOLCHAIN_FILE "C:/Users/MarenCompEng/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_sources(hackrf_source
  PRIVATE
    src/constants.h
    src/hackrfsourcemodule.h
    src/FrequencyModulator.h
    src/RationalResampler.h
    src/audiosource.h
)

if (MSVC)
    # Lib path
    target_link_directories(hackrf_source PRIVATE "C:/Program Files/PothosSDR/bin/")
    target_link_libraries(hackrf_source PRIVATE hackrf)

    # Find and link RtAudio via vcpkg
    find_package(RtAudio CONFIG REQUIRED)

    target_include_directories(hackrf_source PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_libraries(hackrf_source PRIVATE RtAudio::rtaudio)

elseif (ANDROID)
    target_include_directories(hackrf_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/include/libhackrf
    )

    target_link_libraries(hackrf_source PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libusb1.0.so
        /sdr-kit/${ANDROID_ABI}/lib/libhackrf.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBHACKRF REQUIRED libhackrf)
    target_include_directories(hackrf_source PRIVATE ${LIBHACKRF_INCLUDE_DIRS})
    target_link_directories(hackrf_source PRIVATE ${LIBHACKRF_LIBRARY_DIRS})
    target_link_libraries(hackrf_source PRIVATE ${LIBHACKRF_LIBRARIES})

    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    target_include_directories(hackrf_source PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(hackrf_source PRIVATE ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(hackrf_source PRIVATE ${PORTAUDIO_LIBRARIES})

    pkg_check_modules(RTAUDIO REQUIRED rtaudio)

    target_include_directories(hackrf_source PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_directories(hackrf_source PRIVATE ${RTAUDIO_LIBRARY_DIRS})
    target_link_libraries(hackrf_source PRIVATE ${RTAUDIO_LIBRARIES})

    find_package(Boost 1.85 REQUIRED)
    target_include_directories(hackrf_source PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(hackrf_source PRIVATE ${Boost_LIBRARIES})

endif ()
