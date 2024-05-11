add_subdirectory(cbt)
add_subdirectory(imgui)
add_subdirectory(ocean)

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
    shader_manager.cpp
    shader_manager.hpp
    window.cpp
    window.hpp
)
