import argparse
import itertools
import json
import os
from typing import list, Tuple
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

select_shader_template_string = "Shaders select_$NAME_shader($ARGS) noexcept;\n"

compute_pipeline_template_string = """\n{
    .type = $SHADER_TYPE,
    .name = $NAME,
    .binary_path = $BINARY_PATH,
    .source_path = $SOURCE_PATH,
    .defines = {$DEFINES}
},"""

select_shader_switch_case_template_string_cpp = "\n    case $CASE: $ARG = $TARGET; break;"

select_shader_switch_template_string_cpp ="""switch($ARG)
    {$CASES
    }
"""

select_shader_template_string_cpp = """Shaders select_$NAME_shader($ARGS) noexcept
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
class ShaderParameter:
    parameter_names: list[str]
    parameter_types: list[str]

@dataclass
class ShaderVariant:
    name: str
    entry_point: str
    binary_path: str
    source_path: str
    defines: list[str]

@dataclass
class Shader:
    name: str
    type: str
    variants: list[ShaderVariant]
    parameters: list[ShaderParameter]

def construct_shader_name_string(name: str) -> str:
    return "\n    " + name + ","

def construct_shader_binary_path_string(binary_base_path: str, name: str) -> str:
    return binary_base_path + name + ".bin"

def construct_metadata_string(shader_type: str, entry_point: str, name: str, binary_path: str, source_path: str, defines: str) -> str:
    return compute_pipeline_template_string.replace(
                "$SHADER_TYPE", decode_shader_type(shader_type)).replace(
                "$ENTRY_POINT", entry_point).replace(
                "$NAME", "\"" + name + "\"").replace(
                "$BINARY_PATH", "\"" + binary_path + "\"").replace(
                "$SOURCE_PATH", "\"" + source_path + "\"").replace(
                "$DEFINES", defines)

def decode_shader_type(shader_type):
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

def generate_permutations(shader, source_path: str, binary_base_path: str) -> Tuple[str, str, str, str, str, str, str, bool]:
    has_permutations = "permutation_groups" in shader and len(shader["permutation_groups"]) > 0
    permutation_count = 1
    name_list = ""
    metadata = ""
    fn_args = ""
    fn_arg_names = ""
    fn_switches = ""
    first_element = ""
    permutation_sizes = ""

    shader_type = shader["shader_type"]
    entry_point = shader["entry_point"]
    name = shader["name"]

    if has_permutations:
        permutation_index_lists = []
        for permutation_group in shader["permutation_groups"]:
            if permutation_group["type"] == "int":
                permutation_index_lists.append(list(range(0, len(permutation_group["define_values"][0]))))
            elif permutation_group["type"] == "bool":
                permutation_index_lists.append([0, 1])
        permutation_index_tuples = list(itertools.product(*permutation_index_lists))
        permutation_count = len(permutation_index_tuples)

        for permutation_group in shader["permutation_groups"]:
            fn_arg_names += permutation_group['name'] + ", "
            if permutation_group["type"] == "int":
                fn_args += f"uint32_t {permutation_group['name']}, "
                if "swizzle_define_values" in permutation_group and permutation_group["swizzle_define_values"]:
                    switch_cases = ""
                    for i, define_value in enumerate(permutation_group["define_values"][0]):
                        switch_cases += select_shader_switch_case_template_string_cpp.replace("$CASE", str(define_value)).replace("$ARG", permutation_group["name"]).replace("$TARGET", str(i))
                    fn_switches += select_shader_switch_template_string_cpp.replace("$ARG", permutation_group["name"]).replace("$CASES", switch_cases)
                    permutation_sizes += f"{len(permutation_group['define_values'][0])}, "
            elif permutation_group["type"] == "bool":
                fn_args += f"bool {permutation_group['name']}, "
                permutation_sizes += "2, "

        for permutation in permutation_index_tuples:
            permutation_name = shader["name"]
            permutation_entry_point = entry_point
            defines = ""
            for i, tuple_value in enumerate(permutation):
                permutation_element = shader["permutation_groups"][i]
                if permutation_element["type"] == "int":
                    permutation_name += "_" + (permutation_element["name"] if "use_name" in permutation_element and permutation_element["use_name"] else "") + str(permutation_element["define_values"][0][tuple_value])
                    for j, define_name in enumerate(permutation_element["define_names"]):
                        defines += f"\"{define_name}={permutation_element['define_values'][j][tuple_value]}\", "
                elif permutation_element["type"] == "bool":
                    if tuple_value == 1:
                        permutation_name += "_" + permutation_element["name"]
                        defines += f"\"{permutation_element['define_name']}=1\", "
                    else:
                        defines += f"\"{permutation_element['define_name']}=0\", "
                # TODO: add support for entry point permutations
                # elif permutation_element["type"] == "entry_points":
                #     permutation_entry_point = permutation_element['entry_point_names'][tuple_value]
            if first_element == "":
                first_element = permutation_name

            name_list += construct_shader_name_string(permutation_name)
            metadata += construct_metadata_string(shader_type, permutation_entry_point, name, construct_shader_binary_path_string(binary_base_path, permutation_name), source_path, defines)
    else:
        name_list += construct_shader_name_string(name)
        metadata += construct_metadata_string(shader_type, entry_point, name, construct_shader_binary_path_string(binary_base_path, name), source_path, "")

    has_permutations = has_permutations and permutation_count > 1
    return name_list, metadata, shader["name"], fn_args[:-2], fn_switches, first_element, permutation_sizes, fn_arg_names, has_permutations

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader-json-files", nargs='+', type=str, required=True)
    parser.add_argument("--binary-base-path", type=str, required=True)
    parser.add_argument("--source-base-path", type=str, required=True)
    parser.add_argument("--outfile-path-wo-ext", type=str, required=True)
    parser.add_argument("--cpp-source-base-path", type=str, required=True)
    args = parser.parse_args()

    Shaders_string = ""
    Shader_Metadata_string = ""
    compute_pipeline_select_shader_string = ""
    compute_pipeline_select_shader_cpp_string = ""

    for shader_json_file in args.shader_json_files:
        print(f"Processing {shader_json_file}.")
        json_object = json.load(open(shader_json_file))
        shader_source_path = shader_json_file.replace(args.source_base_path, "").replace(".json", ".hlsl")
        pipeline_string, metadata, name, fn_args, fn_switches, first_element, permutation_sizes, fn_arg_names, has_permutations = generate_permutations(json_object, "." + shader_source_path, args.binary_base_path + os.path.dirname(shader_source_path) + "/")
        Shaders_string += pipeline_string
        Shader_Metadata_string += metadata
        if has_permutations:
            compute_pipeline_select_shader_string += select_shader_template_string.replace(
                "$NAME", name).replace(
                "$ARGS", fn_args)
            compute_pipeline_select_shader_cpp_string += select_shader_template_string_cpp.replace(
                "$NAME", name).replace(
                "$ARGS", fn_args).replace(
                "$SWITCHES", fn_switches).replace(
                "$FIRST_ENUM_ELEMENT", "Shaders::" + first_element).replace(
                "$PERMUTATION_SIZES", permutation_sizes).replace(
                "$ARG_NAMES", fn_arg_names)

    output_string_hpp = output_template_string_hpp.replace(
        "$SHADERS", Shaders_string).replace(
        "$SHADER_METADATA", Shader_Metadata_string).replace(
        "$SELECT_FUNCTIONS", compute_pipeline_select_shader_string)
    output_hpp = args.outfile_path_wo_ext + ".hpp"

    output_cpp = args.outfile_path_wo_ext + ".cpp"
    output_string_cpp = output_template_string_cpp.replace(
        "$HPP_INCLUDE_PATH", output_hpp.replace(args.cpp_source_base_path + "/", "")).replace(
        "$FUNCTIONS", compute_pipeline_select_shader_cpp_string)
    with open(output_cpp, 'w') as outfile:
        outfile.write(output_string_cpp)
        outfile.close()
    with open(output_hpp, 'w') as outfile:
        outfile.write(output_string_hpp)
        outfile.close()
