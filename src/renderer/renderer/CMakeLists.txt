add_subdirectory(cbt)
add_subdirectory(imgui)
add_subdirectory(ocean)
add_subdirectory(scene)

target_sources(
    renderer PRIVATE
    application.cpp
    application.hpp
    asset.hpp
    asset_manager.cpp
    asset_manager.hpp
    logger.cpp
    logger.hpp
    main.cpp
    render_pass.cpp
    render_pass.hpp
    renderer.cpp
    renderer.hpp
    shader_manager.cpp
    shader_manager.hpp
    window.cpp
    window.hpp
)
