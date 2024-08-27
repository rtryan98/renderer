add_subdirectory(imgui)
add_subdirectory(ocean)

target_sources(
    renderer_compute_pipeline_library PRIVATE
    fft.cs.json
    fullscreen_triangle.vs.json
)
