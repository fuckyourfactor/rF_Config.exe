cmake_minimum_required(VERSION 3.20)
project(rf_config_open_source VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    enable_language(RC)
    add_executable(rf-config WIN32
        src/main.cpp
        src/config.cpp
        src/config.hpp
        src/resources.rc
        src/app.manifest
        src/resource.h
    )
    target_compile_definitions(rf-config PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_include_directories(rf-config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(rf-config PRIVATE d3d9 comctl32 shell32 user32 gdi32 advapi32)
    if(MSVC)
        target_compile_options(rf-config PRIVATE /W4 /EHsc /utf-8)
        target_link_options(rf-config PRIVATE /MANIFEST:NO)
        set_property(TARGET rf-config PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(rf-config PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(rf-config PROPERTIES OUTPUT_NAME "rF Config")
    install(TARGETS rf-config RUNTIME DESTINATION .)
else()
    message(STATUS "Windows GUI target is unavailable; building portable configuration tests.")
endif()

enable_testing()
add_executable(config-tests tests/config_tests.cpp src/config.cpp src/config.hpp)
target_include_directories(config-tests PRIVATE src)
add_test(NAME config-tests COMMAND config-tests)
