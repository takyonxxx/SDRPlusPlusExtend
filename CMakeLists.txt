cmake_minimum_required(VERSION 3.13)
project(sdrpp)

# Backends
option(OPT_BACKEND_GLFW "Use the GLFW backend" ON)
option(OPT_BACKEND_ANDROID "Use the Android backend" OFF)

# Compatibility Options
option(OPT_OVERRIDE_STD_FILESYSTEM "Use a local version of std::filesystem on systems that don't have it yet" OFF)

# Sources
option(OPT_BUILD_RTL_SDR_SOURCE "Build RTL-SDR Source Module (Dependencies: librtlsdr)" ON)
option(OPT_BUILD_HACKRF_SOURCE "Build HackRF Source Module (Dependencies: libhackrf)" ON)

# Sinks
option(OPT_BUILD_AUDIO_SINK "Build Audio Sink Module (Dependencies: rtaudio)" ON)

# Decoders
option(OPT_BUILD_RADIO "Main audio modulation decoder (AM, FM, SSB, etc...)" ON)

option(OPT_BUILD_RECORDER "Audio and baseband recorder" ON)

# Other options
option(USE_INTERNAL_LIBCORRECT "Use an internal version of libcorrect" ON)
option(USE_BUNDLE_DEFAULTS "Set the default resource and module directories to the right ones for a MacOS .app" OFF)
option(COPY_MSVC_REDISTRIBUTABLES "Copy over the Visual C++ Redistributable" OFF)

# Module cmake path
set(SDRPP_MODULE_CMAKE "${CMAKE_SOURCE_DIR}/sdrpp_module.cmake")

# Root source folder
set(SDRPP_CORE_ROOT "${CMAKE_SOURCE_DIR}/core/src/")

# Compiler flags
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    # Debug Flags
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -g -Og -std=c++17)
    endif ()
else()
    # Normal Flags
    if (MSVC)
        set(SDRPP_COMPILER_FLAGS /O2 /Ob2 /std:c++17 /EHsc)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
    else ()
        set(SDRPP_COMPILER_FLAGS -O3 -std=c++17)
    endif ()
endif()
set(SDRPP_MODULE_COMPILER_FLAGS ${SDRPP_COMPILER_FLAGS})

# Set a default install prefix
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "..." FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
    endif()
endif()

# Configure toolchain for android
if (ANDROID)
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
    )
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=c++17")
endif (ANDROID)

# Core of SDR++
add_subdirectory("core")

# Source modules

if (OPT_BUILD_HACKRF_SOURCE)
add_subdirectory("source_modules/hackrf_source")
endif (OPT_BUILD_HACKRF_SOURCE)

if (OPT_BUILD_RTL_SDR_SOURCE)
add_subdirectory("source_modules/rtl_sdr_source")
endif (OPT_BUILD_RTL_SDR_SOURCE)

# Sink modules
if (OPT_BUILD_AUDIO_SINK)
add_subdirectory("sink_modules/audio_sink")
endif (OPT_BUILD_AUDIO_SINK)

# Decoders
if (OPT_BUILD_RADIO)
add_subdirectory("decoder_modules/radio")
endif (OPT_BUILD_RADIO)

# Misc
if (OPT_BUILD_RECORDER)
add_subdirectory("misc_modules/recorder")
endif (OPT_BUILD_RECORDER)

if (MSVC)
    add_executable(sdrpp "src/main.cpp" "win32/resources.rc")
else ()
    add_executable(sdrpp "src/main.cpp")
endif ()

target_link_libraries(sdrpp PRIVATE sdrpp_core)

# Compiler arguments
target_compile_options(sdrpp PRIVATE ${SDRPP_COMPILER_FLAGS})

if (MSVC)
    add_custom_target(do_always ALL xcopy /s \"$<TARGET_FILE_DIR:sdrpp_core>\\*.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)
    add_custom_target(do_always_volk ALL xcopy /s \"C:/Program Files/PothosSDR/bin\\volk.dll\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y)

    # Create the directory if it doesn't exist
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Release/modules")

    # Define the target and post-build command as above
    add_custom_command(TARGET sdrpp
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:hackrf_source>
        "${CMAKE_BINARY_DIR}/Release/modules/"
        COMMENT "Copying hackrf_source.dll to modules directory"
    )

    if (COPY_MSVC_REDISTRIBUTABLES)
        # Get the list of Visual C++ runtime DLLs
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP True)
        include(InstallRequiredSystemLibraries)
        # Create a space separated list
        set(REDIST_DLLS_STR "")
        foreach(DLL IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            set(REDIST_DLLS_STR COMMAND xcopy /F \"${DLL}\" \"$<TARGET_FILE_DIR:sdrpp>\" /Y ${REDIST_DLLS_STR})
        endforeach()

        # Create target
        add_custom_target(do_always_msvc ALL ${REDIST_DLLS_STR})
    endif ()
endif ()


if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries(sdrpp PUBLIC pthread)
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.so\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(do_always ALL cp \"$<TARGET_FILE_DIR:sdrpp_core>/libsdrpp_core.dylib\" \"$<TARGET_FILE_DIR:sdrpp>\")
endif ()

# cmake .. "-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" -DOPT_BUILD_BLADERF_SOURCE=ON -DOPT_BUILD_LIMESDR_SOURCE=ON -DOPT_BUILD_SDRPLAY_SOURCE=ON -DOPT_BUILD_M17_DECODER=ON -DOPT_BUILD_SCANNER=ON -DOPT_BUILD_SCHEDULER=ON -DOPT_BUILD_USRP_SOURCE=ON -DOPT_BUILD_PAGER_DECODER=ON

# Create module cmake file
configure_file(${CMAKE_SOURCE_DIR}/sdrpp_module.cmake ${CMAKE_CURRENT_BINARY_DIR}/sdrpp_module.cmake @ONLY)

# Install directives
install(TARGETS sdrpp DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/bandplans DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/colormaps DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/fonts DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/icons DESTINATION share/sdrpp)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/root/res/themes DESTINATION share/sdrpp)
configure_file(${CMAKE_SOURCE_DIR}/sdrpp.desktop ${CMAKE_CURRENT_BINARY_DIR}/sdrpp.desktop @ONLY)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sdrpp.desktop DESTINATION share/applications)
endif ()

# Create uninstall target
configure_file(${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Create headers target
