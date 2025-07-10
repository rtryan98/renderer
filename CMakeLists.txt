cmake_minimum_required(VERSION 3.20)

project(
    renderer
    VERSION 0.1.0
    LANGUAGES CXX
)

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
)
target_include_directories(
    asset_baker PUBLIC
    src/asset_baker
    src/shared
    thirdparty/spdlog/include
    thirdparty/tclap/include
)
set_target_properties(
    asset_baker PROPERTIES
    CXX_STANDARD 23
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

add_subdirectory(src)

get_target_property(RENDERER_CPP_SOURCES renderer SOURCES)
list(FILTER RENDERER_CPP_SOURCES INCLUDE REGEX "[ch]pp")

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/renderer PREFIX src FILES ${RENDERER_CPP_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT renderer)

core_deploy_d3d12(D3D12)
