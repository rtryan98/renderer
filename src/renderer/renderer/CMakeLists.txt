add_subdirectory(asset)
add_subdirectory(cbt)
add_subdirectory(filesystem)
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
    input_codes.hpp
    logger.cpp
    logger.hpp
    main.cpp
    renderer.cpp
    renderer.hpp
    window.cpp
    window.hpp
)
