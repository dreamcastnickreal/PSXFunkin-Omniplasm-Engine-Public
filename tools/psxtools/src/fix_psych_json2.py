#!/usr/bin/env python3
"""Fix the indentation in haxesourceparser.py"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'rb') as f:
    content = f.read()

# Find the psych_json section
start = content.find(b'psych_json = {')
if start == -1:
    print("Could not find psych_json = {")
else:
    # Find the end of this section - the "with open(psych_json_path" line
    end = content.find(b'with open(psych_json_path', start)
    if end == -1:
        print("Could not find end")
    else:
        new_section = b'''            psych_json = {
                "image": char.atlas_path + ".png",
                "scale": char.scale[0] if char.scale[0] == char.scale[1] else char.scale[0],
                "animations": []
            }

            # Add animations to psych_json
            for anim_name, anim_data in char.animations.items():
                psych_json["animations"].append({
                    "anim": anim_name,
                    "name": anim_name.title(),
                    "indices": anim_data.get("indices", []),
                    "fps": anim_data.get("fps", 24),
                    "loop": anim_data.get("loop", False)
                })

            with open(psych_json_path, 'w', encoding='utf-8') as f:
'''
        new_content = content[:start] + new_section + content[end:]
        with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'wb') as f:
            f.write(new_content)
        print('Fixed!')