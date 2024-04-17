import argparse
import itertools
import json
from typing import Tuple

output_template_string_hpp = """// Generated file (generate_compute_pso_library.py). Do not edit directly.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ren
{
struct Compute_Pipeline_Metadata
{
    const char* name;
    const char* binary_path;
    const char* source_path;
    std::vector<const char*> defines;
};

enum class Compute_Pipelines
{$COMPUTE_PIPELINES
};

$SELECT_FUNCTIONS
const auto compute_pipeline_metadata = std::to_array<Compute_Pipeline_Metadata>({$COMPUTE_PIPELINE_METADATA
});
}
"""

select_shader_template_string = "Compute_Pipelines select_$NAME_shader($ARGS) noexcept;\n"

compute_pipeline_template_string = """\n{
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

select_shader_template_string_cpp = """Compute_Pipelines select_$NAME_shader($ARGS) noexcept
{
    $SWITCHES
    constexpr auto permutation_factors = calc_param_factors(std::to_array({ $PERMUTATION_SIZES}));
    auto permutation_addends = std::to_array<uint32_t>({ $ARG_NAMES});
    auto selection = std::to_underlying($FIRST_ENUM_ELEMENT);
    for (auto i = 0; i < permutation_addends.size(); ++i)
    {
        selection += permutation_factors[i] * permutation_addends[i];
    }
    return static_cast<Compute_Pipelines>(selection);
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

def generate_permutations(shader, source_path: str, binary_base_path: str) -> Tuple[str, str, str, str, str, str, str]:
    permutation_index_lists = []
    for permutation_group in shader["permutation_groups"]:
        if permutation_group["type"] == "int":
            permutation_index_lists.append(list(range(0, len(permutation_group["define_values"][0]))))
        elif permutation_group["type"] == "bool":
            permutation_index_lists.append([0, 1])
    permutation_index_tuples = list(itertools.product(*permutation_index_lists))

    name_list = ""
    metadata = ""
    fn_args = ""
    fn_arg_names = ""
    fn_switches = ""
    first_element = ""
    permutation_sizes = ""

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
        name = shader["name"]
        defines = ""
        for i, tuple_value in enumerate(permutation):
            permutation_element = shader["permutation_groups"][i]
            if permutation_element["type"] == "int":
                name += "_" + (permutation_element["name"] if "use_name" in permutation_element and permutation_element["use_name"] else "") + str(permutation_element["define_values"][0][tuple_value])
                for j, define_name in enumerate(permutation_element["define_names"]):
                    defines += f"\"{define_name}={permutation_element['define_values'][j][tuple_value]}\", "
            elif permutation_element["type"] == "bool":
                if tuple_value == 1:
                    name += "_" + permutation_element["name"]
                    defines += f"\"{permutation_element['define_name']}=1\", "
                else:
                    defines += f"\"{permutation_element['define_name']}=0\", "
        binary_path = binary_base_path + name + ".bin"
        if first_element == "":
            first_element = name

        name_list += "\n    " + name + ","
        metadata += compute_pipeline_template_string.replace(
            "$NAME", "\"" + name + "\"").replace(
            "$BINARY_PATH", "\"" + binary_path + "\"").replace(
            "$SOURCE_PATH", "\"" + source_path + "\"").replace(
            "$DEFINES", defines)
    
    return name_list, metadata, shader["name"], fn_args[:-2], fn_switches, first_element, permutation_sizes, fn_arg_names

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader-json-files", nargs='+', type=str, required=True)
    parser.add_argument("--binary-base-path", type=str, required=True)
    parser.add_argument("--source-base-path", type=str, required=True)
    parser.add_argument("--outfile-path-wo-ext", type=str, required=True)
    parser.add_argument("--cpp-source-base-path", type=str, required=True)
    args = parser.parse_args()

    compute_pipelines_string = ""
    compute_pipeline_metadata_string = ""
    compute_pipeline_select_shader_string = ""
    compute_pipeline_select_shader_cpp_string = ""

    for shader_json_file in args.shader_json_files:
        json_object = json.load(open(shader_json_file))
        shader_source_path = shader_json_file.replace(args.source_base_path, "").replace(".json", ".hlsl")
        pipeline_string, metadata, name, fn_args, fn_switches, first_element, permutation_sizes, fn_arg_names = generate_permutations(json_object, "." + shader_source_path, args.binary_base_path + shader_source_path.replace(".hlsl", ""))
        compute_pipelines_string += pipeline_string
        compute_pipeline_metadata_string += metadata
        compute_pipeline_select_shader_string += select_shader_template_string.replace(
            "$NAME", name).replace(
            "$ARGS", fn_args)
        compute_pipeline_select_shader_cpp_string += select_shader_template_string_cpp.replace(
            "$NAME", name).replace(
            "$ARGS", fn_args).replace(
            "$SWITCHES", fn_switches).replace(
            "$FIRST_ENUM_ELEMENT", "Compute_Pipelines::" + first_element).replace(
            "$PERMUTATION_SIZES", permutation_sizes).replace(
            "$ARG_NAMES", fn_arg_names)

    output_string_hpp = output_template_string_hpp.replace(
        "$COMPUTE_PIPELINES", compute_pipelines_string).replace(
        "$COMPUTE_PIPELINE_METADATA", compute_pipeline_metadata_string).replace(
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
