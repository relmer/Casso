"""Build CassoEmuCore/CassoExplorer/CassoExplorerIcons.h from the Fluent SVGs.

Most icons are Microsoft's Fluent UI System Icons (MIT; the license is in
Resources/Icons/Fluent/LICENSE), at 20 pixels: outlines, and the filled
dots where Explorer's overflow is that weight. Each is one path
of several sub-shapes, and the table below says which sub-shapes File
Explorer draws in the accent -- the plus inside New's ring, the arrow Sort
points down with -- and which in the foreground ink. The indices were read
off each icon drawn with its sub-shapes numbered; Explorer's own command bar,
captured with a file selected, gave the tones.

Run after changing an icon or its tones:

    python scripts/GenFluentIcons.py
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'Resources' / 'Icons' / 'Fluent'
OUTPUT = ROOT / 'CassoEmuCore' / 'CassoExplorer' / 'CassoExplorerIcons.h'

#  name -> (file stem after ic_fluent_, [(sub-shape indices, accent, band
#  top, band bottom)]). Stems end in the size and the style.
#  A band of (0, 0) is the whole icon.
ALL = 'all'

ICONS = {
    'New':     ('add_circle_20_regular',        [([0], True, 0, 0), ([1, 2], False, 0, 0)]),
    #  The scissors' handle loops and blades are one outline, so the tones
    #  split where the loops begin rather than by sub-shape.
    'Cut':     ('cut_20_regular',               [([0, 1, 2, 3], False, 0, 11.2), ([0, 1, 2], True, 11.2, 20)]),
    'Copy':    ('copy_20_regular',              [([0], False, 0, 0), ([1, 2], True, 0, 0)]),
    'Paste':   ('clipboard_paste_20_regular',   [([0, 1], False, 0, 0), ([2, 3], True, 0, 0)]),
    #  Explorer's Rename is the A in brackets with a text cursor: the A and
    #  the cursor in the accent, the brackets in the ink.
    'Rename':  ('rename_a_20_regular',          [([0, 3, 4], True, 0, 0), ([1, 2], False, 0, 0)]),
    #  Explorer's Share: the box in the ink, the arrow leaving it in the accent.
    'Share':   ('share_20_regular',             [([0], False, 0, 0), ([1, 2], True, 0, 0)]),
    'Delete':  ('delete_16_regular',            ALL),
    'Sort':    ('arrow_sort_24_regular',        [([1], False, 0, 0), ([0], True, 0, 0)]),
    #  The filled dots, which are Explorer's weight; the outline ones are thin.
    'More':    ('more_horizontal_20_filled',    [([0, 1, 2], False, 0, 0)]),
    'Preview': ('panel_right_20_regular',       [([0, 1, 2], False, 0, 0)]),
    'Theme':   ('dark_theme_20_regular',        [([0, 1], False, 0, 0)]),

    #  The View menu's rows. ALL is every sub-shape, in the foreground ink.
    'ViewSmallIcons': ('grid_20_regular',             ALL),
    'ViewList':       ('text_column_two_20_regular',  ALL),
    'ViewDetails':    ('line_horizontal_4_20_regular', ALL),
    'ViewTiles':      ('apps_list_detail_20_regular', ALL),
    'ViewContent':    ('apps_list_20_regular',        ALL),
}

#  Explorer's three icon views are a screen over a stand, smaller for each
#  step down, and Fluent has no such set. These are drawn here instead: a
#  frame a unit thick with rounded corners, and the stand as a bar under it.
#  name -> (frame left, top, right, bottom, bar top).
FRAMES = {
    'ViewExtraLargeIcons': (2.0, 3.5, 18.0, 14.5, 16.0),
    'ViewLargeIcons':      (3.5, 5.0, 16.5, 13.0, 14.5),
    'ViewMediumIcons':     (5.5, 6.5, 14.5, 12.0, 13.5),
}

#  File Explorer draws these three from the Segoe Fluent Icons font at 16 dip,
#  whose drawings are smaller than Fluent's 20-pixel SVGs, or (Sort) of other
#  proportions. Each is scaled about its view box's center by the factor given,
#  in 20-unit pixels per source unit, so its ink matches the font's: the
#  font's Share is 22 by 21 pixels at 150%, Delete 22 by 24 and Sort 24 by 18,
#  where a 20-unit pixel is 1.5 screen pixels. The second value moves it down,
#  in 20-unit pixels, to sit where the font's does.
FIT = {
    'Share':  (0.917, 0.67),   # 16 units of ink to 14.67
    'Delete': (1.128, 0.0),    # 13 units of ink to 14.67
    'Sort':   (0.762, 0.0),    # 21 units of ink to 16
}

B = '/' * 80


def rounded_rect(left, top, right, bottom, radius, clockwise):
    """One closed sub-shape. The fill rule is nonzero, so a frame is an
    outer shape one way round and its hole the other."""
    r = radius
    n = lambda v: f'{v:g}'

    if clockwise:
        return (f'M{n(left + r)} {n(top)}H{n(right - r)}A{n(r)} {n(r)} 0 0 1 {n(right)} {n(top + r)}'
                f'V{n(bottom - r)}A{n(r)} {n(r)} 0 0 1 {n(right - r)} {n(bottom)}'
                f'H{n(left + r)}A{n(r)} {n(r)} 0 0 1 {n(left)} {n(bottom - r)}'
                f'V{n(top + r)}A{n(r)} {n(r)} 0 0 1 {n(left + r)} {n(top)}Z')

    return (f'M{n(left + r)} {n(top)}A{n(r)} {n(r)} 0 0 0 {n(left)} {n(top + r)}'
            f'V{n(bottom - r)}A{n(r)} {n(r)} 0 0 0 {n(left + r)} {n(bottom)}'
            f'H{n(right - r)}A{n(r)} {n(r)} 0 0 0 {n(right)} {n(bottom - r)}'
            f'V{n(top + r)}A{n(r)} {n(r)} 0 0 0 {n(right - r)} {n(top)}Z')


def frame_path(left, top, right, bottom, bar_top):
    inset = 1.5 if (right - left) > 12 else 1.0

    return (rounded_rect(left, top, right, bottom, 2.0, True)
            + rounded_rect(left + 1, top + 1, right - 1, bottom - 1, 1.0, False)
            + rounded_rect(left + inset, bar_top, right - inset, bar_top + 1, 0.5, True))


def bars_path(count, left, right, pitch, thickness):
    """Bars `thickness` thick, `pitch` apart, centered on the icon."""
    first = 10.0 - pitch * (count - 1) / 2.0

    return ''.join(rounded_rect(left, first + i * pitch - thickness / 2.0, right, first + i * pitch + thickness / 2.0, thickness / 2.0, True) for i in range(count))


def path_of(name):
    svg = (SOURCE / f'ic_fluent_{name}.svg').read_text(encoding='utf-8')
    paths = re.findall(r'<path[^>]*\sd="([^"]+)"', svg)
    assert len(paths) == 1, f'{name}: expected one path, found {len(paths)}'
    return paths[0]


def fit_path(d, size, k, dy):
    "Scales an absolute-command path about its view box's center into 20 units."
    tokens = re.findall(r'[A-Za-z]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?', d)
    result = []
    command = None
    index = 0

    def x(v):
        return 10.0 + (float(v) - size / 2.0) * k

    def y(v):
        return 10.0 + dy + (float(v) - size / 2.0) * k

    while index < len(tokens):
        token = tokens[index]

        if token.isalpha():
            assert token.isupper() or token in 'Zz', f'relative command {token}'
            command = token
            result.append(token)
            index += 1
            continue

        if command in 'MLT':
            result.append(f'{x(tokens[index]):.4f} {y(tokens[index + 1]):.4f}')
            index += 2
        elif command == 'H':
            result.append(f'{x(tokens[index]):.4f}')
            index += 1
        elif command == 'V':
            result.append(f'{y(tokens[index]):.4f}')
            index += 1
        elif command in 'CSQ':
            result.append(f'{x(tokens[index]):.4f} {y(tokens[index + 1]):.4f}')
            index += 2
        elif command == 'A':
            rx, ry, rot, large, sweep = tokens[index:index + 5]
            result.append(f'{float(rx) * k:.4f} {float(ry) * k:.4f} {rot} {large} {sweep} {x(tokens[index + 5]):.4f} {y(tokens[index + 6]):.4f}')
            index += 7
        else:
            raise AssertionError(f'unexpected command {command}')

    return ' '.join(result)


def mask(indices):
    value = 0

    for i in indices:
        value |= 1 << i

    return value


def main():
    lines = []
    out = lines.append

    out('#pragma once')
    out('')
    out('')
    out('')
    out('')
    out('')
    out(B)
    out('//')
    out('//  CassoExplorerIcons')
    out('//')
    out('//  GENERATED by scripts/GenFluentIcons.py from Resources/Icons/Fluent.')
    out('//  Edit the script, not this file.')
    out('//')
    out("//  The command bar's and the View menu's icons: Microsoft's Fluent UI")
    out('//  System Icons, MIT licensed, in the two tones File Explorer gives them,')
    out("//  and the three icon views' frames, which the script draws itself.")
    out('//')
    out(B)
    out('')
    out('namespace CassoExplorerIcons')
    out('{')

    entries = [(name, svg, fit_path(path_of(svg), float(re.search(r'_(\d+)_', svg).group(1)), *FIT[name]) if name in FIT else path_of(svg), layers)
               for name, (svg, layers) in ICONS.items()]
    entries += [(name, 'drawn by this script', frame_path(*frame), ALL) for name, frame in FRAMES.items()]

    #  Explorer's View is four bars a pixel thick and 5 pixels apart at 150%,
    #  thinner and closer than Fluent's line_horizontal_4.
    entries += [('View', 'drawn by this script', bars_path(4, 2.0, 18.0, 10.0 / 3.0, 2.0 / 3.0), ALL)]

    for name, svg, d, layers in entries:
        if layers == ALL:
            layers = [(range(d.count('M') + d.count('m')), False, 0, 0)]

        #  One string literal per line, so the header stays readable.
        chunks = re.findall(r'.{1,100}(?:\s|$)|.{1,100}', d)
        chunks = [c for c in chunks if c]

        out('')
        out(f'    //  {svg}')
        out(f'    static constexpr DxuiVectorIconLayer  s_k{name}Layers[] =')
        out('    {')

        for indices, accent, top, bottom in layers:
            out(f'        {{ 0x{mask(indices):02X}u, {"true " if accent else "false"}, {top:.1f}f, {bottom:.1f}f }},')

        out('    };')
        out('')
        out(f'    static constexpr DxuiVectorIcon  s_k{name} =')
        out('    {')

        for i, chunk in enumerate(chunks):
            lead = '        L"'
            out(f'{lead}{chunk}"' + (',' if i == len(chunks) - 1 else ''))

        out(f'        20.0f, s_k{name}Layers, std::size (s_k{name}Layers)')
        out('    };')

    out('}')
    out('')

    OUTPUT.write_text('\r\n'.join(lines), encoding='utf-8', newline='')
    print(f'wrote {OUTPUT.relative_to(ROOT)} ({len(entries)} icons)')


if __name__ == '__main__':
    main()
