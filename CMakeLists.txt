cmake_minimum_required(VERSION 3.20)

set(RENDERER_DOWNLOAD_CACHE ${CMAKE_CURRENT_LIST_DIR}/download_cache)

project(
    renderer
    VERSION 0.1.0
    LANGUAGES CXX
)
include(renderer.cmake)

add_subdirectory(thirdparty)

add_executable(asset_baker)
target_link_libraries(
    asset_baker PUBLIC
    rhi
    unordered_dense
    fastgltf
    nlohmann_json
    xxHash::xxhash
    mikktspace
    enkiTS
    glm
    spdlog
)
target_include_directories(
    asset_baker PUBLIC
    src/asset_baker
    src/shared
    thirdparty/tclap/include
    thirdparty/stb
)
set_target_properties(
    asset_baker PROPERTIES
    CXX_STANDARD 23
)
target_compile_definitions(
    asset_baker PUBLIC
    SPDLOG_COMPILED_LIB
)
add_custom_command(
    TARGET asset_baker
    POST_BUILD
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i thirdparty/gltf-sample-assets/Models/DamagedHelmet -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i thirdparty/gltf-sample-assets/Models/MetalRoughSpheres -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i thirdparty/gltf-sample-assets/Models/Sponza -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i ${RENDERER_DOWNLOAD_CACHE}/intel_sponza -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i ${RENDERER_DOWNLOAD_CACHE}/intel_sponza_curtains -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --gltf -i ${RENDERER_DOWNLOAD_CACHE}/intel_sponza_ivy -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> --hdri -i ${RENDERER_DOWNLOAD_CACHE}/hdri -o ${CMAKE_BINARY_DIR}/assets/cache
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

# Additional assets
rhi_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/830833
    ${RENDERER_DOWNLOAD_CACHE}
    intel_sponza
)
rhi_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/726650
    ${RENDERER_DOWNLOAD_CACHE}
    intel_sponza_curtains
)
rhi_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/726656
    ${RENDERER_DOWNLOAD_CACHE}
    intel_sponza_ivy
)
if (NOT EXISTS ${RENDERER_DOWNLOAD_CACHE}/hdri/lonely_road_afternoon_puresky_4k_hdri)
    message(STATUS "Downloading https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/4k/lonely_road_afternoon_puresky_4k.hdr to ${RENDERER_DOWNLOAD_CACHE}/hdri/lonely_road_afternoon_puresky_4k.hdr.")
    file(
        DOWNLOAD
        https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/4k/lonely_road_afternoon_puresky_4k.hdr
        ${RENDERER_DOWNLOAD_CACHE}/hdri/lonely_road_afternoon_puresky_4k.hdr
    )
endif()
if (NOT EXISTS ${RENDERER_DOWNLOAD_CACHE}/hdri/qwantani_noon_puresky_4k_hdri)
    message(STATUS "Downloading https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/4k/qwantani_noon_puresky_4k.hdr to ${RENDERER_DOWNLOAD_CACHE}/hdri/qwantani_noon_puresky_4k.hdr.")
    file(
        DOWNLOAD
        https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/4k/qwantani_noon_puresky_4k.hdr
        ${RENDERER_DOWNLOAD_CACHE}/hdri/qwantani_noon_puresky_4k.hdr
    )
endif()

add_executable(renderer)
target_link_libraries(
    renderer PUBLIC
    rhi
    rhi_dxc_lib
    imgui
    Shcore.lib
    unordered_dense
    nlohmann_json
    offset_allocator
    glm
    SDL3::SDL3-static
    spdlog
)
target_include_directories(
    renderer PUBLIC
    src/renderer
    src/shared
    thirdparty/tclap/include
)
set_target_properties(
    renderer PROPERTIES
    CXX_STANDARD 23
)
target_compile_definitions(
    renderer PUBLIC
    REN_MAX_FRAMES_IN_FLIGHT=2
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_SWIZZLE
    SPDLOG_COMPILED_LIB
)

add_subdirectory(src)

get_target_property(RENDERER_CPP_SOURCES renderer SOURCES)
list(FILTER RENDERER_CPP_SOURCES INCLUDE REGEX "[ch]pp")

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/renderer PREFIX src FILES ${RENDERER_CPP_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT renderer)

rhi_symlink_shaders(${CMAKE_SOURCE_DIR}/assets/shaders)
rhi_deploy_d3d12(D3D12)

renderer_setup()
renderer_symlink_assets()
