#!/usr/bin/env python3
"""
End-to-end pipeline: Parse Haxe source -> Generate .chr.json -> Build PSXFunkin assets

This script combines:
1. Haxe source parsing (Character.hx, PlayState.hx) -> .chr.json files
2. PSXFunkin Character Maker processing -> C/H/PNG/TIM files

Features:
- Automatic fallback scaling: 0.25 -> 0.125 -> 0.0625
- Pixel character detection (uses 1.0 scale)
- Processes both characters (Character.hx) and stages (PlayState.hx)
"""

import os
import sys
import json
import glob
import shutil
import argparse

# Add the PSXFunkin Character Maker to path
MAKER_ROOT = r'C:\Users\nicho\Downloads\PSXFunkin-CharacterMaker-main\PSXFunkin-CharacterMaker-main\src'
sys.path.insert(0, MAKER_ROOT)

from haxesourceparser import HaxeSourceParser, export_mod_for_psxfunkin


def parse_and_build(mod_root, output_dir, scale_mode='auto', bpp=4, psxfunkin_scale=0.25):
    """
    Complete pipeline: Parse Haxe source -> Generate .chr.json -> Build PSXFunkin assets
    
    Args:
        mod_root: Path to mod folder containing Character.hx, PlayState.hx, assets/
        output_dir: Where to put final PSXFunkin assets (C/H/PNG/TIM files)
        scale_mode: Scale mode ('auto', '0.25', '0.125', '0.0625', 'full', 'custom')
        bpp: Bits per pixel (4 or 8)
        psxfunkin_scale: Base PSXFunkin scale (default 0.25)
    """
    
    print(f"\n{'='*60}")
    print(f"PSXFUNKIN SOURCE TO ASSET PIPELINE")
    print(f"{'='*60}")
    print(f"Mod root:     {mod_root}")
    print(f"Output:       {output_dir}")
    print(f"Scale mode:   {scale_mode}")
    print(f"BPP:          {bpp}")
    print(f"PSXFunkin scale: {psxfunkin_scale}")
    print(f"{'='*60}\n")
    
    # ============================================================
    # STEP 1: Parse Haxe source and generate .chr.json files
    # ============================================================
    print(f"\n[1/3] Parsing Haxe source from: {mod_root}")
    parser = HaxeSourceParser(mod_root)
    
    # Parse characters from Character.hx
    characters = parser.parse_character_hx()
    print(f"  Found {len(characters)} characters")
    for c in characters:
        print(f"    - {c.name} (scale: {c.scale}, atlas: {c.atlas_path})")
    
    # Parse stages from PlayState.hx
    stages = parser.parse_playstate_hx()
    print(f"  Found {len(stages)} stage configurations")
    for s, items in stages.items():
        print(f"    - {s}: {len(items)} background items")
    
    # Export to .chr.json files
    chr_output = os.path.join(output_dir, '_chr_json')
    print(f"\n  Exporting .chr.json files to: {chr_output}")
    export_mod_for_psxfunkin(mod_root, chr_output)
    print(f"  Exported .chr.json files")
    
    # ============================================================
    # STEP 2: Update .chr.json files with desired scale settings
    # ============================================================
    print(f"\n[2/3] Configuring scale mode: {scale_mode}")
    
    for root, dirs, files in os.walk(chr_output):
        for file in files:
            if file.endswith('.chr.json'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r') as f:
                    data = json.load(f)
                data['scale_mode'] = scale_mode
                data['bpp'] = bpp
                data['psxfunkin_scale'] = psxfunkin_scale
                with open(filepath, 'w') as f:
                    json.dump(data, f, indent=2)
    
    print(f"  Updated {len(glob.glob(os.path.join(chr_output, '**', '*.chr.json'), recursive=True))} .chr.json files")
    
    # ============================================================
    # STEP 3: Run PSXFunkin Character Maker
    # ============================================================
    print(f"\n[3/3] Building PSXFunkin assets (C/H/PNG/TIM)...")
    
    # The main.py processes all .chr.json files in the current directory recursively
    # We need to run it from the _chr_json directory
    
    maker_src = os.path.join(MAKER_ROOT)
    old_cwd = os.getcwd()
    
    try:
        # Change to the _chr_json directory so glob finds our .chr.json files
        os.chdir(chr_output)
        
        # Import and run the main processing logic
        # We need to execute main.py's logic - it runs on import if __name__ == "__main__"
        # So we'll exec it
        
        print(f"  Running PSXFunkin Character Maker from: {chr_output}")
        print(f"  (This may take a while...)\n")
        
        with open(os.path.join(maker_src, 'main.py'), 'r') as f:
            code = f.read()
        
        # Execute in a namespace with __name__ == "__main__"
        namespace = {
            '__name__': '__main__',
            '__file__': os.path.join(maker_src, 'main.py'),
        }
        exec(code, namespace)
        
        print(f"\n  PSXFunkin Character Maker completed!")
        
    except Exception as e:
        print(f"\n  ERROR during asset building: {e}")
        import traceback
        traceback.print_exc()
        raise
    finally:
        os.chdir(old_cwd)
    
    print(f"\n{'='*60}")
    print(f"PIPELINE COMPLETE!")
    print(f"Assets generated in: {output_dir}")
    print(f"{'='*60}\n")


def parse_only(mod_root, output_dir):
    """Only parse and generate .chr.json files, skip asset building."""
    parser = HaxeSourceParser(mod_root)
    
    print(f"Parsing Character.hx from {mod_root}...")
    characters = parser.parse_character_hx()
    print(f"Found {len(characters)} characters")
    
    print(f"Parsing PlayState.hx from {mod_root}...")
    stages = parser.parse_playstate_hx()
    print(f"Found {len(stages)} stage configurations")
    
    # Export to .chr.json files
    chr_output = os.path.join(output_dir, '_chr_json')
    export_mod_for_psxfunkin(parser, chr_output)
    
    print(f"\nGenerated .chr.json files in: {chr_output}")
    print(f"Run with --build to process these into PSXFunkin assets.")


def main():
    ap = argparse.ArgumentParser(
        description='Parse Haxe source (Character.hx, PlayState.hx) and build PSXFunkin assets',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Full pipeline: parse Haxe source and build PSXFunkin assets with auto-scaling
  python parse_and_build.py "C:/path/to/mod" "C:/output/assets" --scale auto
  
  # Parse only (generate .chr.json without building assets)
  python parse_and_build.py "C:/path/to/mod" "C:/output" --parse-only
  
  # Build with specific scale
  python parse_and_build.py "C:/path/to/mod" "C:/output/assets" --scale 0.125 --bpp 4
  
  # Custom PSXFunkin base scale
  python parse_and_build.py "C:/path/to/mod" "C:/output/assets" --psxfunkin-scale 0.125
        """
    )
    
    ap.add_argument('mod_root', help='Path to mod folder containing Character.hx, PlayState.hx, assets/')
    ap.add_argument('output_dir', help='Output directory for PSXFunkin assets (C/H/PNG/TIM)')
    ap.add_argument('--scale', default='auto', choices=['auto', '0.25', '0.125', '0.0625', 'full', 'custom'],
                    help='Scale mode for PSXFunkin (default: auto - tries 0.25, 0.125, 0.0625)')
    ap.add_argument('--bpp', type=int, default=4, choices=[4, 8], help='Bits per pixel (default: 4)')
    ap.add_argument('--psxfunkin-scale', type=float, default=0.25, 
                    help='Base PSXFunkin scale factor (default: 0.25)')
    ap.add_argument('--parse-only', action='store_true', 
                    help='Only parse and generate .chr.json, skip asset building')
    
    args = ap.parse_args()
    
    if not os.path.isdir(args.mod_root):
        print(f"Error: Mod root '{args.mod_root}' is not a valid directory")
        sys.exit(1)
    
    # Check for required source files
    source_dir = os.path.join(args.mod_root, 'source')
    if not os.path.isfile(os.path.join(source_dir, 'Character.hx')):
        print(f"Warning: Character.hx not found in {source_dir}")
    if not os.path.isfile(os.path.join(source_dir, 'PlayState.hx')):
        print(f"Warning: PlayState.hx not found in {source_dir}")
    
    os.makedirs(args.output_dir, exist_ok=True)
    
    if args.parse_only:
        parse_only(args.mod_root, args.output_dir)
    else:
        parse_and_build(args.mod_root, args.output_dir, args.scale, args.bpp, args.psxfunkin_scale)


if __name__ == '__main__':
    main()