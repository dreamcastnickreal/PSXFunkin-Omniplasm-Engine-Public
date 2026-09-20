#!/usr/bin/env python3
"""Fix the missing psych_json definition"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'r') as f:
    lines = f.readlines()

# Find the line with "with open(psych_json_path"
for i, line in enumerate(lines):
    if 'with open(psych_json_path' in line:
        print(f'Found at line {i+1}: {repr(line)}')
        # Insert before this line
        indent = '            '
        new_lines = [
            '            psych_json = {\n',
            '                "image": char.atlas_path + ".png",\n',
            '                "scale": char.scale[0] if char.scale[0] == char.scale[1] else char.scale[0],\n',
            '                "animations": []\n',
            '            }\n',
            '\n',
        ]
        for new_line in reversed(new_lines):
            lines.insert(i, new_line)
        break

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'w') as f:
    f.writelines(lines)
print('Fixed!')