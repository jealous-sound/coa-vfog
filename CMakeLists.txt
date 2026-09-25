cmake_minimum_required(VERSION 3.20)
project(coa_vfog CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "coa-vfog targets the 32-bit client; configure with -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB FXC_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/10.*/x86/fxc.exe")
list(SORT FXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
list(GET FXC_CANDIDATES 0 FXC_DEFAULT)
set(FXC "${FXC_DEFAULT}" CACHE FILEPATH "HLSL compiler (fxc.exe)")
if(NOT EXISTS "${FXC}")
    message(FATAL_ERROR "fxc.exe not found; install the Windows SDK or set -DFXC=<path>")
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT})

set(SHADER_HEADERS)
function(vf_shader name source profile)
    set(out ${SHADER_OUT}/${name}.h)
    add_custom_command(
        OUTPUT ${out}
        COMMAND "${FXC}" /nologo /O3 /T ${profile} /E main /Vn g_${name} ${ARGN}
                /Fh ${out} /Fc ${SHADER_OUT}/${name}.asm ${SHADER_DIR}/${source}
        DEPENDS ${SHADER_DIR}/${source} ${SHADER_DIR}/vf_common.hlsli
        COMMENT "fxc ${name}"
        VERBATIM)
    set(SHADER_HEADERS ${SHADER_HEADERS} ${out} PARENT_SCOPE)
endfunction()

vf_shader(vs_fullscreen vf_fullscreen_vs.hlsl vs_3_0)
vf_shader(ps_march_low vf_march.hlsl ps_3_0 /DSTEPS=16)
vf_shader(ps_march_mid vf_march.hlsl ps_3_0 /DSTEPS=24)
vf_shader(ps_march_high vf_march.hlsl ps_3_0 /DSTEPS=32)
vf_shader(ps_temporal vf_temporal.hlsl ps_3_0)
vf_shader(ps_composite vf_composite.hlsl ps_3_0)
vf_shader(ps_ray_mask vf_ray_mask.hlsl ps_3_0)
vf_shader(ps_ray_blur vf_ray_blur.hlsl ps_3_0)
vf_shader(ps_probe vf_probe.hlsl ps_3_0)
add_custom_target(vf_shaders DEPENDS ${SHADER_HEADERS})

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui
    PUBLIC IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS
    PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
            IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS WIN32_LEAN_AND_MEAN NOMINMAX)

add_library(CoAVolFog SHARED
    src/config.cpp
    src/d3d9_wrap.cpp
    src/dllmain.cpp
    src/engine.cpp
    src/fog_data.cpp
    src/fog_model.cpp
    src/hooks.cpp
    src/log.cpp
    src/overlay.cpp
    src/renderer.cpp
    src/settings_panel.cpp
    src/CoAVolFog.def)
add_dependencies(CoAVolFog vf_shaders)
target_include_directories(CoAVolFog PRIVATE src ${SHADER_OUT})
target_compile_definitions(CoAVolFog PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(CoAVolFog PRIVATE /W4 /wd4100 /arch:SSE2 /fp:fast /GS)
target_link_libraries(CoAVolFog PRIVATE kernel32 user32 imgui)

add_library(version SHARED proxy/version_proxy.cpp proxy/version.def)
target_link_libraries(version PRIVATE CoAVolFog)
target_compile_definitions(version PRIVATE WIN32_LEAN_AND_MEAN)

add_executable(vfog_harness tests/harness.cpp src/config.cpp src/fog_data.cpp src/fog_model.cpp src/log.cpp)
target_include_directories(vfog_harness PRIVATE src)
target_compile_definitions(vfog_harness PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(vfog_harness PRIVATE CoAVolFog user32 ole32 windowscodecs)
add_custom_command(TARGET CoAVolFog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CoAVolFog.ini
            ${CMAKE_CURRENT_SOURCE_DIR}/data/fogdata.bin $<TARGET_FILE_DIR:CoAVolFog>)

enable_testing()
add_test(NAME vfog_harness
    COMMAND vfog_harness --out ${CMAKE_CURRENT_BINARY_DIR}/harness-out
            --data ${CMAKE_CURRENT_SOURCE_DIR}/data/fogdata.bin
            --ini ${CMAKE_CURRENT_SOURCE_DIR}/CoAVolFog.ini
    WORKING_DIRECTORY $<TARGET_FILE_DIR:vfog_harness>)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME comments
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_comments.py
                --root ${CMAKE_CURRENT_SOURCE_DIR})
endif()
