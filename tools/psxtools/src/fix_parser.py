#!/usr/bin/env python3
"""Fix the haxesourceparser.py file"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'r') as f:
    content = f.read()

# Append the missing function body
body = '''
    parser = HaxeSourceParser(mod_path)
    
    print(f"Parsing Character.hx from {mod_path}...")
    characters = parser.parse_character_hx()
    print(f"Found {len(characters)} characters")
    
    print(f"Parsing PlayState.hx from {mod_path}...")
    stages = parser.parse_playstate_hx()
    print(f"Found {len(stages)} stage configurations")
    
    # Export to .chr.json files
    char_output = os.path.join(output_dir, "characters")
    stage_output = os.path.join(output_dir, "stages")
    
    parser.export_characters_to_chrjson(char_output, characters)
    parser.export_stages_to_chrjson(stage_output, stages)
    
    print(f"\nExport complete!")
    print(f"Character configs: {char_output}")
    print(f"Stage configs: {stage_output}")
    print(f"\nNow run PSXFunkin Character Maker on these .chr.json files to generate C/H/PNG files.")
'''

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'a') as f:
    f.write(body)
print('Added function body!')