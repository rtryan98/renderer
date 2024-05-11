add_subdirectory(cbt)
add_subdirectory(imgui)
add_subdirectory(ocean)

target_sources(
    renderer PRIVATE
    application.cpp
    application.hpp
    imgui_renderer.cpp
    imgui_renderer.hpp
    logger.cpp
    logger.hpp
    main.cpp
    shader_manager.cpp
    shader_manager.hpp
    window.cpp
    window.hpp
)
