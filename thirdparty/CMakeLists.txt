set(RHI_BUILD_TESTS OFF)
add_subdirectory(rhi)

add_library(imgui)
target_sources(
    imgui PRIVATE
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_sdl3.h
)
target_include_directories(
    imgui PUBLIC
    imgui
)
target_link_libraries(
    imgui PUBLIC
    SDL3::SDL3-static
)

add_subdirectory(fastgltf)
add_subdirectory(nlohmann_json)
add_subdirectory(unordered_dense)

set(XXHASH_BUILD_XXHSUM OFF)
option(BUILD_SHARED_LIBS OFF)
add_subdirectory(xxHash/build/cmake xxhash_build EXCLUDE_FROM_ALL)

add_library(mikktspace)
target_sources(
    mikktspace PRIVATE
    MikkTSpace/mikktspace.c
    MikkTSpace/mikktspace.h
)
target_include_directories(
    mikktspace PUBLIC
    MikkTSpace
)
set_target_properties(
    mikktspace PROPERTIES
    CXX_STANDARD 23
)

set(ENKITS_BUILD_C_INTERFACE OFF)
set(ENKITS_BUILD_EXAMPLES OFF)
add_subdirectory(enkiTS)

add_library(offset_allocator)
target_sources(
    offset_allocator PRIVATE
    offsetAllocator/offsetAllocator.cpp
    offsetAllocator/offsetAllocator.hpp
)
target_include_directories(
    offset_allocator PUBLIC
    offsetAllocator
)
set_target_properties(
    offset_allocator PROPERTIES
    CXX_STANDARD 23
)

add_subdirectory(glm)

set(SDL_TEST_LIBRARY OFF)
add_subdirectory(sdl)
add_subdirectory(spdlog)

file(
    COPY RobotoMono/fonts/ttf/RobotoMono-Regular.ttf
    DESTINATION ${CMAKE_SOURCE_DIR}/assets/fonts/
)

add_subdirectory(meshoptimizer)

add_library(bc7enc_rdo)
target_sources(
    bc7enc_rdo PRIVATE
    bc7enc_rdo/bc7enc.cpp
    bc7enc_rdo/bc7enc.h
    bc7enc_rdo/bc7decomp.cpp
    bc7enc_rdo/bc7decomp_ref.cpp
    bc7enc_rdo/bc7decomp.h
    bc7enc_rdo/ert.cpp
    bc7enc_rdo/ert.h
    bc7enc_rdo/rdo_bc_encoder.cpp
    bc7enc_rdo/rdo_bc_encoder.h
    bc7enc_rdo/rgbcx.cpp
    bc7enc_rdo/rgbcx.h
    bc7enc_rdo/rgbcx_table4.h
    bc7enc_rdo/rgbcx_table4_small.h
    bc7enc_rdo/utils.cpp
    bc7enc_rdo/utils.h
    bc7enc_rdo/lodepng.cpp
    bc7enc_rdo/lodepng.h
    bc7enc_rdo/miniz.h
    bc7enc_rdo/dds_defs.h
)
target_include_directories(
    bc7enc_rdo PUBLIC
    bc7enc_rdo
)
set_target_properties(
    bc7enc_rdo PROPERTIES
    CXX_STANDARD 23
)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(bc7enc_rdo PRIVATE OpenMP::OpenMP_CXX)
endif()
