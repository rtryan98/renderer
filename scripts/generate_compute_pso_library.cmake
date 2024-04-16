set(COMPUTE_PIPELINE_LIBRARY_JSON_OUT_BASE_PATH ${CMAKE_BINARY_DIR})

function (generate_compute_pipeline_library CONSUMING_TARGET TARGET SHADER_SOURCE_PATH SHADER_BINARY_RELATIVE_BASE_PATH OUT_FILE)
    set(JSON_OUT "[\n")
    get_target_property(COMPUTE_LIBRARY_SOURCES ${TARGET} SOURCES)
    set_source_files_properties(${COMPUTE_LIBRARY_SOURCES} PROPERTIES VS_TOOL_OVERRIDE "None")
    foreach(JSON_FILE ${COMPUTE_LIBRARY_SOURCES})
        get_filename_component(SOURCE_FILE_EXT ${JSON_FILE} LAST_EXT)
        if(NOT ${SOURCE_FILE_EXT} STREQUAL ".json")
            continue()
        endif()

        # Get the relative path to the shader source for the source_path entry in the Compute_Pipeline_Metadata struct
        get_filename_component(JSON_FILE_DIR ${JSON_FILE} DIRECTORY)
        string(REPLACE ${SHADER_SOURCE_PATH} "" JSON_FILE_DIR ${JSON_FILE_DIR})
        get_filename_component(JSON_FILE_NAME_WLE ${JSON_FILE} NAME_WLE)
        string(CONCAT SHADER_SOURCE_RELATIVE_FILE_PATH "." ${JSON_FILE_DIR} "/" ${JSON_FILE_NAME_WLE} ".hlsl")

        # Get the relative path to the shader binary for the binary_path entry in the Compute_Pipeline_Metadata struct
        string(CONCAT SHADER_BINARY_RELATIVE_FILE_PATH ${SHADER_BINARY_RELATIVE_BASE_PATH} "/" ${JSON_FILE_NAME_WLE} ".bin")

        # Get the name of the shader for the name entry in the Compute_Pipeline_Metadata struct
        get_filename_component(SHADER_NAME ${JSON_FILE} NAME_WE)

        # Generate the JSON entry string
        file(READ ${JSON_FILE} SHADER_JSON_STRING)
        string(JSON JSON_PERMUTATIONS GET ${SHADER_JSON_STRING} "permutations")
        set(JSON_ENTRY "{\"name\": \"${SHADER_NAME}\",\"binary_path\": \"${SHADER_BINARY_RELATIVE_FILE_PATH}\",\"source_path\": \"${SHADER_SOURCE_RELATIVE_FILE_PATH}\",\"permutations\": []},")
        string(JSON JSON_ENTRY SET ${JSON_ENTRY} "permutations" ${JSON_PERMUTATIONS})

        # Append to target json file for processing in generate_compute_pso_library.py
        string(APPEND JSON_OUT ${JSON_ENTRY})
    endforeach()
    string(APPEND JSON_OUT "]")
    string(REPLACE "\n" "" JSON_OUT ${JSON_OUT})

    add_custom_command(
        TARGET ${TARGET}
        OUTPUT ${OUTFILE_PATH}
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_compute_pso_library.py
        COMMAND py ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_compute_pso_library.py ${JSON_OUT} ${OUT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )

    target_sources(
        ${CONSUMING_TARGET} PRIVATE
        ${OUT_FILE}
    )
    set_source_files_properties(
        ${OUT_FILE} PROPERTIES
        GENERATED TRUE
    )
    get_filename_component(OUT_FILE_DIR ${OUT_FILE} DIRECTORY)
    source_group(TREE ${OUT_FILE_DIR} PREFIX src_generated FILES ${OUT_FILE})
endfunction()
