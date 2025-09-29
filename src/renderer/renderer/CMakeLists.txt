add_subdirectory(asset)
add_subdirectory(filesystem)
add_subdirectory(imgui)
add_subdirectory(scene)
add_subdirectory(techniques)

target_sources(
    renderer PRIVATE
    acceleration_structure_builder.cpp
    acceleration_structure_builder.hpp
    application.cpp
    application.hpp
    gpu_transfer.cpp
    gpu_transfer.hpp
    input_codes.hpp
    logger.cpp
    logger.hpp
    main.cpp
    renderer.cpp
    renderer.hpp
    resource_state_tracker.cpp
    render_resource_blackboard.hpp
    render_resource_blackboard.cpp
    render_resource_blackboard.hpp
    window.cpp
    window.hpp
)
