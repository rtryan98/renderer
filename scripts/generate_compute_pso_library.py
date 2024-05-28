import argparse
import itertools
import json
import os
from typing import Tuple
from dataclasses import dataclass

output_template_string_hpp = """// Generated file (generate_compute_pso_library.py). Do not edit directly.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ren
{
enum class Shader_Type
{
    Vertex,
    Pixel,
    Compute,
    Task,
    Mesh,
    Library,
    Unknown
};

struct Shader_Metadata
{
    Shader_Type type;
    const char* name;
    const char* entry_point;
    const char* binary_path;
    const char* source_path;
    std::vector<const char*> defines;
};

enum class Shaders
{$SHADERS
};

$SELECT_FUNCTIONS
const auto shader_metadata = std::to_array<Shader_Metadata>({$SHADER_METADATA
});
}
"""

select_shader_function_signature_template_string = "Shaders select_$NAME_shader($ARGS) noexcept;\n"

shader_metadata_template_string = """\n{
    .type = $SHADER_TYPE,
    .name = $NAME,
    .entry_point = $ENTRY_POINT,
    .binary_path = $BINARY_PATH,
    .source_path = $SOURCE_PATH,
    .defines = {$DEFINES}
},"""

select_shader_switch_case_template_string = "\n    case $CASE: $ARG = $TARGET; break;"

select_shader_switch_template_string ="""switch($ARG)
    {$CASES
    }
"""

select_shader_function_body_template_string = """Shaders select_$NAME_shader($ARGS) noexcept
{
    $SWITCHES
    constexpr auto permutation_factors = calc_param_factors(std::to_array({ $PERMUTATION_SIZES}));
    auto permutation_addends = std::to_array<uint32_t>({ $ARG_NAMES});
    auto selection = std::to_underlying($FIRST_ENUM_ELEMENT);
    for (auto i = 0; i < permutation_addends.size(); ++i)
    {
        selection += permutation_factors[i] * permutation_addends[i];
    }
    return static_cast<Shaders>(selection);
}
"""

output_template_string_cpp = """// Generated file (generate_compute_pso_library.py). Do not edit directly.
#include "$HPP_INCLUDE_PATH"

namespace ren
{
constexpr auto calc_param_factors(const auto& permutation_sizes)
{
    auto result = permutation_sizes;
    for (int i = permutation_sizes.size() - 1; i >= 0; --i)
    {
        auto offset = 1;
        for (int j = permutation_sizes.size() - 1; j > i; --j)
        {
            offset *= permutation_sizes[j];
        }
        result[i] = offset;
    }
    return result;
}

$FUNCTIONS
}
"""

@dataclass
class ShaderDefine:
    name: str
    value: str

@dataclass
class ShaderParameter:
    name: str
    type: str
    value_count: int
    swizzle_values: list[int]

@dataclass
class ShaderVariant:
    name: str
    entry_point: str
    binary_path: str
    source_path: str
    defines: list[ShaderDefine]

@dataclass
class Shader:
    name: str
    type: str
    variants: list[ShaderVariant]
    parameters: list[ShaderParameter]

def construct_shader_binary_path_string(binary_base_path: str, name: str, shadertype: str) -> str:
    return binary_base_path + name + "." + shadertype + ".bin"

def process_shader_type(shader_type):
    if shader_type == "vs":
        return "Shader_Type::Vertex"
    elif shader_type == "ps":
        return "Shader_Type::Pixel"
    elif shader_type == "cs":
        return "Shader_Type::Compute"
    elif shader_type == "ts" or shader_type == "as":
        return "Shader_Type::Task"
    elif shader_type == "ms":
        return "Shader_Type::Mesh"
    elif shader_type == "lib":
        return "Shader_Type::Library"
    else:
        return "Shader_Type::Unknown"

def generate_shader_base_variant(shader_json, source_path: str, binary_base_path: str) -> ShaderVariant:
    return ShaderVariant(
        name=shader_json["name"] + "_" + shader_json['shader_type'],
        entry_point=shader_json["entry_point"],
        binary_path=construct_shader_binary_path_string(binary_base_path, shader_json["name"], shader_json['shader_type']),
        source_path=source_path,
        defines=[])

def generate_shader_permutation_index_tuples(shader_json):
    permutation_index_lists = []
    for permutation_group in shader_json["permutation_groups"]:
        if permutation_group["type"] == "int":
            permutation_index_lists.append(list(range(0, len(permutation_group["define_values"][0]))))
        elif permutation_group["type"] == "bool":
            permutation_index_lists.append([0, 1])
    return list(itertools.product(*permutation_index_lists))

def generate_shader_permutation_variants(shader_json, source_path: str, binary_base_path: str) -> list[ShaderVariant]:
    result = []

    for permutation in generate_shader_permutation_index_tuples(shader_json):
        permutation_name = shader_json['name']
        permutation_entry_point = shader_json['entry_point']
        permutation_defines = []

        for i, tuple_value in enumerate(permutation):
            permutation_element = shader_json['permutation_groups'][i]
            if permutation_element['type'] == "int":
                permutation_name += "_" + (permutation_element['name'] if 'use_name' in permutation_element and permutation_element['use_name'] else "") + str(permutation_element['define_values'][0][tuple_value])
                for j, define_name in enumerate(permutation_element['define_names']):
                    permutation_defines.append(ShaderDefine(define_name, str(permutation_element['define_values'][j][tuple_value])))
            elif permutation_element['type'] == "bool":
                define_name = permutation_element['define_name']
                if tuple_value == 1:
                    permutation_name += "_" + permutation_element['name']
                    permutation_defines.append(ShaderDefine(define_name, '1'))
                else:
                    permutation_defines.append(ShaderDefine(define_name, '0'))
            # TODO: add support for entry point permutations
            # elif permutation_element["type"] == "entry_points":
            #     permutation_entry_point = permutation_element['entry_point_names'][tuple_value]

        result.append(ShaderVariant(
            name=permutation_name + "_" + shader_json['shader_type'],
            entry_point=permutation_entry_point,
            binary_path=construct_shader_binary_path_string(binary_base_path, permutation_name, shader_json['shader_type']),
            source_path=source_path,
            defines=permutation_defines))

    return result

def generate_shader_parameters(shader_json) -> list[ShaderParameter]:
    result = []
    for permutation_group in shader_json['permutation_groups']:
        name_mod = ""
        value_count = 2 # default for type == 'bool'
        swizzle_values = []
        if permutation_group['type'] == 'int':
            value_count = len(permutation_group['define_values'][0])
        if permutation_group['type'] == 'int' and 'swizzle_define_values' in permutation_group:
            for value in permutation_group['define_values'][0]:
                swizzle_values.append(value)
        if permutation_group['type'] == 'bool':
            name_mod="use_"
        result.append(ShaderParameter(
            name=name_mod+permutation_group['name'],
            type=permutation_group['type'],
            value_count=value_count,
            swizzle_values=swizzle_values))
    return result

def generate_shader(shader_json, source_path: str, binary_base_path: str) -> Shader:
    result = Shader(name=shader_json['name'], type=shader_json['shader_type'], variants=[], parameters=[])
    has_permutations = 'permutation_groups' in shader_json and len(shader_json['permutation_groups']) > 0
    if has_permutations:
        result.variants = generate_shader_permutation_variants(shader_json, source_path, binary_base_path)
        result.parameters = generate_shader_parameters(shader_json)
    else:
        result.variants = [generate_shader_base_variant(shader_json, source_path, binary_base_path)]
    return result

def process_parameter_type(type: str) -> str:
    if type == "int":
        return "uint32_t"
    elif type == "bool":
        return "bool"
    return ""

def process_select_shader_parameters(parameters: list[ShaderParameter]) -> str:
    params = ""
    for parameter in parameters:
        params += process_parameter_type(parameter.type) + " " + parameter.name + ", "
    return params[:-2]

def process_select_shader_parameter_names(parameters: list[ShaderParameter]) -> str:
    param_names = ""
    for parameter in parameters:
        param_names += parameter.name + ", "
    return param_names[:-2]

def process_select_shader_function_signature(shader: Shader) -> str:
    return select_shader_function_signature_template_string.replace(
        "$NAME", shader.name).replace(
        "$ARGS", process_select_shader_parameters(shader.parameters))

def process_select_shader_permutation_sizes(shader: Shader) -> str:
    result = ""
    for parameter in shader.parameters:
        result += str(parameter.value_count) + ", "
    return result

def process_select_shader_function_switches(shader: Shader):
    switches = ""
    for parameter in shader.parameters:
        if parameter.swizzle_values:
            switch_cases = ""
            for i, value in enumerate(parameter.swizzle_values):
                switch_cases += select_shader_switch_case_template_string.replace(
                    "$ARG", parameter.name).replace(
                    "$CASE", str(value)).replace(
                    "$TARGET", str(i))
            switches += select_shader_switch_template_string.replace(
                "$ARG", parameter.name).replace(
                "$CASES", switch_cases)
    return switches

def process_select_shader_function_body(shader: Shader):
    switches = process_select_shader_function_switches(shader)
    permutation_sizes = process_select_shader_permutation_sizes(shader)
    return select_shader_function_body_template_string.replace(
        "$NAME", shader.name).replace(
        "$ARGS", process_select_shader_parameters(shader.parameters)).replace(
        "$SWITCHES", switches).replace(
        "$FIRST_ENUM_ELEMENT", "Shaders::" + shader.variants[0].name).replace(
        "$PERMUTATION_SIZES", permutation_sizes).replace(
        "$ARG_NAMES", process_select_shader_parameter_names(shader.parameters))

def process_defines(variant: ShaderVariant) -> str:
    defines = ""
    for define in variant.defines:
        defines += "\"" + define.name + "=" + str(define.value) + "\", "
    return defines

def process_shaders(shaders: list[Shader]) -> Tuple[str, str, str, str]:
    shader_select_function_signatures = ""
    shader_select_function_bodies = ""
    enum_values = ""
    metadata = ""

    for shader in shaders:
        if len(shader.parameters) > 0:
            shader_select_function_signatures += process_select_shader_function_signature(shader)
            shader_select_function_bodies += process_select_shader_function_body(shader)
        for variant in shader.variants:
            enum_values += "\n    " + variant.name + ","
            metadata += shader_metadata_template_string.replace(
                "$SHADER_TYPE", process_shader_type(shader.type)).replace(
                "$NAME", "\"" + variant.name + "\"").replace(
                "$ENTRY_POINT", "\"" + variant.entry_point + "\"").replace(
                "$BINARY_PATH", "\"" + variant.binary_path + "\"").replace(
                "$SOURCE_PATH", "\"." + variant.source_path + "\"").replace(
                "$DEFINES", process_defines(variant))

    return shader_select_function_signatures, shader_select_function_bodies, enum_values, metadata

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader-json-files", nargs='+', type=str, required=True)
    parser.add_argument("--binary-base-path", type=str, required=True)
    parser.add_argument("--source-base-path", type=str, required=True)
    parser.add_argument("--outfile-path-wo-ext", type=str, required=True)
    parser.add_argument("--cpp-source-base-path", type=str, required=True)
    args = parser.parse_args()

    shaders = []
    for shader_json_file in args.shader_json_files:
        print(f"Processing {shader_json_file}.")
        shader_json = json.load(open(shader_json_file))
        shader_source_path = shader_json_file.replace(args.source_base_path, "").replace(".json", ".hlsl")
        shader_binary_base_path = args.binary_base_path + os.path.dirname(shader_source_path) + "/"
        shaders.append(generate_shader(shader_json, shader_source_path, shader_binary_base_path))
    function_signatures, function_bodies, enum_values, metadata = process_shaders(shaders)

    output_string_hpp = output_template_string_hpp.replace(
        "$SHADERS", enum_values).replace(
        "$SHADER_METADATA", metadata).replace(
        "$SELECT_FUNCTIONS", function_signatures)
    output_hpp = args.outfile_path_wo_ext + ".hpp"

    output_cpp = args.outfile_path_wo_ext + ".cpp"
    output_string_cpp = output_template_string_cpp.replace(
        "$HPP_INCLUDE_PATH", output_hpp.replace(args.cpp_source_base_path + "/", "")).replace(
        "$FUNCTIONS", function_bodies)
    with open(output_cpp, 'w') as outfile:
        outfile.write(output_string_cpp)
        outfile.close()
    with open(output_hpp, 'w') as outfile:
        outfile.write(output_string_hpp)
        outfile.close()
