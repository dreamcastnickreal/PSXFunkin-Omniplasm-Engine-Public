#!/usr/bin/env python3
"""Check animations for 3d-bf-death"""

import re

with open(r'C:\Users\nicho\Downloads\1.2-source-code\VsEvilCorruptedDaveDay3Final-98f11efb93d5dd8fbc4b74f88de6f04dfbac3ad6\source\Character.hx', 'r', encoding='utf-8-sig') as f:
    content = f.read()

# Find the 3d-bf-death case
switch_match = re.search(r'switch\s+\(curCharacter\)\s*\{(.*?)\n\s*\}', content, re.DOTALL)
if switch_match:
    switch_content = switch_match.group(1)
    case_blocks = re.split(r'\n\s*case\s+\'', switch_content)
    for block in case_blocks[1:]:
        first_line = block.split('\n')[0]
        if '3d-bf-death' in first_line:
            char_content = '\n'.join(block.split('\n')[1:])
            print('Block content:')
            print(block[:2000])
            break

# Test the animation regex
anim_pattern = r"animation\.addByPrefix\('([^']+)',\s*'([^']+)'(?:\.toLowerCase\(\))?\s*,\s*(\d+),\s*(true|false)"
for match in re.finditer(anim_pattern, block):
    anim_name, prefix, fps, loop = match.groups()
    print(f'Animation: {anim_name}, prefix: {prefix}, fps: {fps}, loop: {loop}')