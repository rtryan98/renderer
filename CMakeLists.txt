cmake_minimum_required(VERSION 3.20)

project(
    renderer
    VERSION 0.1.0
    LANGUAGES CXX
)

add_subdirectory(thirdparty)

add_executable(renderer)
target_link_libraries(
    renderer PUBLIC
    rhi
    rhi_dxc_lib
    imgui
)
target_include_directories(
    renderer PUBLIC
    src/renderer
)
set_target_properties(
    renderer PROPERTIES
    CXX_STANDARD 23
)

add_subdirectory(src)

get_target_property(RENDERER_SOURCES renderer SOURCES)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src/renderer PREFIX src FILES ${RENDERER_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT renderer)

core_deploy_d3d12(D3D12)
