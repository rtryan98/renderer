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
    imgui/backends/imgui_impl_win32.cpp
    imgui/backends/imgui_impl_win32.h
)
target_include_directories(
    imgui PUBLIC
    imgui
)

add_subdirectory(fastgltf)
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
