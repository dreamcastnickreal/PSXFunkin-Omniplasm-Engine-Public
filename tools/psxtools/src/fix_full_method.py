#!/usr/bin/env python3
"""Fix the haxesourceparser.py file properly"""

with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'rb') as f:
    content = f.read()

# Normalize line endings
content = content.replace(b'\r\n', b'\n').replace(b'\r', b'\n')

# Find the export_characters_to_chrjson method and fix it completely
# Find the start of the method
start = content.find(b'def export_characters_to_chrjson(self, output_dir: str, characters: List[CharacterDefinition]):')
if start == -1:
    print("Could not find export_characters_to_chrjson method")
else:
    # Find the end of the method (next method or end of class)
    next_method = content.find(b'\n    def ', start + 1)
    if next_method == -1:
        next_method = len(content)
    
    # Replace the entire method
    new_method = b'''    def export_characters_to_chrjson(self, output_dir: str, characters: List[CharacterDefinition]):
        """Export character definitions to .chr.json format for PSXFunkin Character Maker."""
        os.makedirs(output_dir, exist_ok=True)
        
        for char in characters:
            # Determine the base name from the atlas path (use the last part of the path)
            atlas_base_name = os.path.basename(char.atlas_path)
            
            # Write the PsychEngine JSON file - place in characters root folder (where CharacterMaker expects it)
            psych_json_dir = os.path.join(output_dir, "..", "characters")
            os.makedirs(psych_json_dir, exist_ok=True)
            psych_json_path = os.path.join(psych_json_dir, f"{atlas_base_name}.json")
            
            psych_json = {
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
                json.dump(psych_json, f, indent=2)
            
            # Copy the XML and PNG files to the characters root folder (where CharacterMaker expects them)
            import shutil
            xml_src = self.resolve_image_path(char.atlas_path + ".xml")
            png_src = self.resolve_image_path(char.atlas_path + ".png")
            if xml_src:
                shutil.copy2(xml_src, os.path.join(psych_json_dir, os.path.basename(xml_src)))
            if png_src:
                shutil.copy2(png_src, os.path.join(psych_json_dir, os.path.basename(png_src)))
            
            # Also copy to character's subfolder (for reference)
            char_dir = os.path.join(output_dir, char.name)
            os.makedirs(char_dir, exist_ok=True)
            if xml_src:
                shutil.copy2(xml_src, os.path.join(char_dir, os.path.basename(xml_src)))
            if png_src:
                shutil.copy2(png_src, os.path.join(char_dir, os.path.basename(png_src)))
            
            # fnf_json should be just the filename (in characters root)
            fnf_json = f"{atlas_base_name}.json"
            
            chr_data = {
                "name": char.name,
                "fnf_json": fnf_json,
                "json_path": f"{atlas_base_name}.json",
                "output_path": os.path.join(output_dir, char.name),
                "bpp": 4,
                "vram_position": "opponent",
                "scale_mode": "0.25",
                "custom_scale": 1.0,
                "deduplicate_frames": True,
                "skip_nth_frame": 0,
                "is_stage": False,
                "is_player": char.is_player,
                "generate_ch_files": True,
                "health_bar_color": 4365946819,
                "focus_offset": [-100, -100],
                "focus_zoom": [125, 100],
                "character_size": [100, 100],
                "flip_x": char.flip_x,
                "unrotate_sprites": True,
                "optimize_names": True,
                "psxfunkin_scale": 0.25,
                "animations": []
            }
            
            # Add animations
            for anim_name, anim_data in char.animations.items():
                anim_entry = {
                    "fnf_name": anim_name,
                    "name": anim_name.title(),
                    "indices": anim_data.get("indices", []),
                    "fps": anim_data.get("fps", 24),
                    "loop": anim_data.get("loop", False),
                    "psxfunkin_name": anim_name.title()
                }
                chr_data["animations"].append(anim_entry)
            
            # Write .chr.json in characters root folder (where CharacterMaker expects it)
            os.makedirs(os.path.join(output_dir, "..", "characters"), exist_ok=True)
            output_path = os.path.join(output_dir, "..", "characters", f"{char.name}.chr.json")
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(chr_data, f, indent=2)
            
            print(f"Exported character: {char.name} -> {output_path}")'''

    # Find the end of the current method
    method_start = content.find(b'    def export_characters_to_chrjson(self, output_dir: str, characters: List[CharacterDefinition]):')
    if method_start == -1:
        print("Could not find method")
    else:
        # Find the next method
        next_method = content.find(b'\n    def ', method_start + 1)
        if next_method == -1:
            next_method = len(content)
        
        # Replace the method
        new_content = content[:method_start] + new_method + content[next_method:]
        with open(r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src\haxesourceparser.py', 'wb') as f:
            f.write(new_content)
        print('Fixed!')