cmake_minimum_required(VERSION 3.20)

project(
    renderer
    VERSION 0.1.0
    LANGUAGES CXX
)

add_subdirectory(thirdparty)

include(scripts/shaders.cmake)
add_custom_target(renderer_builtin_resources)

include(scripts/generate_compute_pso_library.cmake)
add_custom_target(renderer_compute_pipeline_library)

add_executable(renderer)
target_link_libraries(
    renderer PUBLIC
    rhi
    rhi_dxc_lib
    imgui
    Shcore.lib
    fastgltf
    unordered_dense
)
target_include_directories(
    renderer PUBLIC
    src/renderer
    src/shared
    thirdparty/spdlog/include
)
set_target_properties(
    renderer PROPERTIES
    CXX_STANDARD 23
)
add_dependencies(
    renderer
    renderer_builtin_resources
    renderer_compute_pipeline_library
)

add_subdirectory(src)

get_target_property(RENDERER_CPP_SOURCES renderer SOURCES)
list(FILTER RENDERER_CPP_SOURCES INCLUDE REGEX "[ch]pp")

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/renderer PREFIX src FILES ${RENDERER_CPP_SOURCES})
get_target_property(RENDERER_BUILTIN_RESOURCES_SOURCES renderer_builtin_resources SOURCES)
# source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/shaders PREFIX src_shader FILES ${RENDERER_BUILTIN_RESOURCES_SOURCES})
set(RENDERER_BUILTIN_RESOURCES_COMMAND)
foreach(BUILTIN_SHADER_SOURCE ${RENDERER_BUILTIN_RESOURCES_SOURCES})

    get_filename_component(BUILTIN_SHADER_SOURCE_EXT ${BUILTIN_SHADER_SOURCE} EXT)
    get_filename_component(BUILTIN_SHADER_SOURCE_FILENAME_WE ${BUILTIN_SHADER_SOURCE} NAME_WE)
    get_filename_component(BUILTIN_SHADER_SOURCE_FILENAME_WLE ${BUILTIN_SHADER_SOURCE} NAME_WLE)
    if(${BUILTIN_SHADER_SOURCE_EXT} MATCHES "\.hlsl")
        renderer_compile_builtin_shader(${BUILTIN_SHADER_SOURCE})
        set(RENDERER_BUILTIN_SHADER_BIN_PATH ${RENDERER_BUILTIN_SHADER_FILEPATH}/${BUILTIN_SHADER_SOURCE_FILENAME_WLE})
        set(RENDERER_BUILTIN_SHADER_GEN_PATH ${RENDERER_SHADER_POST_TRANSLATE_PATH}/${BUILTIN_SHADER_SOURCE_FILENAME_WLE})
        string(REPLACE "\." "_" RENDERER_BUILTIN_SHADER_VAR ${BUILTIN_SHADER_SOURCE_FILENAME_WLE})
        set(RENDERER_BUILTIN_SHADER_DXIL_PARAMS ${RENDERER_BUILTIN_SHADER_BIN_PATH}.dxil ${RENDERER_BUILTIN_SHADER_GEN_PATH}.dxil.hpp ${RENDERER_BUILTIN_SHADER_VAR}_dxil)
        set(RENDERER_BUILTIN_SHADER_SPV_PARAMS ${RENDERER_BUILTIN_SHADER_BIN_PATH}.spv ${RENDERER_BUILTIN_SHADER_GEN_PATH}.spv.hpp ${RENDERER_BUILTIN_SHADER_VAR}_spv)
        set(RENDERER_BUILTIN_RESOURCES_COMMAND ${RENDERER_BUILTIN_RESOURCES_COMMAND} py scripts/bin_to_hex_char_array.py ${RENDERER_BUILTIN_SHADER_DXIL_PARAMS} &&)
        set(RENDERER_BUILTIN_RESOURCES_COMMAND ${RENDERER_BUILTIN_RESOURCES_COMMAND} py scripts/bin_to_hex_char_array.py ${RENDERER_BUILTIN_SHADER_SPV_PARAMS} &&)
        set_source_files_properties(
            ${RENDERER_BUILTIN_SHADER_GEN_PATH}.dxil.hpp
            ${RENDERER_BUILTIN_SHADER_GEN_PATH}.spv.hpp
            PROPERTIES GENERATED TRUE
        )
        target_sources(
            renderer PRIVATE
            ${RENDERER_BUILTIN_SHADER_GEN_PATH}.dxil.hpp
            ${RENDERER_BUILTIN_SHADER_GEN_PATH}.spv.hpp
        )
        source_group(TREE ${RENDERER_SHADER_POST_TRANSLATE_PATH} PREFIX src_generated FILES ${RENDERER_BUILTIN_SHADER_GEN_PATH}.dxil.hpp ${RENDERER_BUILTIN_SHADER_GEN_PATH}.spv.hpp)
    endif()
endforeach()
add_custom_command(
    TARGET renderer_builtin_resources POST_BUILD
    COMMAND ${RENDERER_BUILTIN_RESOURCES_COMMAND} echo on #hack for last &&
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
generate_compute_pipeline_library(renderer renderer_compute_pipeline_library ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders ./shaders ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/renderer/generated/compute_pipeline_library ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer)

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT renderer)

core_deploy_d3d12(D3D12)
