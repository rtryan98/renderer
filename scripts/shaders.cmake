set(RENDERER_DXC_ARGS -enable-16bit-types -HV 2021 -Zpr -O3)
set(RENDERER_DXC_INCLUDE_ARGS -I ../../../../../../src/shaders -I ../../../../../../thirdparty/rhi/src/shaders)
set(RENDERER_DXC_SPV_ARGS -spirv -fvk-use-scalar-layout -fspv-use-legacy-buffer-matrix-order)
set(RENDERER_BUILTIN_SHADER_FILEPATH ${CMAKE_BINARY_DIR}/builtins/shaders)
set(RENDERER_SHADER_POST_TRANSLATE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/renderer/generated)

function(renderer_compile_builtin_shader SHADER_FILE_PATH)
    set(RENDERER_DXC_STAGE_ARGS)
    set(RENDERER_DXC_STAGE_SPV_ARGS)

    if (${SHADER_FILE_PATH} MATCHES "\.vs\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T vs_6_6")
        set(RENDERER_DXC_STAGE_SPV_ARGS ${RENDERER_DXC_STAGE_SPV_ARGS} -fvk-invert-y -fvk-use-dx-position-w)
    elseif(${SHADER_FILE_PATH} MATCHES "\.ps\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T ps_6_6")
    elseif(${SHADER_FILE_PATH} MATCHES "\.cs\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T cs_6_6")
    elseif(${SHADER_FILE_PATH} MATCHES "\.gs\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T gs_6_6")
        set(RENDERER_DXC_STAGE_SPV_ARGS ${RENDERER_DXC_STAGE_SPV_ARGS} -fvk-invert-y -fvk-use-dx-position-w)
    elseif(${SHADER_FILE_PATH} MATCHES "\.hs\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T hs_6_6")
    elseif(${SHADER_FILE_PATH} MATCHES "\.ds\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T ds_6_6")
        set(RENDERER_DXC_STAGE_SPV_ARGS ${RENDERER_DXC_STAGE_SPV_ARGS} -fvk-invert-y -fvk-use-dx-position-w)
    elseif(${SHADER_FILE_PATH} MATCHES "\.as\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T as_6_6")
    elseif(${SHADER_FILE_PATH} MATCHES "\.ms\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T ms_6_6")
    elseif(${SHADER_FILE_PATH} MATCHES "\.lib\.hlsl")
        set(RENDERER_BUILTIN_SHADER_PROFILE "-T lib_6_6")
    else()
        message(VERBOSE "Skipping shader compile for ${SHADER_FILE_PATH}.")
        return() # No valid shader
    endif()

    get_filename_component(RENDERER_BUILTIN_FILENAME ${SHADER_FILE_PATH} NAME_WLE)

    set(RENDERER_DXC_FULL_ARGS ${RENDERER_BUILTIN_SHADER_PROFILE} ${RENDERER_DXC_ARGS} ${RENDERER_DXC_INCLUDE_ARGS} ${RENDERER_DXC_STAGE_ARGS})
    set(RENDERER_DXC_FULL_SPV_ARGS ${RENDERER_DXC_FULL_ARGS} ${RENDERER_DXC_SPV_ARGS} ${RENDERER_DXC_STAGE_SPV_ARGS})

    add_custom_command(
        OUTPUT ${RENDERER_BUILTIN_SHADER_FILEPATH}/${RENDERER_BUILTIN_FILENAME}.dxil
            ${RENDERER_BUILTIN_SHADER_FILEPATH}/${RENDERER_BUILTIN_FILENAME}.spv
            ${RENDERER_SHADER_POST_TRANSLATE_PATH}/${BUILTIN_SHADER_SOURCE_FILENAME_WLE}.dxil.hpp
            ${RENDERER_SHADER_POST_TRANSLATE_PATH}/${BUILTIN_SHADER_SOURCE_FILENAME_WLE}.spv.hpp
        COMMAND echo Compiling DXIL && dxc.exe ${RENDERER_DXC_FULL_ARGS} -Fo ${RENDERER_BUILTIN_SHADER_FILEPATH}/${RENDERER_BUILTIN_FILENAME}.dxil ${SHADER_FILE_PATH}
        COMMAND echo Compiling SPV && dxc.exe ${RENDERER_DXC_FULL_SPV_ARGS} -Fo ${RENDERER_BUILTIN_SHADER_FILEPATH}/${RENDERER_BUILTIN_FILENAME}.spv ${SHADER_FILE_PATH}
        MAIN_DEPENDENCY ${SHADER_FILE_PATH}
        COMMENT "Compiling builtin shader ${SHADER_FILE_PATH}."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rhi/thirdparty/directx_shader_compiler/bin/x64
        VERBATIM
    )
endfunction()
