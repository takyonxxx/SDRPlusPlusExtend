cmake_minimum_required(VERSION 3.13)
project(hackrf_sink)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

target_sources(hackrf_sink
  PRIVATE
    src/constants.h
)

if (MSVC)
    # Lib path
    target_link_directories(hackrf_sink PRIVATE "C:/Program Files/PothosSDR/bin/")
    target_link_libraries(hackrf_sink PRIVATE hackrf)

    find_package(portaudio CONFIG REQUIRED)
    target_link_libraries(hackrf_sink PRIVATE portaudio)

elseif (ANDROID)
    target_include_directories(hackrf_sink PUBLIC
        /sdr-kit/${ANDROID_ABI}/include/libhackrf
    )

    target_link_libraries(hackrf_sink PUBLIC
        /sdr-kit/${ANDROID_ABI}/lib/libusb1.0.so
        /sdr-kit/${ANDROID_ABI}/lib/libhackrf.so
    )
else (MSVC)
    find_package(PkgConfig)

    pkg_check_modules(LIBHACKRF REQUIRED libhackrf)
    target_include_directories(hackrf_sink PRIVATE ${LIBHACKRF_INCLUDE_DIRS})
    target_link_directories(hackrf_sink PRIVATE ${LIBHACKRF_LIBRARY_DIRS})
    target_link_libraries(hackrf_sink PRIVATE ${LIBHACKRF_LIBRARIES})

    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    target_include_directories(hackrf_sink PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_directories(hackrf_sink PRIVATE ${PORTAUDIO_LIBRARY_DIRS})
    target_link_libraries(hackrf_sink PRIVATE ${PORTAUDIO_LIBRARIES})

    find_package(Gnuradio REQUIRED)
    target_include_directories(hackrf_sink PRIVATE ${GNURADIO_INCLUDE_DIRS})
    target_link_libraries(hackrf_sink PRIVATE ${GNURADIO_LIBRARIES})

    find_package(Volk REQUIRED CONFIG)
    target_include_directories(hackrf_sink PRIVATE ${VOLK_INCLUDE_DIRS})
    target_link_libraries(hackrf_sink PRIVATE Volk::volk)

    # Manually set include directories for Homebrew libraries
    set(HOMEBREW_CELLAR_PATH /usr/local/Cellar)

    target_include_directories(hackrf_sink PRIVATE
        ${HOMEBREW_CELLAR_PATH}/hackrf/2024.02.1/include
        ${HOMEBREW_CELLAR_PATH}/spdlog/1.13.0/include
        ${HOMEBREW_CELLAR_PATH}/fmt/10.2.1_1/include
        ${HOMEBREW_CELLAR_PATH}/gmp/6.3.0/include
        ${HOMEBREW_CELLAR_PATH}/volk/3.1.2/include
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/include
        ${HOMEBREW_CELLAR_PATH}/boost/1.85.0/include
        ${HOMEBREW_CELLAR_PATH}/soapysdr/0.8.1_1/include
        ${HOMEBREW_CELLAR_PATH}/fftw/3.3.10_1/include
        ${HOMEBREW_CELLAR_PATH}/portaudio/19.7.0/include
    )

    # Link libraries from Homebrew
    target_link_libraries(hackrf_sink PRIVATE
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-analog.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-blocks.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-digital.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-filter.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-fft.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-runtime.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-audio.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-soapy.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-pmt.dylib
        ${HOMEBREW_CELLAR_PATH}/gnuradio/3.10.9.2_4/lib/libgnuradio-uhd.dylib
    )

endif ()
