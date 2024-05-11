add_subdirectory(imgui)
add_subdirectory(ocean)

target_sources(
    renderer_compute_pipeline_library PRIVATE
    fft.cs.json
)

target_sources(
    renderer PRIVATE
    shared_types.hlsli
)
