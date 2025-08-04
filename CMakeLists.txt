cmake_minimum_required(VERSION 3.20)

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
)
target_include_directories(
    asset_baker PUBLIC
    src/asset_baker
    src/shared
    thirdparty/spdlog/include
    thirdparty/tclap/include
    thirdparty/stb
)
set_target_properties(
    asset_baker PROPERTIES
    CXX_STANDARD 23
)
add_custom_command(
    TARGET asset_baker
    POST_BUILD
    COMMAND $<TARGET_FILE:asset_baker> -i thirdparty/gltf-sample-assets/Models/ -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> -i ${CMAKE_BINARY_DIR}/download/intel_sponza -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> -i ${CMAKE_BINARY_DIR}/download/intel_sponza_curtains -o ${CMAKE_BINARY_DIR}/assets/cache
    COMMAND $<TARGET_FILE:asset_baker> -i ${CMAKE_BINARY_DIR}/download/intel_sponza_ivy -o ${CMAKE_BINARY_DIR}/assets/cache
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
# Asset baker is not supporting unicode right now.
# So long as the support is not there, it will crash with unicode inputs.
# To "remedy" it, delete this folder.
file(REMOVE_RECURSE thirdparty/gltf-sample-assets/Models/Unicode❤♻Test)

# Additional assets
core_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/830833
    ${CMAKE_BINARY_DIR}/download
    intel_sponza
)
core_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/726650
    ${CMAKE_BINARY_DIR}/download
    intel_sponza_curtains
)
core_download_and_extract_zip(
    https://cdrdv2.intel.com/v1/dl/getContent/726656
    ${CMAKE_BINARY_DIR}/download
    intel_sponza_ivy
)

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
)
target_include_directories(
    renderer PUBLIC
    src/renderer
    src/shared
    thirdparty/spdlog/include
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
)

add_subdirectory(src)

get_target_property(RENDERER_CPP_SOURCES renderer SOURCES)
list(FILTER RENDERER_CPP_SOURCES INCLUDE REGEX "[ch]pp")

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/renderer PREFIX src FILES ${RENDERER_CPP_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT renderer)

rhi_symlink_shaders(${CMAKE_SOURCE_DIR}/assets/shaders)
core_deploy_d3d12(D3D12)

renderer_setup()
renderer_symlink_assets()
