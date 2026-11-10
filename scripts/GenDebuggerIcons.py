"""Generate the vector icons of the debugger's bars and the input picker.

Every icon is defined below as strokes and solids on a 20-unit box and
written out as filled SVG path data, the form DxuiVectorIcon draws. A stroke
becomes a closed outline with mitered joins and butt ends. Sub-paths of one
layer are filled together by the nonzero winding rule, so solids are wound
clockwise and holes counterclockwise.

Run:  python scripts/GenDebuggerIcons.py
"""

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOX  = 20.0
CY   = 10.0

KAPPA = 0.5522847498


# --------------------------------------------------------------------------
#  Geometry
# --------------------------------------------------------------------------

def area(points):
    total = 0.0
    for i, (x0, y0) in enumerate(points):
        x1, y1 = points[(i + 1) % len(points)]
        total += x0 * y1 - x1 * y0
    return total / 2.0


def wound(points, clockwise):
    """The points in the given direction. Clockwise on screen has a positive area, y being down."""
    return list(points) if (area(points) > 0) == clockwise else list(reversed(points))


def unit(dx, dy):
    length = math.hypot(dx, dy)
    return dx / length, dy / length


def offset(points, distance, closed):
    """Each point moved `distance` to the left of the direction of travel, joins mitered."""
    out   = []
    count = len(points)

    for i, (x, y) in enumerate(points):
        before = points[i - 1] if (closed or i > 0) else None
        after  = points[(i + 1) % count] if (closed or i < count - 1) else None
        n_in   = None
        n_out  = None

        if before is not None:
            ux, uy = unit(x - before[0], y - before[1])
            n_in   = (uy, -ux)

        if after is not None:
            ux, uy = unit(after[0] - x, after[1] - y)
            n_out  = (uy, -ux)

        if n_in is None:
            n_in = n_out
        if n_out is None:
            n_out = n_in

        mx, my = n_in[0] + n_out[0], n_in[1] + n_out[1]
        length = math.hypot(mx, my)
        mx, my = mx / length, my / length
        scale  = distance / (mx * n_in[0] + my * n_in[1])

        out.append((x + mx * scale, y + my * scale))

    return out


def fmt(value):
    text = f'{value:.2f}'.rstrip('0').rstrip('.')
    return '0' if text in ('-0', '') else text


def polygon(points, clockwise=True):
    points = wound(points, clockwise)
    return 'M' + 'L'.join(f'{fmt(x)} {fmt(y)}' for x, y in points) + 'Z'


def stroke(points, width):
    """An open polyline of the given width as one solid."""
    left  = offset(points,  width / 2.0, False)
    right = offset(points, -width / 2.0, False)
    return [polygon(left + list(reversed(right)))]


def stroke_closed(points, width):
    """A closed polyline of the given width, centered on it: a solid and its hole."""
    a = offset(points,  width / 2.0, True)
    b = offset(points, -width / 2.0, True)
    outer, inner = (a, b) if abs(area(a)) > abs(area(b)) else (b, a)
    return [polygon(outer, True), polygon(inner, False)]


def rounded_rect(left, top, right, bottom, radius, clockwise=True):
    r = max(0.0, min(radius, (right - left) / 2.0, (bottom - top) / 2.0))
    k = r * (1.0 - KAPPA)

    if r == 0.0:
        return polygon([(left, top), (right, top), (right, bottom), (left, bottom)], clockwise)

    f = fmt

    if clockwise:
        return (f'M{f(left + r)} {f(top)}H{f(right - r)}'
                f'C{f(right - k)} {f(top)} {f(right)} {f(top + k)} {f(right)} {f(top + r)}'
                f'V{f(bottom - r)}'
                f'C{f(right)} {f(bottom - k)} {f(right - k)} {f(bottom)} {f(right - r)} {f(bottom)}'
                f'H{f(left + r)}'
                f'C{f(left + k)} {f(bottom)} {f(left)} {f(bottom - k)} {f(left)} {f(bottom - r)}'
                f'V{f(top + r)}'
                f'C{f(left)} {f(top + k)} {f(left + k)} {f(top)} {f(left + r)} {f(top)}Z')

    return (f'M{f(left + r)} {f(top)}'
            f'C{f(left + k)} {f(top)} {f(left)} {f(top + k)} {f(left)} {f(top + r)}'
            f'V{f(bottom - r)}'
            f'C{f(left)} {f(bottom - k)} {f(left + k)} {f(bottom)} {f(left + r)} {f(bottom)}'
            f'H{f(right - r)}'
            f'C{f(right - k)} {f(bottom)} {f(right)} {f(bottom - k)} {f(right)} {f(bottom - r)}'
            f'V{f(top + r)}'
            f'C{f(right)} {f(top + k)} {f(right - k)} {f(top)} {f(right - r)} {f(top)}Z')


def disc(cx, cy, r, clockwise=True):
    return rounded_rect(cx - r, cy - r, cx + r, cy + r, r, clockwise)


def ring(cx, cy, r, width):
    """A circle of the given stroke width, centered on the radius."""
    return [disc(cx, cy, r + width / 2.0, True), disc(cx, cy, r - width / 2.0, False)]


def frame(left, top, right, bottom, width, radius=0.0):
    """A rectangle's outline, drawn inside its bounds."""
    return [rounded_rect(left, top, right, bottom, radius, True),
            rounded_rect(left + width, top + width, right - width, bottom - width, max(0.0, radius - width), False)]


def arrow(start, tip, head, width):
    """A shaft from `start` to `tip` and a right-angled head whose arms are `head` long on each axis.

    The head's center line ends short of the tip by the amount the miter adds,
    so the point is where two butt-ended strokes meeting at the tip would end.
    """
    dx, dy = unit(tip[0] - start[0], tip[1] - start[1])
    pull   = width * (math.sqrt(0.5) - 0.5 * math.sqrt(0.5))
    apex   = (tip[0] - dx * pull, tip[1] - dy * pull)
    bx, by = -dx * head, -dy * head
    arm_a  = (apex[0] + bx - by, apex[1] + by + bx)
    arm_b  = (apex[0] + bx + by, apex[1] + by - bx)

    return stroke([start, apex], width) + stroke([arm_a, apex, arm_b], width)


def arc_points(cx, cy, rx, ry, a0, a1, segments):
    return [(cx + rx * math.cos(a0 + (a1 - a0) * i / segments),
             cy + ry * math.sin(a0 + (a1 - a0) * i / segments)) for i in range(segments + 1)]


def crescent(cx, cy, r, cut_x, cut_y, segments=24):
    """The disc at (cx, cy) less an equal disc at (cut_x, cut_y)."""
    away  = math.atan2(cy - cut_y, cx - cut_x)
    half  = math.acos(math.hypot(cx - cut_x, cy - cut_y) / (2.0 * r))
    # On the first disc the crescent's outer edge runs the long way round, through `away`.
    outer = arc_points(cx, cy, r, r, away - (math.pi - half), away + (math.pi - half), segments)
    # On the cutting disc its inner edge runs back through `away` as well.
    inner = arc_points(cut_x, cut_y, r, r, away + half, away - half, segments // 2)
    return [polygon(outer + inner[1:-1])]


# --------------------------------------------------------------------------
#  The debugger bar. Shapes follow Visual Studio's debugging toolbar.
# --------------------------------------------------------------------------

W = BOX / 7.5   # the bar's stroke

STEP_DOT = disc(10.0, CY + 7.2, 2.2)
BACK_DOT = disc(4.0,  CY - 6.0, 2.2)


def step_over(mirror):
    side   = -1.0 if mirror else 1.0
    x_end  = 10.0 + side * 7.2
    pull   = W * 0.5 * math.sqrt(0.5)
    start  = math.pi if not mirror else 0.0
    sweep  = math.pi if not mirror else -math.pi
    over   = arc_points(10.0, CY - 0.8, 7.2, 8.4, start, start + sweep, 24)
    tip    = (x_end, CY + 4.0)
    apex   = (x_end, tip[1] - pull)

    return (stroke(over + [apex], W) +
            stroke([(x_end + 4.4, apex[1] - 4.4), apex, (x_end - 4.4, apex[1] - 4.4)], W) +
            [STEP_DOT])


ACCENT = True
INK    = False

# name -> (comment, [(sub-paths, accent, opacity)])
DEBUGGER = {
    'Run':          ('A triangle pointing right',
                     [([polygon([(3.6, CY - 8.4), (18.0, CY), (3.6, CY + 8.4)])], ACCENT, 1.0)]),
    'Pause':        ('Two upright rounded bars',
                     [([rounded_rect(3.2, CY - 8.0, 8.0, CY + 8.0, 2.4),
                        rounded_rect(12.0, CY - 8.0, 16.8, CY + 8.0, 2.4)], INK, 1.0)]),
    'StepInto':     ('An arrow down to a dot',
                     [(arrow((10.0, CY - 9.2), (10.0, CY + 2.4), 4.8, W) + [STEP_DOT], ACCENT, 1.0)]),
    'StepOver':     ('An arc over a dot, ending in an arrow down on the right',
                     [(step_over(False), ACCENT, 1.0)]),
    'StepOut':      ('An arrow up from a dot',
                     [(arrow((10.0, CY + 2.4), (10.0, CY - 9.2), 4.8, W) + [STEP_DOT], ACCENT, 1.0)]),
    'RunToCursor':  ('An arrow right, ending at an upright bar',
                     [(arrow((0.4, CY), (14.4, CY), 6.0, W), ACCENT, 1.0),
                      (stroke([(18.4, CY - 8.4), (18.4, CY + 8.4)], W), INK, 1.0)]),
    'StepBackInto': ('An arrow up and to the left, into a dot',
                     [(arrow((17.6, CY + 7.6), (8.0, CY - 2.0), 4.8, W) + [BACK_DOT], ACCENT, 1.0)]),
    'StepBackOver': ('An arc over a dot, ending in an arrow down on the left',
                     [(step_over(True), ACCENT, 1.0)]),
    'StepBackOut':  ('An arrow down and to the right, away from a dot',
                     [(arrow((8.0, CY - 2.0), (17.6, CY + 7.6), 4.8, W) + [BACK_DOT], ACCENT, 1.0)]),
    'ReverseRun':   ('A triangle pointing left',
                     [([polygon([(16.4, CY - 8.4), (2.0, CY), (16.4, CY + 8.4)])], ACCENT, 1.0)]),
    'ShowNext':     ('A plain arrow pointing right',
                     [(arrow((1.6, CY), (18.4, CY), 6.8, W), INK, 1.0)]),
}


def load_file():
    """A two-by-two grid of 1s and 0s, a quarter larger than the box and centered on it."""
    size  = BOX * 1.25
    cell  = size / 2.0
    left  = -BOX * 0.125
    top   = CY - size / 2.0
    thin  = size / 14.0
    high  = cell * 0.78
    wide  = cell * 0.48
    ones  = []
    zeros = []

    for row, bits in enumerate(((1, 0), (0, 1))):
        for col, bit in enumerate(bits):
            cx = left + cell * (col + 0.5)
            cy = top + cell * (row + 0.5)

            if bit:
                ones += stroke([(cx, cy - high / 2.0), (cx, cy + high / 2.0)], thin)
            else:
                zeros += frame(cx - wide / 2.0, cy - high / 2.0, cx + wide / 2.0, cy + high / 2.0, thin, wide * 0.45)

    return [(ones, ACCENT, 1.0), (zeros, INK, 1.0)]


DEBUGGER['LoadFile'] = ('Bytes of a file: a grid of 1s and 0s', load_file())


# --------------------------------------------------------------------------
#  The Breakpoints bar.
# --------------------------------------------------------------------------

T = BOX / 12.0   # the Breakpoints bar's stroke


def balls(cross):
    back   = crescent(13.2, CY + 1.6, 6.0, 8.8, CY + 2.8)
    front  = [disc(8.8, CY + 2.8, 6.0)]
    layers = [(back, INK, 0.5), (front, INK, 1.0)]

    if cross:
        cx, cy, arm = 4.0, CY - 6.0, 2.6
        layers.append((stroke([(cx - arm, cy - arm), (cx + arm, cy + arm)], T * 1.5) +
                       stroke([(cx - arm, cy + arm), (cx + arm, cy - arm)], T * 1.5), ACCENT, 1.0))

    return layers


def go_to(bracket):
    paths = []

    for i in range(3):
        y = CY - 6.4 + i * 6.4
        paths += stroke([(8.4, y), (20.0, y)], T)

    if bracket:
        paths += stroke([(5.6, CY - 8.4), (2.4, CY - 8.4), (2.4, CY + 8.4), (5.6, CY + 8.4)], T)
    else:
        paths += arrow((0.0, CY - 6.4), (6.0, CY - 6.4), 3.2, T)

    return [(paths, INK, 1.0)]


def file_arrow(out):
    start, tip = ((16.0, CY), (1.6, CY)) if out else ((1.0, CY), (15.2, CY))

    return [(stroke([(18.4, CY - 7.2), (18.4, CY + 7.2)], T) + arrow(start, tip, 5.6, T), INK, 1.0)]


BREAKPOINTS = {
    'DeleteAll':  ('Two breakpoint marks, one behind the other, under a cross', balls(True)),
    'DisableAll': ('Two breakpoint marks, one behind the other',               balls(False)),
    'GoToSource': ('Lines of text and an arrow into the first',                go_to(False)),
    'GoToCode':   ('Lines of text behind a bracket',                           go_to(True)),
    'Export':     ('An arrow pointing left, away from a bar',                  file_arrow(True)),
    'Import':     ('An arrow pointing right, into a bar',                      file_arrow(False)),
}


# --------------------------------------------------------------------------
#  The input picker's devices, the outlines InputMonoGlyphs draws.
# --------------------------------------------------------------------------

G = BOX / 15.0   # the glyphs' stroke


def joystick():
    return (ring(10.0, 4.2, 3.2, G) +
            stroke([(10.0, 7.4), (10.0, 13.2)], G) +
            frame(3.2, 13.2, 16.8, 17.2, G))


def paddle():
    a_left  = math.radians(160.0)
    a_right = math.radians(380.0)
    outline = arc_points(10.0, 6.8, 4.8, 4.8, a_left, a_right, 24) + [(12.6, 17.6), (7.4, 17.6)]

    return stroke_closed(outline, G) + ring(10.0, 6.8, 4.8 * 0.42, G)


def gamepad():
    left, right, top, bottom, grip = 3.2, 16.8, 6.0, 12.4, 16.0
    outline = [(left, top), (right, top), (right, bottom), (right - 1.2, grip), (11.2, bottom),
               (8.8, bottom), (left + 1.2, grip), (left, bottom)]

    return stroke_closed(outline, G) + ring(6.0, 9.2, 1.7, G) + ring(14.0, 9.2, 1.7, G)


def keys():
    key, gap, row = 5.2, 0.8, 10.4
    up    = row - key - gap
    paths = []

    for x, y in ((10.0 - key / 2.0, up), (10.0 - key / 2.0, row), (10.0 - key * 1.5 - gap, row), (10.0 + key / 2.0 + gap, row)):
        paths += frame(x, y, x + key, y + key, G)

    return paths


INPUT = {
    'Gamepad':  ('A body with two grips and two thumbsticks', [(gamepad(),  INK, 1.0)]),
    'Joystick': ('Ball, stick and slab',                      [(joystick(), INK, 1.0)]),
    'Paddle':   ('A knob at the head of a tapering body',     [(paddle(),   INK, 1.0)]),
    'Keys':     ('Four arrow keys in an inverted T',          [(keys(),     INK, 1.0)]),
}


# --------------------------------------------------------------------------
#  Output
# --------------------------------------------------------------------------

def wrap(d, width=100):
    lines = []
    while len(d) > width:
        cut = d.rfind(' ', 0, width)
        lines.append(d[:cut + 1])
        d = d[cut + 1:]
    lines.append(d)
    return lines


def write(path, namespace, about, icons):
    lines = []
    out   = lines.append
    bar   = '/' * 80

    out('#pragma once')
    out('')
    out('')
    out('')
    out('')
    out('')
    out(bar)
    out('//')
    out(f'//  {namespace}')
    out('//')
    out('//  GENERATED by scripts/GenDebuggerIcons.py. Edit the script, not this file.')
    out('//')
    for line in about:
        out(f'//  {line}')
    out('//')
    out(bar)
    out('')
    out(f'namespace {namespace}')
    out('{')

    for name, (comment, layers) in icons.items():
        index = 0
        paths = []

        out('')
        out(f'    //  {comment}')
        out(f'    static constexpr DxuiVectorIconLayer  s_k{name}Layers[] =')
        out('    {')

        for subpaths, accent, opacity in layers:
            mask = sum(1 << (index + i) for i in range(len(subpaths)))
            index += len(subpaths)
            paths += subpaths
            flag  = 'true ' if accent else 'false'
            out(f'        {{ 0x{mask:02X}u, {flag}, 0.0f, 0.0f, {opacity:.1f}f }},')

        assert index <= 32, name

        out('    };')
        out('')
        out(f'    static constexpr DxuiVectorIcon  s_k{name} =')
        out('    {')
        for piece in wrap(''.join(paths)):
            out(f'        L"{piece}"')
        lines[-1] += ','
        out(f'        {BOX:.1f}f, s_k{name}Layers, std::size (s_k{name}Layers)')
        out('    };')
        out('')

    out('}')
    out('')

    path.write_text('\r\n'.join(lines), encoding='ascii', newline='')
    print(f'wrote {path.relative_to(ROOT)} ({len(icons)} icons)')


def main():
    write(ROOT / 'CassoEmuCore' / 'Ui' / 'Debugger' / 'DebuggerIcons.h', 'DebuggerIcons',
          ["The debugger bar's and the Breakpoints bar's icons, in the forms of",
           "Visual Studio's debugging toolbar. The accent part of each is the part",
           'drawn in the run or step color.'],
          {**DEBUGGER, **BREAKPOINTS})

    write(ROOT / 'CassoEmuCore' / 'Ui' / 'Chrome' / 'InputGlyphIcons.h', 'InputGlyphIcons',
          ["The input picker's device icons, in the outlines of InputMonoGlyphs."],
          INPUT)


if __name__ == '__main__':
    main()
