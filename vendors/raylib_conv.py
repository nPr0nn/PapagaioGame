#!/usr/bin/env python3
import re
import os
import sys

"""
My take on an snake cased raylib API 
This is a slight modification over @keyle code 
from here https://github.com/keyle/raylib-converter
"""

# Custom type mappings
type_mappings = {
    "Vector2": "Vec2",
    "Vector3": "Vec3",
    "Vector4": "Vec4",
    "Quaternion": "Quat",
    "Matrix": "Mat4",
    "Rectangle": "Rect",
}


def camel_to_snake(name):
    name = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
    name = re.sub(r"([a-z])([A-Z])", r"\1_\2", name)
    name = re.sub(r"([a-z])([0-9][A-Z])", r"\1_\2", name)
    name = name.lower().replace("vector_2", "vector2_")
    name = name.lower().replace("vector_3", "vector3_")
    return name


def process_header(file_content, file, header_name, func_prefix):
    new_header = """#ifndef {}\n#define {}\n\n #include "{}"\n\n""".format(
        header_name, header_name, file
    )

    new_header += "// Types\n"
    for original_type, new_type in type_mappings.items():
        macro = f"#define {new_type} {original_type}\n"
        new_header += macro

    new_header += "\n// Functions\n"
    # functions
    functions_pattern = re.findall(
        r"(?:RLAPI|RMAPI|extern)\s+((?:const\s+|unsigned\s+)*[A-Za-z0-9]+\s*[*]*\s+)([A-Z][a-zA-Z0-9]+)\(([\s\S]+?)\).*",
        file_content,
    )
    for return_type, func_name, params in functions_pattern:
        snake_func = func_prefix + camel_to_snake(func_name)
        macro = f"#define {snake_func} {func_name}\n"
        new_header += macro

    new_header += """\n#endif // {}\n""".format(header_name)
    return new_header


def convert():
    # Prompt user for raylib folder
    if len(sys.argv) > 1:
        raylib_folder = sys.argv[1]
    else:
        raylib_folder = input("Enter the path to the raylib folder: ").strip()

    # Ensure the path exists
    if not os.path.isdir(raylib_folder):
        print(f"Error: Directory '{raylib_folder}' does not exist.")
        sys.exit(1)

    # Define file paths
    raylib_h = os.path.join(raylib_folder, "raylib.h")
    raymath_h = os.path.join(raylib_folder, "raymath.h")
    sc_raylib_h = os.path.join(raylib_folder, "sc_raylib.h")
    sc_raymath_h = os.path.join(raylib_folder, "sc_raymath.h")

    # Check if files exist
    if not os.path.isfile(raylib_h):
        print(f"Error: 'raylib.h' not found in '{raylib_folder}'.")
        sys.exit(1)
    if not os.path.isfile(raymath_h):
        print(f"Error: 'raymath.h' not found in '{raylib_folder}'.")
        sys.exit(1)

    # Process raylib.h
    with open(raylib_h, "r") as file:
        original_header = file.read()
    processed_header = process_header(original_header, "raylib.h", "SC_RAYLIB_H", "rl_")
    with open(sc_raylib_h, "w") as file:
        file.write(processed_header)
    print(f"Generated: {sc_raylib_h}")

    # Process raymath.h
    with open(raymath_h, "r") as file:
        original_header = file.read()
    processed_header = process_header(original_header, "raymath.h", "SC_RAYMATH_H", "")
    with open(sc_raymath_h, "w") as file:
        file.write(processed_header)
    print(f"Generated: {sc_raymath_h}")


if __name__ == "__main__":
    convert()
