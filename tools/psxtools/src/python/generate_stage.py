#!/usr/bin/env python3
"""Generate C stage files from stage assets (json, hx, hxs, lua, manifest formats).

Two output formats:
  legacy : weekN.c style (WeekN_Load/WeekN_DrawBG, overlay EXE packing)
  omni   : Omniplasm src/stage style (Back_X struct, per-layer DrawBG/MD/FG/HUD,
           ARC loading) including Week4-style animated actors when items carry
           'frames'/'animations' (CharFrame + Animation + SetFrame/Draw helpers
           driven through an Animatable).
"""

import os
import sys
import json
import re
import struct
import argparse
from collections import Counter

try:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'PSXFunkin-CharacterMaker-main', 'PSXFunkin-CharacterMaker-main', 'src'))
    from generate_stage_ch import parse_all_stages, StageFileGenerator
    HAVE_LEGACY = True
except Exception:
    HAVE_LEGACY = False


def parse_lua_file(filepath):
    """Parse Lua stage file - converts to JSON-like structure."""
    with open(filepath, 'r', encoding='utf-8') as f:
        lua_content = f.read()

    data = {}
    items = []

    # Parse makeLuaSprite / makeAnimatedLuaSprite calls
    sprite_pattern = r"(?:makeLuaSprite|makeAnimatedLuaSprite)\s*\(\s*['\"]([^'\"]+)['\"](?:\s*,\s*['\"]([^'\"]+)['\"])?(?:\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+))?"
    for order, match in enumerate(re.finditer(sprite_pattern, lua_content)):
        name, image, x, y = match.groups()
        if not image:
            continue
        items.append({
            'name': name,
            'image': image,
            'position': [float(x or 0), float(y or 0)],
            'scroll': [1.0, 1.0],
            'layer': 'bg',
            'order': order,
            'animated': 'makeAnimatedLuaSprite' in match.group(0)
        })

    # Parse scale objects
    for item in items:
        name = re.escape(item['name'])
        scale = re.search(r"scaleObject\s*\(\s*['\"]" + name + r"['\"]\s*,\s*([\d.]+)\s*,\s*([\d.]+)", lua_content)
        if scale:
            item['scale'] = [float(scale.group(1)), float(scale.group(2))]
        scroll = re.search(r"setScrollFactor\s*\(\s*['\"]" + name + r"['\"]\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)", lua_content)
        if scroll:
            item['scroll'] = [float(scroll.group(1)), float(scroll.group(2))]
        for axis in ('x', 'y'):
            prop = re.search(r"setProperty\s*\(\s*['\"]" + name + r"\." + axis + r"['\"]\s*,\s*[^\n]*?([+-])\s*([\d.]+)", lua_content)
            if prop:
                item['position'][0 if axis == 'x' else 1] += (1 if prop.group(1) == '+' else -1) * float(prop.group(2))
        hud = re.search(r"setObjectCamera\s*\(\s*['\"]" + name + r"['\"]\s*,\s*['\"](camHUD|camOther)['\"]", lua_content)
        if hud:
            item['layer'] = 'hud'

    if items:
        data['items'] = items

    # Extract stage properties
    image_match = re.search(r'image\s*=\s*["\']([^"\']+)["\']', lua_content)
    if image_match:
        data["image"] = image_match.group(1)

    scale_match = re.search(r'scale\s*=\s*([\d.]+)', lua_content)
    if scale_match:
        data["scale"] = float(scale_match.group(1))

    position_match = re.search(r'position\s*=\s*\{([^}]+)\}', lua_content)
    if position_match:
        positions = [float(x.strip()) for x in position_match.group(1).split(',')]
        if len(positions) > 1:
            data["position"] = positions
        elif len(positions) == 1:
            data["position"] = [positions[0], 0]
        else:
            data["position"] = [0, 0]

    # Parse table-style items
    item_pattern = r'\{[^}]*name\s*=\s*["\']([^"\']+)["\'][^}]*\}'
    for match in re.finditer(item_pattern, lua_content):
        item_name = match.group(1)
        item_data = {"name": item_name}

        item_image = re.search(r'image\s*=\s*["\']([^"\']+)["\']', match.group(0))
        if item_image:
            item_data["image"] = item_image.group(1)

        item_scale = re.search(r'scale\s*=\s*([\d.]+)', match.group(0))
        if item_scale:
            item_data["scale"] = float(item_scale.group(1))

        item_pos = re.search(r'position\s*=\s*\{([^}]+)\}', match.group(0))
        if item_pos:
            positions = [float(x.strip()) for x in item_pos.group(1).split(',')]
            item_data["position"] = positions if len(positions) > 1 else [positions[0], 0]

        item_layer = re.search(r'layer\s*=\s*["\']([^"\']+)["\']', match.group(0))
        if item_layer:
            item_data["layer"] = item_layer.group(1)

        items.append(item_data)

    if items:
        data["items"] = items

    return data


def parse_hx_file(filepath):
    """Parse HX (Haxe) format stage data."""
    with open(filepath, 'r', encoding='utf-8') as f:
        hx_content = f.read()

    items = []

    def _eval_expr(s):
        s = s.strip()
        try:
            return float(s)
        except:
            pass
        try:
            return float(eval(s, {"__builtins__": {}}, {}))
        except:
            return 0.0

    pattern = r"new\s+BGSprite\s*\(\s*['\"]([^'\"]+)['\"]\s*,\s*([^,\n]+)\s*,\s*([^,\n]+)(?:\s*,\s*([^,\n]+)\s*,\s*([^,\n]+))?"
    for index, match in enumerate(re.finditer(pattern, hx_content)):
        image, x, y, sx, sy = match.groups()
        items.append({
            'name': os.path.basename(image).replace(' ', '_') or f'element_{index}',
            'image': image,
            'position': [_eval_expr(x), _eval_expr(y)],
            'scroll': [_eval_expr(sx) if sx else 1.0, _eval_expr(sy) if sy else 1.0],
            'layer': 'bg',
            'order': index
        })

    # Try to extract basic properties
    data = {}
    image_match = re.search(r'image\s*[:=]\s*["\']([^"\']+)["\']', hx_content)
    if image_match:
        data["image"] = image_match.group(1)

    scale_match = re.search(r'scale\s*[:=]\s*([\d.]+)', hx_content)
    if scale_match:
        data["scale"] = float(scale_match.group(1))

    return data, items


def parse_hxs_file(filepath):
    """Parse HXS (Haxe Serialize) format stage data."""
    with open(filepath, 'r', encoding='utf-8') as f:
        hxs_content = f.read()

    data = {}

    image_match = re.search(r'image["\']?\s*[:=]\s*["\']([^"\']+)["\']', hxs_content)
    if image_match:
        data["image"] = image_match.group(1)

    scale_match = re.search(r'scale["\']?\s*[:=]\s*([\d.]+)', hxs_content)
    if scale_match:
        data["scale"] = float(scale_match.group(1))

    return data


def parse_json_file(filepath):
    """Parse JSON stage file."""
    with open(filepath, 'r', encoding='utf-8') as f:
        return json.load(f)


def find_stage_files(folder):
    """Find all stage files (json, hx, hxs, lua, manifest) in a folder."""
    stage_files = {
        'json': None,
        'hx': None,
        'hxs': None,
        'lua': None,
        'manifest': None
    }

    for filename in os.listdir(folder):
        filepath = os.path.join(folder, filename)
        if not os.path.isfile(filepath):
            continue

        lower = filename.lower()
        if lower.endswith('.json'):
            stage_files['json'] = filepath
        elif lower.endswith('.hx'):
            stage_files['hx'] = filepath
        elif lower.endswith('.hxs'):
            stage_files['hxs'] = filepath
        elif lower.endswith('.lua'):
            stage_files['lua'] = filepath
        elif lower.endswith('.txt') or lower.endswith('.manifest'):
            stage_files['manifest'] = filepath

    return stage_files


def parse_manifest_file(filepath):
    """Parse PSXFunkin export manifest format.
    
    Format: image_name | alias | scale=VALUE | rect=[x, y, w, h] | pos=[x, y] | vram=[vx, vy, ow, oh, ???] | source=PATH
    Also supports [actN] sections to group items by act.
    """
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    items = []
    current_act = None

    # Parse each line: image | alias | scale=VALUE | rect=[x, y, w, h] | pos=[x, y] | vram=[vx, vy, ow, oh, ???] | source=PATH
    # [actN] headers switch the act for the lines that follow them.
    line_pattern = r'^(.+?)\s*\|\s*(.+?)\s*\|\s*scale=([\d.]+)\s*\|\s*rect=\[([^\]]+)\]\s*\|\s*pos=\[([^\]]+)\]\s*\|\s*vram=\[([^\]]+)\]\s*\|\s*source=(.+)$'

    for line in content.split('\n'):
        s = line.strip()
        if not s:
            continue
        mact = re.match(r'\[act(\d+)\]', s, re.I)
        if mact:
            current_act = f"act{mact.group(1)}"
            continue
        if s.startswith('['):
            continue

        m = re.match(line_pattern, s)
        if m:
            image_name = m.group(1).strip()
            alias = m.group(2).strip()
            scale = float(m.group(3))
            rect = [int(x.strip()) for x in m.group(4).split(',')]
            pos = [int(x.strip()) for x in m.group(5).split(',')]
            vram = [int(x.strip()) for x in m.group(6).split(',')]
            source = m.group(7).strip()
            
            # Determine layer from source path or alias
            layer = 'bg'
            if 'bg' in source.lower() or alias.lower() == 'bg':
                layer = 'bg'
            elif 'fg' in source.lower() or alias.lower() == 'fg':
                layer = 'fg'
            elif 'md' in source.lower():
                layer = 'md'
            elif 'hud' in source.lower():
                layer = 'hud'
            
            item = {
                'name': alias,
                'image': image_name,
                'rect': rect,       # crop inside the packed sheet (TIM px)
                'src': rect,        # alias kept for the static emitter
                'pos': pos,         # pack offset (NOT a stage position)
                'position': pos,    # replaced by hx merge when matched
                'pack_scale': scale,  # art->TIM shrink; metadata only
                'scale': [scale, scale],
                'scroll': [1.0, 1.0],
                'layer': layer,
                'act': current_act,
                'source': source,
                'flip_x': False,
                'animations': [],
            }
            
            # Extract VRAM position (first two values are vram, next two are clut)
            if len(vram) >= 4:
                item['vram_position'] = vram[:2]
                item['clut_position'] = vram[2:4]
            
            items.append(item)

    return {'items': items, 'image': None, 'scale': 1.0, 'position': [0, 0], 'scroll': [1, 1]}


def _eval_num(s):
    """Safe float eval for hx arithmetic like '-730 - 275'. 0.0 if not numeric."""
    s = (s or '').strip()
    try:
        return float(s)
    except (TypeError, ValueError):
        pass
    if not s or not re.fullmatch(r'[\d\s\.\+\-\*/\(\)]+', s):
        return 0.0
    try:
        return float(eval(s, {"__builtins__": {}}, {}))
    except Exception:
        return 0.0


def parse_hx_placements(hx_content):
    """BGSprite placements from stars.hx: var -> placement dict.

    Captures the constructor (image, x, y, scroll) plus follow-up props:
    scale.set (FNF display scale -> dst multiplier), scrollFactor.set
    (scroll override), cameras (camOther/camHUD -> hud layer),
    screenCenter (centered flag), flipX, alpha (0..1 -> blend opacity).
    """
    out = {}
    ctor = re.compile(
        r"(\w+)\s*=\s*new\s+BGSprite\s*\(\s*['\"]([^'\"]+)['\"]"
        r"\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^,\)\]]+?)"
        r"(?:\s*,\s*(\[[^\]]*\]))?\s*\)")
    for m in ctor.finditer(hx_content):
        var, image, x, y, sx, sy, anims = m.groups()
        anim_list = []
        if anims:
            anim_list = re.findall(r"['\"]([^'\"]+)['\"]", anims)
        out[var] = {
            'var': var,
            'image': image,
            'x': _eval_num(x), 'y': _eval_num(y),
            'scroll': [_eval_num(sx), _eval_num(sy)],
            'anims': anim_list,
            'hud': False,
            'scale': [1.0, 1.0],
            'scroll_override': None,
            'centered': False,
            'flip_x': False,
            'alpha': None,
        }
    for m in re.finditer(r"(\w+)\.scale\.set\(\s*([^,]+?)\s*,\s*([^\)]+?)\)", hx_content):
        var = m.group(1)
        if var in out:
            out[var]['scale'] = [_eval_num(m.group(2)), _eval_num(m.group(3))]
    for m in re.finditer(r"(\w+)\.scrollFactor\.set\(\s*([^,]+?)\s*,\s*([^\)]+?)\)", hx_content):
        var = m.group(1)
        if var in out:
            out[var]['scroll_override'] = [_eval_num(m.group(2)), _eval_num(m.group(3))]
    for m in re.finditer(r"(\w+)\.cameras\s*=\s*(\[[^\]]*\])", hx_content):
        var = m.group(1)
        if var in out and re.search(r'camOther|camHUD', m.group(2)):
            out[var]['hud'] = True
    for m in re.finditer(r"(\w+)\.screenCenter\s*\(", hx_content):
        if m.group(1) in out:
            out[m.group(1)]['centered'] = True
    for m in re.finditer(r"(\w+)\.flipX\s*=\s*true", hx_content):
        if m.group(1) in out:
            out[m.group(1)]['flip_x'] = True
    for m in re.finditer(r"(\w+)\.alpha\s*=\s*([\d.]+)", hx_content):
        if m.group(1) in out:
            try:
                out[m.group(1)]['alpha'] = float(m.group(2))
            except ValueError:
                pass
    # addByPrefix('name', 'prefix', fps, looped) per sprite var, either quote style
    for m in re.finditer(r"(\w+)\.animation\.addByPrefix\(\s*['\"]([^'\"]*)['\"]"
                         r"\s*,\s*['\"]([^'\"]*)['\"]\s*,\s*([\d.]+)\s*,\s*(true|false)",
                         hx_content):
        var = m.group(1)
        if var in out:
            try:
                fps = float(m.group(4))
            except ValueError:
                fps = 24.0
            out[var].setdefault('hx_anims', []).append({
                'name': m.group(2), 'prefix': m.group(3),
                'fps': fps, 'looped': m.group(5) == 'true',
            })
    return out


def _norm_prefix(name):
    """Animation prefix of a SubTexture name: strip trailing digits, trim."""
    return re.sub(r'\d+$', '', str(name or '')).strip()


def parse_source_xmls(source_dir):
    """Sparrow atlases under <folder>/source: xml basename -> atlas info.

    Returns {key: {'path', 'png' (or None), 'defs': [{name, prefix, x, y, w,
    h, fx, fy, fw, fh, idx}]}}. Only XMLs with a sibling PNG are usable
    (the packed TIMs must exist for the frames to be drawable).
    """
    index = {}
    if not source_dir or not os.path.isdir(source_dir):
        return index
    for root, _dirs, files in os.walk(source_dir):
        for fn in sorted(files):
            if not fn.lower().endswith('.xml'):
                continue
            path = os.path.join(root, fn)
            try:
                import xml.etree.ElementTree as ET
                tree = ET.parse(path)
            except Exception:
                continue
            png = os.path.join(root, os.path.splitext(fn)[0] + '.png')
            if not os.path.isfile(png):
                # case-insensitive stem fallback (e.g. 'gray static')
                same = [p for p in os.listdir(root)
                        if p.lower().endswith('.png')
                        and os.path.splitext(p)[0].lower() == os.path.splitext(fn)[0].lower()]
                png = os.path.join(root, same[0]) if same else None
            defs = []
            for idx, elem in enumerate(tree.getroot().iter()):
                nm = elem.attrib.get('name')
                if not nm:
                    continue

                def _i(key):
                    try:
                        return int(float(elem.attrib.get(key, 0)))
                    except (TypeError, ValueError):
                        return 0

                w = _i('width') or _i('w')
                h = _i('height') or _i('h')
                if w <= 0 or h <= 0:
                    continue
                defs.append({
                    'name': nm, 'prefix': _norm_prefix(nm), 'idx': idx,
                    'x': _i('x'), 'y': _i('y'), 'w': w, 'h': h,
                    'fx': _i('frameX'), 'fy': _i('frameY'),
                    'fw': _i('frameWidth'), 'fh': _i('frameHeight'),
                })
            if defs:
                index[os.path.splitext(fn)[0].lower()] = {
                    'path': path, 'png': png, 'defs': defs,
                }
    return index


def parse_stars_json(path):
    """stars.json character/stage positions. {} when absent or unrelated."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception:
        return {}
    if not isinstance(data, dict) or 'boyfriend' not in data:
        return {}
    out = {'characters': {}}
    for key in ('boyfriend', 'opponent', 'girlfriend'):
        if key in data:
            try:
                out['characters'][key] = [float(data[key][0]), float(data[key][1])]
            except (TypeError, ValueError, IndexError):
                pass
    for key in ('defaultZoom', 'isPixelStage', 'directory', 'hide_girlfriend'):
        if key in data:
            out[key] = data[key]
    return out


def merge_hx_positions(items, placements, pos_scale=0.1):
    """Apply hx placements onto manifest items (matched by source image).

    Position, scroll, display scale, hud layer, flip and alpha come from hx;
    rect/TIM/sheet elements stay from the manifest. A placement only applies
    when its var carries the item's act (act1 items <- act1Stat, ...), so
    shared sheets (e.g. Act_2_Intro) don't leak across acts. Items without a
    match keep manifest values. Mutates items in place, returns (matched, total).
    """
    by_image = {}
    for var, p in placements.items():
        base = os.path.splitext(os.path.basename(p['image']))[0].lower()
        by_image.setdefault(base, []).append((var, p))
    # aliases forming a numeric sequence (stat0..3, p2tran0..42, body0..)
    # are animation frames, not placed singletons: leave them unmerged
    # (consumed via --animate grouping or left at manifest pos)
    seq_re = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*?)\d+$')
    counts = {}
    for it in items:
        m = seq_re.match(str(it.get('name') or ''))
        if m:
            key = ((it.get('act') or '').lower(), m.group(1).lower())
            counts[key] = counts.get(key, 0) + 1
    matched = 0
    for it in items:
        m = seq_re.match(str(it.get('name') or ''))
        if m and counts.get(((it.get('act') or '').lower(), m.group(1).lower()), 0) > 1:
            continue
        srcbase = os.path.splitext(
            os.path.basename(str(it.get('source') or '')))[0].lower()
        cands = by_image.get(srcbase, [])
        act = (it.get('act') or '').lower()
        if act:
            same = [c for c in cands if act in c[0].lower()]
            if same:
                cands = same
            elif any(re.search(r'act\d', c[0].lower()) for c in cands):
                # every candidate belongs to another act: no merge
                cands = []
        if not cands:
            continue
        var, p = cands[0]
        matched += 1
        it['hx_var'] = var
        it['hx_anims'] = p['anims']
        it['scroll'] = list(p['scroll_override'] or p['scroll'])
        it['hx_scale'] = list(p['scale'])
        if p['hud']:
            it['layer'] = 'hud'
        if p['flip_x']:
            it['flip_x'] = True
        if p['alpha'] is not None:
            try:
                it['blend'] = {'mode': 0, 'opacity': int(round(float(p['alpha']) * 255))}
            except (TypeError, ValueError):
                pass
        dw = it['rect'][2] * p['scale'][0] if it.get('rect') else 0
        dh = it['rect'][3] * p['scale'][1] if it.get('rect') else 0
        if p['centered']:
            it['position'] = [-dw / 2.0, -dh / 2.0]
        else:
            # manifest pos is a fine pixel offset on top of the hx position
            off = it.get('pos') or [0, 0]
            try:
                ox, oy = float(off[0]), float(off[1])
            except (TypeError, ValueError, IndexError):
                ox, oy = 0.0, 0.0
            it['position'] = [p['x'] * pos_scale + ox, p['y'] * pos_scale + oy]
    return matched, len(items)


def _match_xml_frame(member, xml, skey, used, warnings, aid):
    """Greedy nearest art-size match of a manifest member to an XML def.

    Used when alias numbers can't be doc indices (e.g. headp steps exceed
    the atlas). Expected art size = packed rect / pack scale. used holds
    (source, idx) keys so multi-source buckets can't collide.
    """
    defs = xml['defs']
    scale = float(member.get('scale', [1.0, 1.0])[0]
                  if isinstance(member.get('scale'), list)
                  else (member.get('scale') or 1.0))
    rect = member.get('rect') or [0, 0, 0, 0]
    ew, eh = rect[2] / (scale or 1.0), rect[3] / (scale or 1.0)
    best, best_err, best_def = None, None, None
    for idx, d in enumerate(defs):
        if (skey, idx) in used:
            continue
        err = abs(d['w'] - ew) + abs(d['h'] - eh)
        if best is None or err < best_err:
            best, best_err, best_def = idx, err, d
    if best is not None:
        denom = max(1.0, ew + eh)
        if best_err / denom > 0.35:
            warnings.append(
                f"{aid}: frame '{member.get('name')}' size-matched to "
                f"'{best_def['name']}' with {best_err / denom:.0%} error")
    return best_def


def group_stat_actors(items, placements, pos_scale=0.1, prefixes=('stat',),
                      xml_index=None):
    """Pull <prefix><N> sequences (per act) out of statics into actor items.

    Source XML atlases (when found under <folder>/source) upgrade the actor:
    SubTexture trims become proper CharFrame offsets and each XML animation
    prefix becomes its own Animation entry (speed/loop from hx addByPrefix).
    Without XML the previous behavior holds (manifest pos offsets, one
    looping animation). Returns (actors, rest, warnings).
    """
    xml_index = xml_index or {}
    seq_re = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*?)(\d+)$')
    buckets = {}
    rest = []
    want_all = '*' in prefixes
    wanted = [p.lower() for p in prefixes]
    for it in items:
        m = seq_re.match(str(it.get('name') or ''))
        if m and (want_all or m.group(1).lower() in wanted):
            buckets.setdefault((it.get('act'), m.group(1).lower()), []).append(
                (int(m.group(2)), it))
        else:
            rest.append(it)
    actors, warnings = [], []
    for (act, prefix), members in sorted(buckets.items(),
                                        key=lambda kv: (kv[0][0] or '', kv[0][1])):
        members.sort(key=lambda t: t[0])
        if len(members) < 2:
            # lone numbered alias, not a sequence: stays static
            rest.extend(it for _, it in members)
            continue
        # TIM sheets in frame order
        tims, seen = [], set()
        frames = []
        for _num, it in members:
            sheet = str(it.get('image') or '')
            if sheet not in seen:
                seen.add(sheet)
                tims.append(sheet)
            rect = it.get('rect') or [0, 0, 0, 0]
            frames.append({'tex': tims.index(sheet),
                           'src': [rect[0], rect[1], rect[2], rect[3]],
                           'off': [0, 0]})
        if not frames:
            continue
        # manifest pos doubles as the per-frame draw offset (Sparrow-style
        # alignment: dead0 [15,8] vs dead1 [8,4]); the Draw helper
        # subtracts it when any frame carries a nonzero offset
        for _f, (_num, _it) in zip(frames, members):
            _f['off'] = list(_it.get('pos', [0, 0]) or [0, 0])[:2]
            while len(_f['off']) < 2:
                _f['off'].append(0)
        # placements whose image matches the frames' source art: one
        # instance per placement (same-act preferred), so e.g. p2tran can
        # play different animations at two stage positions at once
        srcbase = os.path.splitext(os.path.basename(
            str(members[0][1].get('source') or '')))[0].lower()
        places = []
        for var, p in placements.items():
            if os.path.splitext(os.path.basename(p['image']))[0].lower() == srcbase:
                places.append(p)
        if act:
            same = [p for p in places if act.lower() in p['var'].lower()]
            if same:
                places = same
        place = places[0] if places else None

        def _make_inst(p, anim):
            ins = {'x': 0.0, 'y': 0.0, 'par': [1.0, 1.0],
                   'layer': 'bg', 'anim': anim}
            if p is None:
                return ins
            ins['x'] = p['x'] * pos_scale
            ins['y'] = p['y'] * pos_scale
            ins['par'] = list(p['scroll_override'] or p['scroll'])
            if p['hud']:
                ins['layer'] = 'hud'
            if p['alpha'] is not None:
                try:
                    ins['blend'] = {'mode': 0,
                                    'opacity': int(round(float(p['alpha']) * 255))}
                except (TypeError, ValueError):
                    pass
            return ins

        scale = list(place['scale']) if place else [1.0, 1.0]
        aid = prefix if not act else f'{prefix}_{act}'
        # ---- source XML upgrade: trims -> offsets, prefixes -> animations
        anims_out = None
        anim_dst = {}
        by_source = {}
        for _num, it in members:
            by_source.setdefault(os.path.splitext(
                os.path.basename(str(it.get('source') or '')))[0].lower(),
                []).append((_num, it))
        xml_frames = {}  # table index -> xml def
        if any(s in xml_index and xml_index[s]['png'] for s in by_source):
            use_index = True
            for s, sub in by_source.items():
                xml = xml_index.get(s)
                if not (xml and xml['png']):
                    use_index = False
                    break
                for _num, _it in sub:
                    if _num >= len(xml['defs']):
                        use_index = False
                        break
            used_defs = set()
            order = []
            for pos, (_num, it) in enumerate(members):
                s = os.path.splitext(os.path.basename(
                    str(it.get('source') or '')))[0].lower()
                xml = xml_index.get(s)
                d = None
                if xml and xml['png'] and use_index:
                    if (s, _num) not in used_defs:
                        d = xml['defs'][_num]
                        used_defs.add((s, _num))
                elif xml and xml['png']:
                    d = _match_xml_frame(it, xml, s, used_defs,
                                         warnings, aid)
                    if d is not None:
                        used_defs.add((s, xml['defs'].index(d)))
                if d is not None:
                    xml_frames[pos] = d
                    order.append(pos)
            if xml_frames:
                # proper offsets from XML trims (art px -> TIM px -> dst px);
                # fall back to manifest pos where the atlas is untrimmed
                ds = scale
                try:
                    dsx, dsy = float(ds[0]), float(ds[1])
                except (TypeError, ValueError, IndexError):
                    dsx = dsy = 1.0
                for pos, d in xml_frames.items():
                    _num, it = members[pos]
                    try:
                        ps = float(it.get('scale', [1.0, 1.0])[0]
                                   if isinstance(it.get('scale'), list)
                                   else (it.get('scale') or 1.0))
                    except (TypeError, ValueError, IndexError):
                        ps = 1.0
                    if (d['fx'], d['fy']) != (0, 0):
                        frames[pos]['off'] = [round(d['fx'] * ps * dsx),
                                              round(d['fy'] * ps * dsy)]
                # one animation per XML prefix present in the table
                groups = {}
                for pos in order:
                    pre = xml_frames[pos].get('prefix') or 'anim'
                    groups.setdefault(pre, []).append(pos)
                hx_speed = {}
                if place:
                    for ha in list(place.get('hx_anims') or []) + \
                            [{'prefix': a, 'fps': None, 'looped': True}
                             for a in place.get('anims') or []]:
                        hx_speed.setdefault(str(ha.get('prefix') or '').lower(),
                                            ha)
                anims_out = []
                anim_dst = {}
                for pre, seq in groups.items():
                    key = pre.lower()
                    ha = hx_speed.get(key)
                    if ha is None:
                        for k, v in hx_speed.items():
                            if k and (k in key or key in k):
                                ha = v
                                break
                    if ha is not None and ha.get('fps'):
                        try:
                            speed = max(1, round(float(ha['fps']) / 12))
                        except (TypeError, ValueError):
                            speed = 2
                        looped = ha.get('looped', True)
                    else:
                        speed, looped = 2, True
                    anims_out.append({'speed': speed,
                                      'seq': list(seq) + ['ASCR_LOOP' if looped
                                                          else 'ASCR_HOLD']})
                    # constant dst for this animation: most common canvas of
                    # its frames (0.25 x canvas x draw scale, trio-style).
                    # Falls back to frame art size when untrimmed.
                    sizes = []
                    for pos in seq:
                        d = xml_frames.get(pos)
                        if not d:
                            continue
                        if d.get('fw') and d.get('fh'):
                            sizes.append((d['fw'], d['fh']))
                    if not sizes:
                        for pos in seq:
                            d = xml_frames.get(pos)
                            if d:
                                sizes.append((d['w'], d['h']))
                    if sizes:
                        cw, ch = Counter(sizes).most_common(1)[0][0]
                        anim_dst[len(anims_out) - 1] = (
                            round(cw * 0.25 * dsx), round(ch * 0.25 * dsy))
        nanims = len(anims_out) if anims_out else 1
        # one instance per placement; extra placements spread across the
        # available animations (clamped) so distinct stage positions can
        # play different animations simultaneously (override via actors.json)
        instances = []
        for idx, p in enumerate(places):
            ins = _make_inst(p, min(idx, nanims - 1))
            instances.append(ins)
        if not instances:
            instances = [_make_inst(None, 0)]
        actors.append({
            'name': aid,
            'act': act,
            'layer': instances[0]['layer'],
            'arc': None,  # generator default BACK arc
            'tims': [os.path.splitext(t)[0] + '.tim' for t in tims],
            'sym': aid.capitalize() if not act else f'{prefix.capitalize()}_{act}',
            'scale': scale,
            'frames': frames,
            'animations': anims_out or [{'speed': 1,
                                         'seq': list(range(len(frames))) + ['ASCR_LOOP']}],
            'anim_dst': anim_dst,
            'instances': instances,
        })
    return actors, rest, warnings


# ================= Omniplasm (src/stage) generator =================

MPL = """/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
"""

LAYER_FNS = {'bg': 'DrawBG', 'md': 'DrawMD', 'fg': 'DrawFG', 'hud': 'DrawHUD'}


def _png_size(path):
    """PNG dimensions without Pillow (IHDR). None when unreadable."""
    try:
        with open(path, 'rb') as f:
            head = f.read(26)
        if head[:8] != b'\x89PNG\r\n\x1a\n':
            return None
        w, h = struct.unpack('>II', head[16:24])
        return (w, h)
    except (OSError, struct.error):
        return None


def _sanitize(name):
    out = ''.join(c if (c.isalnum() or c == '_') else '_' for c in str(name))
    return out or 'x'


def _cap(name):
    # capitalize every _-separated word: stars_act1 -> Stars_Act1
    out = '_'.join(p[:1].upper() + p[1:] for p in str(name).split('_') if p)
    return out or 'Stage'


def _act_tag(act):
    return _sanitize(act) if act else ''


def _sheet_base(image):
    """TIM/sheet base without act scope: stat0.png -> stat0."""
    return _sanitize(os.path.splitext(os.path.basename(str(image or 'back0')))[0])


def _field_base(item):
    """C-symbol base with act scope: act1 + bg.png -> act1_bg."""
    t = _sheet_base(item.get('image', 'back0'))
    a = _act_tag(item.get('act'))
    return f'{a}_{t}' if a else t


def _item_arc(item, upper, def_arc):
    """ARC path: explicit item arc, else per-act ARC, else BACK.ARC."""
    if item.get('arc'):
        return item['arc']
    a = item.get('act')
    if a:
        return '\\\\%s\\\\%s.ARC;1' % (upper, re.sub(r'\W', '_', str(a)).upper())
    return def_arc


def _num(v, default=0.0):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def _fxd(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return '0'
    f = round(f, 4)  # kill binary float noise (427/320 products etc.)
    if abs(f - round(f)) < 1e-6:
        return f'FIXED_DEC({int(round(f))},1)'
    return f'FIXED_DEC({f},1)'


def _par_expr(f):
    """Integer-only camera factor (engine style, no float math on PSX).

    1 -> '' (bare stage.camera.x), 0.5 -> '>> 1', 1.25 -> '* 5 >> 2',
    0.4 -> '* 2 / 5'. None when no follow. NOTE: a pure '>> N' can only
    express dyadic fractions, so 0.4 can never be '>> 4' (that is 0.0625).
    """
    if f == 0:
        return None
    if f == 1:
        return ''
    if float(f).is_integer():
        return f' * {int(f)}'
    for b in range(1, 11):
        a = round(f * (1 << b))
        if a > 0 and abs(a / (1 << b) - f) < 1e-9:
            return f' >> {b}' if a == 1 else f' * {a} >> {b}'
    from fractions import Fraction
    fr = Fraction(f).limit_denominator(100)
    if abs(float(fr) - f) < 1e-9 and fr.denominator > 1:
        return f' * {fr.numerator} / {fr.denominator}'
    return f' * {f:g}'


def _par_lines(par, indent='\t'):
    """C fx/fy assignments for a [sx, sy] scroll pair. [] when no follow."""
    lines = []
    for axis, v in (('x', par[0] if len(par) > 0 else 0),
                    ('y', par[1] if len(par) > 1 else 0)):
        try:
            f = float(v)
        except (TypeError, ValueError):
            continue
        e = _par_expr(f)
        if e is None:
            continue
        lines.append(f'{indent}f{axis} = stage.camera.{axis}{e};')
    return lines


def _actor_states(a):
    """Playback states of an actor: [(suffix, anim_idx)].

    Single-state actors get [('', anim)] (classic unsuffixed symbols,
    byte-identical to the old output). Multi-state actors get one suffixed
    state per animation index used by their instances ('_0', '_1', ...),
    so several instances can play different animations at the same time
    (each state has its own Animatable + frame/tex_id). Instances sharing
    an animation share its state (synchronized playback).
    """
    nanims = max(1, len(a.get('animations') or []))
    idxs = set()
    for ins in (a.get('instances') or [{}]):
        try:
            idx = int(ins.get('anim', 0))
        except (TypeError, ValueError):
            idx = 0
        idxs.add(min(max(0, idx), nanims - 1))
    if len(idxs) <= 1:
        return [('', next(iter(idxs), 0))]
    return [(f'_{i}', i) for i in sorted(idxs)]


def _inst_state(a, ins):
    """(suffix, anim_idx) for one instance (sidecar may omit 'anim')."""
    nanims = max(1, len(a.get('animations') or []))
    try:
        idx = int(ins.get('anim', 0))
    except (TypeError, ValueError):
        idx = 0
    idx = min(max(0, idx), nanims - 1)
    for sfx, ai in a.get('_states', [('', 0)]):
        if ai == idx:
            return sfx, idx
    return '', idx


def generate_omni_files(stage_name, items, output_dir, extra=None):
    """Emit Back_<Stage>.c/.h in Omniplasm style. Returns (c_path, h_path).

    Static item keys: name, image, position [x, y] (hx-merged PSX px),
    rect [x, y, w, h] (TIM crop), hx_scale [sx, sy] (dst multiplier),
    scroll [sx, sy], layer bg/md/fg/hud, flip_x, blend {mode, opacity},
    cond, arc. Without 'rect' (lua/hx items), size+scale drive dst.
    Animated item keys: frames [{tex, src, off}], animations [{speed, seq}],
    tims [..], arc, animatable, scale [sx, sy] (Draw helper dst multiplier),
    instances [{x, y, par, layer, anim, blend}] (or position+scroll).
    Each extra placement becomes an instance; instance 'anim' selects which
    animation it plays (default spreads placements across animations).
    Single-state actors emit byte-identical code to previous versions.
    extra: optional {'characters': {...}, ...} emitted as a comment block.
    """
    back = 'Back_' + _cap(stage_name)
    upper = re.sub(r'\W', '_', stage_name).upper() or 'STAGE'
    # ARC folder drops a split _ACTn suffix (stars_act1 -> STARS\ACT1.ARC)
    arc_upper = re.sub(r'_ACT\d+$', '', upper) or upper
    def_arc = f'\\\\{arc_upper}\\\\BACK.ARC;1'

    statics, actors = [], []
    for it in items:
        (actors if it.get('frames') else statics).append(it)
    for a in actors:
        a['_states'] = _actor_states(a)
    # per-act files hold a single act: drop its tag from fn bases
    # (Stars_Act1_P2tran_Act1_* -> Stars_Act1_P2tran_*)
    _file_acts = set(a.get('act') for a in actors if a.get('act')) | \
        set(it.get('act') for it in statics if it.get('act'))
    _solo_act = next(iter(_file_acts)) if len(_file_acts) == 1 else None

    def _fn_sym(sym):
        if _solo_act:
            suf = '_' + str(_solo_act)
            if sym.lower().endswith(suf.lower()):
                return sym[:-len(suf)]
        return sym

    # texture loads: (field base, TIM base, ARC) deduped, first-seen order.
    # field bases carry act scope (act1_bg) so multi-act stages can't
    # collide; TIM bases stay sheet names (bg.tim lives in each act ARC).
    triplets, seen_f = [], set()
    for it in statics:
        tbase = _sheet_base(it.get('image', 'back0'))
        fbase = _field_base(it)
        arc = _item_arc(it, arc_upper, def_arc)
        it['_fbase'], it['_tbase'], it['_arc'] = fbase, tbase, arc
        if fbase not in seen_f:
            seen_f.add(fbase)
            triplets.append((fbase, tbase, arc))

    L = []
    L.append(MPL)
    L.append(f'\n#include "{stage_name}.h"\n\n')
    L.append('#include "../archive.h"\n#include "../mem.h"\n#include "../stage.h"\n')
    if actors:
        L.append('#include "../animation.h"\n')
    L.append('\n')

    chars = (extra or {}).get('characters', {})
    if chars or (extra or {}).get('defaultZoom') is not None:
        L.append('//Stage reference from stars.json (FNF px, not engine code)\n')
        for key in ('boyfriend', 'opponent', 'girlfriend'):
            if key in chars:
                L.append(f'//  {key}: [{chars[key][0]:g}, {chars[key][1]:g}]\n')
        for key in ('defaultZoom', 'isPixelStage', 'directory', 'hide_girlfriend'):
            if key in (extra or {}):
                L.append(f'//  {key}: {(extra or {})[key]}\n')
        L.append('\n')

    # struct
    L.append(f'//{back} background structure\ntypedef struct\n{{\n')
    L.append('\t//Stage background base structure\n\tStageBack back;\n')
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        arcvar = f'arc_{aid}'
        nptr = max(len(a.get('tims', [])), 1)
        L.append(f'\t\n\tIO_Data {arcvar}, {arcvar}_ptr[{nptr}];\n')
    L.append('\t\n\t//Textures\n')
    for (fbase, tbase, _arc) in triplets:
        L.append(f'\tGfx_Tex tex_{fbase}; //{tbase}.tim\n')
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        L.append(f'\tGfx_Tex tex_{aid}; //animated {aid}\n')
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        sym = _cap(a.get('sym', aid))
        atable = a.get('animatable', f'{aid}_animatable')
        L.append(f'\t\n\t//{sym} state\n')
        for sfx, _anim in a['_states']:
            L.append(f'\tu8 {aid}_frame{sfx}, {aid}_tex_id{sfx};\n')
            L.append(f'\tAnimatable {atable}{sfx};\n')
    L.append(f'}} {back};\n')

    # animated scaffolding (one SetFrame/Draw pair per playback state)
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        sym = _cap(a.get('sym', aid))
        fnbase = a.get('fnbase', f'{_cap(stage_name)}_{_fn_sym(sym)}')
        atable = a.get('animatable', f'{aid}_animatable')
        L.append(f'\n//{sym} animation and rects\n')
        L.append(f'static const CharFrame {aid}_frame[] = {{\n')
        for f in a.get('frames', []):
            s = f.get('src', [0, 0, 0, 0])
            o = f.get('off', [0, 0])
            ox = o[0] if len(o) > 0 else 0
            oy = o[1] if len(o) > 1 else 0
            L.append(f'\t{{{f.get("tex", 0)}, {{  {s[0]},  {s[1]},  {s[2]},  {s[3]}}}, '
                     f'{{ {ox},  {oy}}}}},\n')
        L.append('};\n')
        L.append(f'\nstatic const Animation {aid}_anim[] = {{\n')
        for an in a.get('animations', [{"speed": 1, "seq": [0]}]):
            seq = ', '.join(str(x) for x in an.get('seq', [0]))
            L.append(f'\t{{{an.get("speed", 1)}, (const u8[]) {{{seq}}}}},\n')
        L.append('};\n')
        use_off = any(f.get('off', [0, 0]) != [0, 0] for f in a.get('frames', []))
        asc = a.get('scale', [1, 1])
        try:
            asx, asy = float(asc[0]), float(asc[1])
        except (TypeError, ValueError, IndexError):
            asx = asy = 1.0
        flip = 'FlipX' if a.get('flip_x') else ''
        for sfx, _anim in a['_states']:
            L.append(f'\nvoid {fnbase}_SetFrame{sfx}(void *user, u8 frame)\n{{\n')
            L.append(f'\t{back} *this = ({back}*)user;\n\t\n')
            L.append('\t//Check if this is a new frame\n\tif (frame != this->'
                     f'{aid}_frame{sfx})\n\t{{\n')
            L.append('\t\t//Check if new art shall be loaded\n\t\tconst CharFrame *cframe = '
                     f'&{aid}_frame[this->{aid}_frame{sfx} = frame];\n')
            L.append(f'\t\tif (cframe->tex != this->{aid}_tex_id{sfx})\n')
            L.append(f'\t\t\tGfx_LoadTex(&this->tex_{aid}, this->arc_{aid}_ptr'
                     f'[this->{aid}_tex_id{sfx} = cframe->tex], 0);\n\t}}\n}}\n')
            L.append(f'\nvoid {fnbase}_Draw{sfx}({back} *this, fixed_t x, fixed_t y)\n{{\n')
            L.append('\t//Draw character\n\tconst CharFrame *cframe = '
                     f'&{aid}_frame[this->{aid}_frame{sfx}];\n\t\n')
            if use_off:
                L.append('\tfixed_t ox = x - ((fixed_t)cframe->off[0] << FIXED_SHIFT);\n')
                L.append('\tfixed_t oy = y - ((fixed_t)cframe->off[1] << FIXED_SHIFT);\n')
            else:
                L.append('\tfixed_t ox = x;\n\tfixed_t oy = y;\n')
            L.append('\n\tRECT src = {cframe->src[0], cframe->src[1], cframe->src[2], cframe->src[3]};\n')
            ad = (a.get('anim_dst') or {}).get(_anim)
            if ad:
                # constant canvas dst for this animation (0.25 x canvas x hx)
                L.append(f'\tRECT_FIXED dst = {{ox, oy, FIXED_DEC({ad[0]},1), FIXED_DEC({ad[1]},1)}};\n')
            elif asx == 1 and asy == 1:
                L.append('\tRECT_FIXED dst = {ox, oy, src.w << FIXED_SHIFT, src.h << FIXED_SHIFT};\n')
            else:
                L.append(f'\tRECT_FIXED dst = {{ox, oy, (src.w * {asx:g}) << FIXED_SHIFT, (src.h * {asy:g}) << FIXED_SHIFT}};\n')
            L.append(f'\tStage_DrawTex{("_" + flip) if flip else ""}'
                     f'(&this->tex_{aid}, &src, &dst, stage.camera.bzoom, stage.camera.angle);\n}}\n')

    # per-layer draw functions
    by_layer = {'bg': [], 'md': [], 'fg': [], 'hud': []}
    for it in statics:
        by_layer.get(str(it.get('layer', 'bg')).lower(), by_layer['bg']).append(('s', it))
    for a in actors:
        insts = a.get('instances') or [{'x': (a.get('position') or [0, 0])[0],
                                        'y': (a.get('position') or [0, 0])[1],
                                        'par': a.get('scroll', [1, 1]),
                                        'layer': a.get('layer', 'bg')}]
        for ins in insts:
            by_layer.get(str(ins.get('layer', a.get('layer', 'bg'))).lower(),
                         by_layer['bg']).append(('a', (a, ins)))

    for layer, fn in LAYER_FNS.items():
        members = by_layer[layer]
        if not members:
            continue
        L.append(f'\nvoid {back}_{fn}(StageBack *back)\n{{\n')
        L.append(f'\t{back} *this = ({back}*)back;\n\t\n\tfixed_t fx, fy;\n\t\n')
        done_animate = set()
        cur = [None, None]
        used_vars = set()  # RECT var names are function-scoped: dedup them
        for kind, payload in members:
            if kind == 'a':
                a, ins = payload
                aid = _sanitize(a.get('name', 'actor'))
                sym = _cap(a.get('sym', aid))
                fnbase = a.get('fnbase', f'{_cap(stage_name)}_{_fn_sym(sym)}')
                atable = a.get('animatable', f'{aid}_animatable')
                sfx, _anim = _inst_state(a, ins)
                if (aid, sfx) not in done_animate:
                    L.append(f'\tAnimatable_Animate(&this->{atable}{sfx}, (void*)this, {fnbase}_SetFrame{sfx});\n\t\n')
                    done_animate.add((aid, sfx))
                par = ins.get('par', a.get('scroll', [1, 1]))
                for pl in _par_lines(par):
                    key = 0 if 'fx' in pl else 1
                    if cur[key] != pl:
                        L.append(pl + '\n')
                        cur[key] = pl
                sx = f'{_fxd(_num(ins.get("x", 0)))}' + (' - fx' if float(par[0]) != 0 else '')
                sy = f'{_fxd(_num(ins.get("y", 0)))}' + (' - fy' if len(par) > 1 and float(par[1]) != 0 else '')
                L.append(f'\t{fnbase}_Draw{sfx}(this, {sx}, {sy});\n\n')
                continue
            it = payload
            fbase = it.get('_fbase') or _field_base(it)
            vbase, k = fbase, 2
            while vbase in used_vars:
                vbase = f'{fbase}_{k}'
                k += 1
            used_vars.add(vbase)
            v = vbase
            par = it.get('scroll', [1, 1])
            pos = it.get('position', [0, 0])
            src = it.get('src')
            size = it.get('size')
            hxs = it.get('hx_scale', [1, 1])
            try:
                hsw, hsh = float(hxs[0]), float(hxs[1])
            except (TypeError, ValueError, IndexError):
                hsw = hsh = 1.0
            # dst = 0.25 x source art x hx display scale. The TIM crop only
            # locates the draw; the art size comes from the source PNG
            # (restored from packed rect / pack scale when unresolvable).
            art = it.get('art')
            if art and len(art) >= 2:
                dw, dh = round(art[0] * 0.25 * hsw), round(art[1] * 0.25 * hsh)
            elif src and len(src) >= 4:
                try:
                    sc0 = it.get('scale', [1, 1])
                    ps = float(sc0[0]) if isinstance(sc0, list) else float(sc0 or 1.0)
                except (TypeError, ValueError, IndexError):
                    ps = 1.0
                if not ps:
                    ps = 1.0
                dw, dh = round(src[2] / ps * 0.25 * hsw), round(src[3] / ps * 0.25 * hsh)
            elif size and len(size) >= 2:
                dw, dh = size[0], size[1]
            else:
                dw, dh = 0, 0
            cond = it.get('cond')
            ind = '\t'
            if cond:
                L.append(f'\tif ({cond})\n\t{{\n')
                ind = '\t\t'
                cur = [None, None]
            if not art and not (src and len(src) >= 4):
                # non-manifest items only: item scale sizes the draw
                sc = it.get('scale', [1, 1])
                if isinstance(sc, (int, float)):
                    sc = [sc, sc]
                try:
                    dw, dh = float(dw) * float(sc[0]), float(dh) * float(sc[1])
                except (TypeError, ValueError, IndexError):
                    pass
            if src and len(src) >= 4:
                L.append(f'{ind}RECT {v}_src = {{{src[0]}, {src[1]}, {src[2]}, {src[3]}}};\n')
            elif size and len(size) >= 2:
                L.append(f'{ind}RECT {v}_src = {{0, 0, {size[0]}, {size[1]}}};\n')
            else:
                L.append(f'{ind}RECT {v}_src = {{0, 0, {sw:g}, {sh:g}}};\n')
            for pl in _par_lines(par, ind):
                key = 0 if 'fx' in pl else 1
                if cur[key] != pl:
                    L.append(pl + '\n')
                    cur[key] = pl
            L.append(f'{ind}RECT_FIXED {v}_dst = {{\n')
            L.append(f'{ind}\t{_fxd(_num(pos[0]))}' + (' - fx' if float(par[0]) != 0 else '') + ',\n')
            L.append(f'{ind}\t{_fxd(_num(pos[1]))}' + (' - fy' if len(par) > 1 and float(par[1]) != 0 else '') + ',\n')
            L.append(f'{ind}\t{_fxd(dw)},\n{ind}\t{_fxd(dh)}\n{ind}}};\n')
            if it.get('blend'):
                bl = it['blend'] if isinstance(it['blend'], dict) else {}
                L.append(f'{ind}Stage_BlendTexV2(&this->tex_{fbase}, &{v}_src, &{v}_dst, '
                         f'stage.camera.bzoom, {bl.get("mode", 0)}, {bl.get("opacity", 128)});\n')
            elif it.get('flip_x'):
                L.append(f'{ind}Stage_DrawTex_FlipX(&this->tex_{fbase}, &{v}_src, &{v}_dst, '
                         f'stage.camera.bzoom, stage.camera.angle);\n')
            else:
                L.append(f'{ind}Stage_DrawTex(&this->tex_{fbase}, &{v}_src, &{v}_dst, '
                         f'stage.camera.bzoom, stage.camera.angle);\n')
            if cond:
                L.append('\t}\n')
            L.append('\n')
        L.append('}\n')
    # Free
    L.append(f'\nvoid {back}_Free(StageBack *back)\n{{\n')
    L.append(f'\t{back} *this = ({back}*)back;\n\t\n')
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        L.append(f'\t//Free {aid} archive\n\tMem_Free(this->arc_{aid});\n\t\n')
    L.append('\t//Free structure\n\tMem_Free(this);\n}\n')
    # New
    L.append(f'\nStageBack *{back}_New(void)\n{{\n')
    L.append(f'\t//Allocate background structure\n\t{back} *this = ({back}*)Mem_Alloc(sizeof({back}));\n')
    L.append('\tif (this == NULL)\n\t\treturn NULL;\n\t\n')
    L.append('\t//Set background functions\n')
    for layer, fn in LAYER_FNS.items():
        attr = {'bg': 'draw_bg', 'md': 'draw_md',
                'fg': 'draw_fg', 'hud': 'draw_hud'}[layer]
        if by_layer[layer]:
            L.append(f'\tthis->back.{attr} = {back}_{fn};\n')
        else:
            L.append(f'\tthis->back.{attr} = NULL;\n')
    L.append(f'\tthis->back.free = {back}_Free;\n')
    # static arcs grouped by path (per-act ARCs when items carry acts)
    groups, gorder = {}, []
    for (fbase, tbase, arc) in triplets:
        if arc not in groups:
            groups[arc] = []
            gorder.append(arc)
        groups[arc].append((fbase, tbase))
    if triplets:
        L.append('\t\n\t//Load background textures\n')
        for gi, arc in enumerate(gorder):
            var = 'arc_back' if gi == 0 else f'arc_back{gi + 1}'
            L.append(f'\tIO_Data {var} = IO_Read("{arc}");\n')
            for (fbase, tbase) in groups[arc]:
                L.append(f'\tGfx_LoadTex(&this->tex_{fbase}, Archive_Find({var}, "{tbase}.tim"), 0);\n')
            L.append(f'\tMem_Free({var});\n')
    for a in actors:
        aid = _sanitize(a.get('name', 'actor'))
        sym = _cap(a.get('sym', aid))
        atable = a.get('animatable', f'{aid}_animatable')
        arc = a.get('arc') or _item_arc(a, arc_upper, def_arc)
        L.append(f'\t\n\t//Load {aid} textures\n')
        L.append(f'\tthis->arc_{aid} = IO_Read("{arc}");\n')
        for ti, tim in enumerate(a.get('tims', [f'{aid}0.tim'])):
            L.append(f'\tthis->arc_{aid}_ptr[{ti}] = Archive_Find(this->arc_{aid}, "{tim}");\n')
        for sfx, anim in a['_states']:
            L.append(f'\t\n\tAnimatable_Init(&this->{atable}{sfx}, {aid}_anim);\n')
            L.append(f'\tAnimatable_SetAnim(&this->{atable}{sfx}, {anim});\n')
            L.append(f'\tthis->{aid}_frame{sfx} = this->{aid}_tex_id{sfx} = 0xFF; //Force art load\n')
    L.append('\t\n\treturn (StageBack*)this;\n}\n')

    os.makedirs(output_dir, exist_ok=True)
    c_path = os.path.join(output_dir, f'{stage_name}.c')
    h_path = os.path.join(output_dir, f'{stage_name}.h')
    with open(c_path, 'w', encoding='utf-8') as f:
        f.write(''.join(L))
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(MPL + f'\n#ifndef PSXF_GUARD_{upper}_H\n#define PSXF_GUARD_{upper}_H\n\n'
                f'#include "../stage.h"\n\n//{_cap(stage_name)} functions\n'
                f'StageBack *{back}_New();\n\n#endif\n')
    return c_path, h_path


def generate_omni_split(stage_name, items, output_dir, extra=None, split=None):
    """One Back_* file per act (each loads only its own ARC), or a single
    file. split=None auto-splits when items carry acts. Returns [(c, h)]."""
    acts = sorted(set(it.get('act') for it in items if it.get('act')))
    if split is None:
        split = bool(acts)
    if not split or not acts:
        c, h = generate_omni_files(stage_name, items, output_dir, extra)
        return [(c, h)]
    noact = [it for it in items if not it.get('act')]
    out = []
    for act in acts:
        sub = [it for it in items if it.get('act') == act] + noact
        sub_name = f'{stage_name}_{act}'
        c, h = generate_omni_files(sub_name, sub, output_dir, extra)
        out.append((c, h))
    return out


def main():
    ap = argparse.ArgumentParser(description='Generate C stage files from stage assets.')
    ap.add_argument('folder_path', help='folder containing stage assets (json, hx, hxs, lua, manifest)')
    ap.add_argument('stage_name', nargs='?', default=None,
                    help='name for generated files (defaults to folder name)')
    ap.add_argument('--format', choices=['legacy', 'omni'], default='omni',
                    help='output format (default: omni)')
    ap.add_argument('--out', default=None, help='output directory (default: <folder>/../generated)')
    ap.add_argument('--pos-scale', type=float, default=0.1,
                    help='FNF px -> PSX px factor for hx positions (default: 0.1)')
    ap.add_argument('--animate', default='*',
                    help="comma-separated alias prefixes folded into animated actors; "
                         "'*' (default) folds every <name><number> sequence")
    ap.add_argument('--split-acts', dest='split_acts', action=argparse.BooleanOptionalAction,
                    default=None,
                    help='one Back_* file per act, each loading only its own ARC '
                         '(default: auto-split when items carry acts)')
    args = ap.parse_args()

    folder_path = args.folder_path
    stage_name = args.stage_name or os.path.basename(os.path.normpath(folder_path))

    if not os.path.isdir(folder_path):
        print(f"Error: {folder_path} is not a valid directory", file=sys.stderr)
        sys.exit(1)

    # Collect stage files: manifest = elements, hx = placements,
    # stars.json (boyfriend key) = character/stage reference
    print(f"Parsing stage assets from: {folder_path}")
    manifest_items, hx_items, lua_items, json_items = [], [], [], []
    placements, stars_meta = {}, {}
    for fn in sorted(os.listdir(folder_path)):
        fp = os.path.join(folder_path, fn)
        if not os.path.isfile(fp):
            continue
        low = fn.lower()
        try:
            if low.endswith('.json'):
                data = parse_json_file(fp)
                if isinstance(data, dict) and 'boyfriend' in data:
                    stars_meta = parse_stars_json(fp)
                    print(f"  stars.json: characters {sorted(stars_meta.get('characters', {}))}")
                elif isinstance(data, dict) and 'items' in data:
                    json_items.extend(data['items'])
                elif isinstance(data, list):
                    json_items.extend(data)
            elif low.endswith('.lua'):
                lua_items.extend(parse_lua_file(fp).get('items', []))
            elif low.endswith('.hx'):
                with open(fp, 'r', encoding='utf-8') as f:
                    hx_content = f.read()
                placements.update(parse_hx_placements(hx_content))
                _d, hx_new = parse_hx_file(fp)
                hx_items.extend(hx_new)
            elif low.endswith(('.txt', '.manifest')):
                manifest_items.extend(parse_manifest_file(fp).get('items', []))
        except Exception as e:
            print(f"  warning: could not parse {fn}: {e}", file=sys.stderr)

    if manifest_items:
        print(f"Found {len(manifest_items)} manifest elements, "
              f"{len(placements)} hx placements")
        # source art dims per item (dst = 0.25 x art x hx); cached by art basename
        png_cache = {}
        srcdir = os.path.join(folder_path, 'source')
        for it in manifest_items:
            base = os.path.splitext(os.path.basename(
                str(it.get('source') or '')))[0].lower()
            if base not in png_cache:
                found = None
                if os.path.isdir(srcdir):
                    for root, _dirs, files in os.walk(srcdir):
                        for fn in files:
                            if fn.lower().endswith('.png') and \
                                    os.path.splitext(fn)[0].lower() == base:
                                found = os.path.join(root, fn)
                                break
                        if found:
                            break
                png_cache[base] = _png_size(found) if found else None
            it['art'] = png_cache[base]
        nart = sum(1 for it in manifest_items if it.get('art'))
        print(f"Resolved source art for {nart}/{len(manifest_items)} elements")
        if placements:
            matched, total = merge_hx_positions(manifest_items, placements,
                                                args.pos_scale)
            print(f"Merged hx positions/scales into {matched}/{total} elements")
            # placements whose dst scale matches no manifest element are lost
            src_bases = set(os.path.splitext(os.path.basename(
                str(it.get('source') or '')))[0].lower()
                for it in manifest_items)
            for var, p in placements.items():
                if p['scale'] != [1.0, 1.0]:
                    base = os.path.splitext(os.path.basename(p['image']))[0].lower()
                    if base not in src_bases:
                        print(f"  note: placement {var} has scale {p['scale']} but no "
                              f"manifest element uses image '{base}' (scale not applied anywhere)")
        xml_index = parse_source_xmls(os.path.join(folder_path, 'source'))
        if xml_index:
            ndefs = sum(len(v['defs']) for v in xml_index.values())
            nopng = sorted(k for k, v in xml_index.items() if not v['png'])
            print(f"Source atlases: {len(xml_index)} XML ({ndefs} frames)"
                  + (f"; no PNG for: {', '.join(nopng)}" if nopng else ""))
        prefixes = tuple(p.strip() for p in args.animate.split(',') if p.strip())
        actors, rest, warnings = group_stat_actors(manifest_items, placements,
                                                   args.pos_scale, prefixes,
                                                   xml_index)
        if actors:
            print(f"Folded frame sequences into {len(actors)} animated actor(s): "
                  + ", ".join(f"{a['name']} ({a.get('act') or 'noact'}, "
                              f"{len(a['frames'])} frames, "
                              f"{len(a['animations'])} anims)" for a in actors))
        for w in warnings[:15]:
            print(f"  warning: {w}", file=sys.stderr)
        if len(warnings) > 15:
            print(f"  ... and {len(warnings) - 15} more warnings", file=sys.stderr)
        # actors.json sidecar: {aid: [{x, y, par, layer, anim, blend?}]}
        # replaces the auto instances of that actor (full control over
        # simultaneous instances and which animation each one plays)
        sidecar = os.path.join(folder_path, 'actors.json')
        if os.path.isfile(sidecar):
            try:
                with open(sidecar, 'r', encoding='utf-8') as f:
                    overrides = json.load(f)
                n = 0
                for a in actors:
                    if a['name'] in overrides and isinstance(overrides[a['name']], list):
                        a['instances'] = overrides[a['name']]
                        n += 1
                print(f"actors.json: overrode instances of {n} actor(s)")
            except Exception as e:
                print(f"  warning: could not parse actors.json: {e}", file=sys.stderr)
        items = rest + actors + lua_items + json_items
    else:
        if HAVE_LEGACY:
            items = parse_all_stages(folder_path).get('items', [])
        else:
            items = hx_items + lua_items + json_items
    print(f"Found {len(items)} stage items")

    output_dir = args.out or os.path.join(os.path.dirname(os.path.abspath(folder_path)), "generated")
    os.makedirs(output_dir, exist_ok=True)

    print(f"\nGenerating {args.format} C/H files for stage: {stage_name}")
    if args.format == 'omni':
        for c_path, h_path in generate_omni_split(stage_name, items, output_dir,
                                                  stars_meta or None,
                                                  args.split_acts):
            print(f"Generated source file: {c_path}")
            print(f"Generated header file: {h_path}")
    else:
        if not HAVE_LEGACY:
            print("Error: legacy generator (generate_stage_ch) not found; use --format omni",
                  file=sys.stderr)
            sys.exit(1)
        header_path = StageFileGenerator.generate_header_file(stage_name, output_dir)
        source_path = StageFileGenerator.generate_source_file(
            stage_name,
            items,
            output_dir
        )

        print(f"Generated header file: {header_path}")
        print(f"Generated source file: {source_path}")


if __name__ == '__main__':
    main()