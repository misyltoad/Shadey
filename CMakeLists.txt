cmake_minimum_required (VERSION 3.6)

project(shadey)

set(ENABLE_GLSLANG_BINARIES ON)
set(ENABLE_SPVREMAPPER ON)
set(SKIP_GLSLANG_INSTALL ON)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(thirdparty/sleepy-discord)
add_subdirectory(thirdparty/tinyxml2)
add_subdirectory(thirdparty/glslang)

find_package(Vulkan)

add_executable(shadey
    src/client/main.cpp
    src/client/hooks.h
    src/client/hooks.cpp
    src/client/client.cpp
    src/client/client.h
    src/client/non_copyable.h
    src/client/shader_helpers.cpp
    src/client/shader_helpers.h
    src/client/renderer.cpp
    src/client/renderer.h
    src/client/command_helpers.h
    src/client/command_helpers.cpp
    src/client/commands/ping.cpp
    src/client/commands/shader.cpp
    src/client/commands/vulkan_types.cpp)
target_link_libraries(shadey sleepy-discord tinyxml2 glslang ${Vulkan_LIBRARY})
target_compile_definitions(shadey PRIVATE SHADEY_CLIENT)
target_include_directories(shadey PUBLIC src/client thirdparty/stb ${Vulkan_INCLUDE_DIRS})
set_property(TARGET shadey PROPERTY CXX_STANDARD 20)