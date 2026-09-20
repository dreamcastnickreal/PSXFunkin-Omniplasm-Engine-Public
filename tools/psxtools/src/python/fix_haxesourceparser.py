#!/usr/bin/env python3
"""Fix the haxesourceparser.py file"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'r') as f:
    content = f.read()

# Find the last occurrence of the function definition
idx = content.rfind('def export_mod_for_psxfunkin(mod_path: str, output_dir: str):')
if idx >= 0:
    # Find the end of the function definition line
    end_idx = content.find('\n', idx)
    # Replace from the function definition to end with proper implementation
    new_content = content[:end_idx + 1] + '''
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

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        print("Usage: python haxesourceparser.py <mod_path> <output_dir>")
        sys.exit(1)
        
    mod_path = sys.argv[1]
    output_dir = sys.argv[2]
    export_mod_for_psxfunkin(mod_path, output_dir)
'''
    
    with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'w') as f:
        f.write(content[:idx] + new_end)
    print('Fixed!')