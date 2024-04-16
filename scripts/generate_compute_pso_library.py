import sys
import json

input_json_string = sys.argv[1]
outfile_path = sys.argv[2]

output_template_string = """// Generated file (generate_compute_pso_library.py). Do not edit directly.
#pragma once

#include <array>
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

const auto compute_pipeline_metadata = std::to_array<Compute_Pipeline_Metadata>({$COMPUTE_PIPELINE_METADATA
});
}
"""

compute_pipeline_template_string = """\n{
    .name = $NAME,
    .binary_path = $BINARY_PATH,
    .source_path = $SOURCE_PATH,
    .defines = {$DEFINES}
},"""

compute_pipelines_string = ""
compute_pipeline_metadata_string = ""

json_object = json.loads(input_json_string)

for shader in json_object:
    original_name = shader["name"]
    binary_path = shader["binary_path"]
    source_path = shader["source_path"]
    for permutation in shader["permutations"]:
        permutation_name = original_name
        permutation_defines = ""
        if "name" in permutation:
            permutation_name += "_" + permutation["name"]
        if "defines" in permutation:
            for define in permutation["defines"]:
                for key, value in define.items():
                    permutation_defines += "\"" + key + "=" + str(value) + "\", "
        compute_pipelines_string += "\n    " + permutation_name + ","
        compute_pipeline_metadata_string += compute_pipeline_template_string.replace(
            "$NAME", "\"" + permutation_name + "\"").replace(
            "$BINARY_PATH", "\"" + binary_path + "\"").replace(
            "$SOURCE_PATH", "\"" + source_path + "\"").replace(
            "$DEFINES", permutation_defines)

output_string = output_template_string.replace(
    "$COMPUTE_PIPELINES", compute_pipelines_string).replace(
    "$COMPUTE_PIPELINE_METADATA", compute_pipeline_metadata_string)

with open(outfile_path, 'w') as outfile:
    outfile.write(output_string)
