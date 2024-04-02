import sys
import os
from pathlib import Path

file = sys.argv[1]
with open(file, 'rb') as input:
    content = input.read().hex()
    hex_array = ["0x" + content[i:i + 2] for i in range(0, len(content), 2)]
    out_hpp_content  = "#pragma once\n"
    out_hpp_content += "#include <array>\n"
    out_hpp_content += "namespace ren\n"
    out_hpp_content += "{\n"
    out_hpp_content += f"auto {sys.argv[3]} = std::to_array<unsigned char>({'{'}"
    for hex in hex_array:
        out_hpp_content += hex + ", "
    out_hpp_content += "});\n"
    out_hpp_content += "}\n"
    if not os.path.exists(os.path.dirname(sys.argv[2])):
        Path(os.path.dirname(sys.argv[2])).mkdir(parents=True, exist_ok=True)
    with open(sys.argv[2], 'w') as output:
        output.write(out_hpp_content)
