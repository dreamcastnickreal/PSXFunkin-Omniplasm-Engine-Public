#!/usr/bin/env python3
"""Fix the indentation in haxesourceparser.py"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'r') as f:
    lines = f.readlines()

# Fix indentation for lines 635-655
new_lines = []
for i, line in enumerate(lines):
    if i == 640:  # Line 642 (0-indexed 641)
        new_lines = [
            '            # Add animations to psych_json\n',
            '            for anim_name, anim_data in char.animations.items():\n',
            '                psych_json["animations"].append({\n',
            '                    "anim": anim_name,\n',
            '                    "name": anim_name.title(),\n',
            '                    "indices": anim_data.get("indices", []),\n',
            '                    "fps": anim_data.get("fps", 24),\n',
            '                    "loop": anim_data.get("loop", False)\n',
            '                })\n',
            '\n',
        ]
        # Replace lines 641-650 (indices 641-650)
        for new_line in reversed(new_lines):
            lines.insert(i, new_line)
        # Remove the old lines (642-650)
        del lines[i+1:i+10]
        break

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'w') as f:
    f.writelines(lines)
print('Fixed!')