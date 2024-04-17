set(COMPUTE_PIPELINE_LIBRARY_JSON_OUT_BASE_PATH ${CMAKE_BINARY_DIR})
set(COMPUTE_PIPELINE_LIBRARY_BASE_CMD py ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_compute_pso_library.py)

function (generate_compute_pipeline_library CONSUMING_TARGET TARGET SHADER_SOURCE_PATH SHADER_BINARY_RELATIVE_BASE_PATH OUT_FILE SOURCE_BASE_PATH)
    set(JSON_FILE_LIST)
    get_target_property(COMPUTE_LIBRARY_SOURCES ${TARGET} SOURCES)
    foreach(JSON_FILE ${COMPUTE_LIBRARY_SOURCES})
        get_filename_component(SOURCE_FILE_EXT ${JSON_FILE} LAST_EXT)
        if(NOT ${SOURCE_FILE_EXT} STREQUAL ".json")
            continue()
        endif()
        set(JSON_FILE_LIST ${JSON_FILE_LIST} ${JSON_FILE})
    endforeach()

    add_custom_command(
        TARGET ${TARGET}
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_compute_pso_library.py
        COMMAND ${COMPUTE_PIPELINE_LIBRARY_BASE_CMD} --outfile-path-wo-ext ${OUT_FILE} --source-base-path ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders --binary-base-path ./shaders --shader-json-files ${JSON_FILE_LIST} --cpp-source-base-path ${SOURCE_BASE_PATH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_sources(
        ${CONSUMING_TARGET} PRIVATE
        ${OUT_FILE}.cpp ${OUT_FILE}.hpp
    )
    set_source_files_properties(
        ${OUT_FILE}.cpp ${OUT_FILE}.hpp PROPERTIES
        GENERATED TRUE
    )
    get_filename_component(OUT_FILE_DIR ${OUT_FILE}.cpp DIRECTORY)
    source_group(TREE ${OUT_FILE_DIR} PREFIX src_generated FILES ${OUT_FILE}.cpp ${OUT_FILE}.hpp)
endfunction()
