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
