add_subdirectory(cbt)

target_sources(
    renderer PRIVATE
    application.cpp
    application.hpp
    imgui_renderer.cpp
    imgui_renderer.hpp
    main.cpp
    window.cpp
    window.hpp
)
