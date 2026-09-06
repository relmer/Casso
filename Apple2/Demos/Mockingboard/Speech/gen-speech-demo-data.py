"""Emit the .a65 data tables for the SSI-263 speech demo, in both its versions.

There are two demos and one voice. mockingboard-speech-demo-hgr.a65 draws HAL's
eye in hi-res over 40-column captions; mockingboard-speech-demo-dhgr.a65 draws
it in double hi-res over 80-column ones. They speak the same lines and sing the
same song, so the speech and the song are written once here and the eye twice,
and each version gets its own .inc.

Pitch: f = XCK / (8 * (4096 - I)), so I = round(4096 - XCK/8/f). The 12 bits are
scattered -- I10..I3 -> $C441, I11 -> $C442 bit 3, I2..I0 -> $C442 bits 2..0 --
and $C442's HIGH nibble is the speech rate. Every pitch-low byte therefore folds
the rate in; emitting a bare pitch there silently resets the chip to rate 0,
which is what made the question run 1.6x slow.

Durations: heard_ms = 4.005 * (16 - R) * (4 - D) under the current emulator
(the phoneme countdown is loaded in XCK ticks and drained in 6502 cycles, an
exact 14/8 = 1.75x overshoot). At rate 8 that gives 32 / 64 / 96 / 128 ms for
DR 3 / 2 / 1 / 0. Vowels take the long classes and consonants the short ones, so
syllables have the duration contrast the ear segments on.
"""

import os
import sys

#  Write the files rather than printing them. This script printed to stdout, so a
#  caller that forgot to redirect it changed nothing at all while appearing to
#  succeed -- which is exactly what happened, twice, to an edit of the pause
#  lengths in the HAL line.
_HERE = os.path.dirname (os.path.abspath (__file__))

XCK = 1789772.5
K = XCK / 8.0

PH = {
    "PA": 0x00, "E": 0x01, "E1": 0x02, "Y": 0x03, "YI": 0x04, "AY": 0x05,
    "IE": 0x06, "I": 0x07, "A": 0x08, "AI": 0x09, "EH": 0x0A, "EH1": 0x0B,
    "AE": 0x0C, "AE1": 0x0D, "AH": 0x0E, "AH1": 0x0F, "AW": 0x10, "O": 0x11,
    "OU": 0x12, "OO": 0x13, "IU": 0x14, "IU1": 0x15, "U": 0x16, "U1": 0x17,
    "UH": 0x18, "UH1": 0x19, "UH2": 0x1A, "UH3": 0x1B, "ER": 0x1C, "R": 0x1D,
    "R1": 0x1E, "R2": 0x1F, "L": 0x20, "L1": 0x21, "LF": 0x22, "W": 0x23,
    "B": 0x24, "D": 0x25, "KV": 0x26, "P": 0x27, "T": 0x28, "K": 0x29,
    "HV": 0x2A, "HVC": 0x2B, "HF": 0x2C, "HFC": 0x2D, "HN": 0x2E, "Z": 0x2F,
    "S": 0x30, "J": 0x31, "SCH": 0x32, "V": 0x33, "F": 0x34, "THV": 0x35,
    "TH": 0x36, "M": 0x37, "N": 0x38, "NG": 0x39,
}

LONG, MED, SHORT, QUICK = 0x00, 0x40, 0x80, 0xC0   # DR bits, longest to shortest
RATE_SPEECH = 0x80        # rate 8: 32/64/96/128 ms per DR class
RATE_QUESTION = 0x60      # rate 6: 40/80/120/160 ms -- the question, unhurried
RATE_SLOW   = 0x50        # rate 5: 44/88/132/176 ms -- the Sarah interjection
RATE_HAL    = 0x40        # rate 4: 48/96/144/192 ms -- deliberate, not sluggish
BASE_HZ     = 112.0       # speaking pitch; well clear of the 54.6 Hz floor
#  Not measured, though not for want of trying. The pod bay scene carries a
#  steady drone in the same range as a speaking voice, and HAL is quiet and
#  smooth on top of it. Three pitch trackers -- autocorrelation, harmonic sum
#  over a subtracted background, and cepstrum -- each returned a CONFIDENT
#  pitch for a window of that scene in which nobody speaks at all: 173, 72
#  and 167 Hz. Their answers for HAL himself spanned 75 to 171 Hz, a factor
#  of two, which is not a measurement. Dave's louder line came out at 115 to
#  125 Hz on all three, so the method works where a voice dominates its
#  background; it cannot do this one.
#
#  So this stays a judgement, and the judgement is made by ear rather than by
#  me: 90 Hz is where it started and where it is back. The two departures were
#  both mine -- 105 from my own estimate of a calm male range, and 171 from
#  the bad measurement above, which was heard as far too high straight away.
HAL_HZ      = 90.0

CAP  = 0xFB               # caption escape: $FB len <high-ASCII bytes>, len 0 = clear
FILT = 0xFE
PITCH= 0xFD
WAIT = 0xFC
END  = 0xFF


def note_i(f):
    return max(0, min(4095, int(round(4096 - K / f))))


def pitch_bytes(f, rate):
    """(hi, lo) for $C441/$C442, with the rate nibble folded into lo."""
    i = note_i(f)
    return (i >> 3) & 0xFF, (rate & 0xF0) | ((i >> 8) & 0x08) | (i & 0x07)


def P(name, dur=MED):
    return PH[name] | dur


def cap(text):
    """Caption escape: shows the word as it starts being spoken."""
    b = [CAP, len(text)]
    return b + [ord(c) | 0x80 for c in text]


def clear_caption():
    return [CAP, 0x00]


def setp(f, rate=RATE_SPEECH):
    hi, lo = pitch_bytes(f, rate)
    return [PITCH, hi, lo]


# ---------------------------------------------------------------- speech ---
# One caption escape per word, placed at the head of that word's phonemes so
# the text appears as the word begins. Vowels MED/LONG, consonants SHORT/QUICK.
# PA appears only as a real pause or a genuine pre-stop closure after a vowel --
# never inside a cluster like the /sp/ of "speech", which is what punched
# silent holes through the middle of words.

def wargames():
    """A question contour, not a step to a held note.

    English yes/no questions fall to a low turning point on the nuclear
    syllable and then rise continuously, still climbing when the sound stops.
    A single jump to a steady pitch is the definition of a sung note, which is
    why the first attempt sounded musical. The rise is drawn one pitch value
    per one-frame phoneme -- the sustain trick in reverse -- so every step is
    under ~1.2 semitones and none is held long enough to read as a note.
    """
    q = lambda f: setp(f, RATE_QUESTION)
    s = clear_caption()
    # The greeting is a statement: it falls, and pauses. The question that
    # follows keeps the same caption area rather than clearing it, so the two
    # sentences are read together the way they are heard. That fits one
    # 80-column row and wraps onto a second at 40, which the word wrap already
    # handles and which three caption rows leave room for.
    s += q(112.0) + cap("GREETINGS,") + [P("KV", QUICK), P("R1", SHORT), P("E", LONG), P("T", QUICK),
                                        P("I", SHORT), P("NG", SHORT), P("Z", SHORT), P("PA", MED)]
    s += q(108.0) + cap("PROFESSOR") + [P("P", QUICK), P("R1", SHORT), P("UH3", SHORT), P("F", SHORT),
                                        P("EH", MED), P("S", SHORT), P("ER", MED)]
    s += q(101.0) + cap("FALKEN.") + [P("F", SHORT), P("AW", MED), P("L", SHORT), P("K", QUICK),
                                      P("UH3", SHORT), P("N", MED), P("PA", LONG)]
    s += hold(7)
    # Declination across the run-up.
    s += q(106.0) + cap("SHALL") + [P("SCH", SHORT), P("AE", MED), P("LF", SHORT)]
    s += q(102.0) + cap("WE") + [P("W", SHORT), P("E", MED)]
    s += q(98.0) + cap("PLAY") + [P("P", QUICK), P("L1", SHORT), P("A", MED), P("AY", SHORT)]
    # Dip into the low turning point.
    s += q(92.0) + cap("A") + [P("UH3", QUICK)]
    # The rise: 88 Hz to 140 Hz across "game", one value per frame.
    lo_hz, hi_hz, steps = 88.0, 140.0, 9
    ladder = [lo_hz * (hi_hz / lo_hz) ** (n / steps) for n in range(steps + 1)]
    units = ["KV", "A", "A", "A", "A", "AY", "AY", "M", "M", "M"]
    s += cap("GAME?")
    for f, ph in zip(ladder, units):
        s += q(f)
        s += [P(ph, QUICK)]           # one frame each, so the pitch keeps moving
    s += setp(BASE_HZ) + [P("PA", LONG)]
    return s


HAL = (
    clear_caption()
    + setp(HAL_HZ, RATE_HAL)
    + cap("I'M")   + [P("AH1", LONG), P("I", SHORT), P("M", SHORT)]
    + cap("SORRY,") + [P("S", SHORT), P("AW", LONG), P("R1", SHORT), P("E", MED),
                       P("PA", MED)]                       # the comma
    #  "Dave" is the diphthong the demo already spells out in "afraid" and in
    #  the song's "Daisy": A carries it and AY closes it.
    + cap("DAVE.") + [P("D", QUICK), P("A", LONG), P("AY", SHORT), P("V", SHORT), P("PA", LONG)]
    + cap("I'M")   + [P("AH1", LONG), P("I", SHORT), P("M", SHORT)]
    + cap("AFRAID")+ [P("UH3", QUICK), P("F", SHORT), P("R1", SHORT), P("A", LONG), P("AY", SHORT), P("D", QUICK)]
    + cap("I")     + [P("AH1", MED), P("I", SHORT)]
    + cap("CAN'T") + [P("PA", QUICK), P("K", QUICK), P("AE", LONG), P("N", SHORT), P("T", QUICK)]
    + cap("DO")    + [P("D", QUICK), P("U", LONG)]
    + cap("THAT.") + [P("THV", QUICK), P("AE", LONG), P("PA", QUICK), P("T", QUICK), P("PA", LONG)]
)

# ------------------------------------------------------------------ song ---
BEAT = 10                                     # delay-units per 3/4 beat

# (syllable, note, beats, phonemes, caption) -- the caption is the whole WORD,
# emitted only at its first syllable so the screen reads as lyrics rather than
# as a syllable breakdown.
SONG = [
    ("Dai",  "G3", 3, [("D", 1), ("A", None), ("E", 6)], "DAISY"),
    ("sy",   "E3", 3, [("Z", 3), ("E", None)], ""),
    ("Dai",  "C3", 3, [("D", 1), ("A", None), ("E", 6)], "DAISY"),
    ("sy",   "G2", 3, [("Z", 3), ("E", None)], ""),
    ("give", "A2", 1, [("KV", 1), ("I", None), ("V", 2)], "GIVE"),
    ("me",   "B2", 1, [("M", 2), ("E", None)], "ME"),
    ("your", "C3", 1, [("Y", 2), ("O", None)], "YOUR"),
    ("an",   "A2", 2, [("AE", None), ("N", 3)], "ANSWER"),
    ("swer", "C3", 1, [("S", 3), ("ER", None)], ""),
    ("do",   "G2", 5, [("D", 1), ("U", None)], "DO"),
    ("",     "G2", 1, [("PA", None)], ""),
    ("I'm",  "D3", 3, [("AH1", None), ("I", 4), ("M", 4)], "I'M"),
    ("half", "G3", 3, [("HF", 2), ("AE1", None), ("F", 4)], "HALF"),
    ("cra",  "E3", 3, [("K", 1), ("R1", 2), ("A", None), ("E", 5)], "CRAZY"),
    ("zy",   "C3", 3, [("Z", 3), ("E", None)], ""),
    ("all",  "A2", 1, [("AW", None), ("LF", 3)], "ALL"),
    ("for",  "B2", 1, [("F", 2), ("O", None)], "FOR"),
    ("the",  "C3", 1, [("THV", 1), ("UH2", None)], "THE"),
    ("love", "D3", 2, [("L", 2), ("UH1", None), ("V", 3)], "LOVE"),
    ("of",   "E3", 1, [("UH2", None), ("V", 2)], "OF"),
    ("you",  "D3", 4, [("Y", 2), ("IU", None)], "YOU"),
]


def hz(name):
    step = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}[name[0]]
    return 440.0 * 2 ** ((12 * (int(name[1]) + 1) + step - 69) / 12.0)


def fmt(vals, per=8):
    return "\n".join("    .byte " + ",".join(f"${v:02X}" for v in vals[i:i + per])
                     for i in range(0, len(vals), per))


# ------------------------------------------------------------ Cylon ---
CYLON_HZ   = 72.0         # a Centurion: low, level, and not in a hurry
RATE_CYLON = 0x50

def cylon():
    """A Centurion line from the 1978 pilot, on one pitch from end to end.

    A Centurion has no prosody at all, and the earlier draft of this line had
    a great deal of it: six pitch escapes spanning 70 to 94 Hz, a range of
    five semitones, with a 3.7-semitone leap onto the last syllable of
    "command". Written as English stress, heard as a person. The whole line is
    one pitch escape now, and the only contrast left is duration -- vowels
    long, consonants short -- which segments the syllables without pitching
    them.
    """
    s = clear_caption()
    s += setp(CYLON_HZ, RATE_CYLON)
    s += cap("BY")        + [P("B", QUICK), P("AH1", LONG), P("I", SHORT)]
    s += cap("YOUR")      + [P("Y", SHORT), P("OO", MED), P("R1", SHORT)]
    s += cap("COMMAND,")  + [P("K", QUICK), P("UH3", SHORT), P("M", SHORT),
                             P("AE", LONG), P("AE", MED),
                             P("N", MED), P("D", QUICK), P("PA", MED)]
    s += cap("IMPERIOUS") + [P("I", SHORT), P("M", SHORT), P("P", QUICK),
                             P("E", LONG), P("R1", SHORT), P("E", SHORT),
                             P("UH2", SHORT), P("S", SHORT)]
    s += cap("LEADER.")   + [P("L1", SHORT), P("E", LONG), P("D", QUICK),
                             P("ER", MED), P("PA", LONG)]
    return s


#  Both pictures are 160 rows of the mixed-mode screen, and both address those
#  rows the same way: the hi-res interleave, which double hi-res inherits.
def row_base(y):
    return 0x2000 + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


# ------------------------------------------------ HAL's eye, hi-res ---
#  280 x 160 above four text rows. The palette has no red; orange is the
#  nearest thing, so the disc is orange, the bezel and pinpoint white. Every
#  byte carries the high bit so the whole picture sits in the orange/blue group
#  and no half-pixel shift appears at a boundary. Orange is the odd-x pixels,
#  so a solid orange byte is $AA in an even byte column and $D5 in an odd one;
#  white is every pixel on.
#
#  The flash is three concentric bands inside the disc. A vowel paints all
#  three white at once and they decay orange from the outside in, so each
#  syllable is a pulse that collapses toward the pinpoint. Bands are listed as
#  row spans of whole bytes so a paint is a tight run of stores.
HGR_W, HGR_H = 280, 160
HGR_BLACK, HGR_WHITE, HGR_ORANGE = 0, 1, 2
HGR_CX, HGR_CY, HGR_RX, HGR_RY = 139.5, 79.5, 70.0, 66.0   # pixels a shade wider than tall
HGR_BANDS = [(0.06, 0.18), (0.18, 0.30), (0.30, 0.42)]     # innermost first, radius fractions


def hgr_radius(x, y):
    return (((x - HGR_CX) / HGR_RX) ** 2 + ((y - HGR_CY) / HGR_RY) ** 2) ** 0.5


def hgr_eye():
    img = [[HGR_BLACK] * HGR_W for _ in range(HGR_H)]
    for y in range(HGR_H):
        for x in range(HGR_W):
            r = hgr_radius(x, y)
            if r <= 1.0:
                img[y][x] = HGR_WHITE if r > 0.925 else HGR_ORANGE    # ~5 px bezel
    # The pinpoint is whole byte cells, chosen by their centers, so no byte is
    # part white and part orange. A mixed byte keeps its alternating orange
    # bits when the bands around it go solid white, and read as a bracket.
    for y in range(HGR_H):
        for b in range(40):
            if hgr_radius(b * 7 + 3, y) <= 0.06:
                for i in range(7):
                    img[y][b * 7 + i] = HGR_WHITE
    return img


def hgr_pack_row(row):
    out = []
    for b in range(40):
        val = 0x80
        for i in range(7):
            c = row[b * 7 + i]
            x = b * 7 + i
            if c == HGR_WHITE or (c == HGR_ORANGE and (x & 1)):
                val |= 1 << i
        out.append(val)
    return out


def hgr_rle(stream):
    """(count, byte) pairs; a count with bit 7 set is an ALTERNATING run of
    the byte and its $7F complement. Solid orange is $AA, $D5, $AA, ... across
    byte columns, so without this the fill would pack to runs of one."""
    out = []
    i, n = 0, len(stream)
    while i < n:
        v = stream[i]
        if i + 1 < n and stream[i + 1] == (v ^ 0x7F):
            k = 1
            while i + k < n and k < 127 and stream[i + k] == (v if k % 2 == 0 else v ^ 0x7F):
                k += 1
            if k >= 3:
                out += [0x80 | k, v]; i += k
                continue
        k = 1
        while i + k < n and k < 127 and stream[i + k] == v:
            k += 1
        out += [k, v]; i += k
    return out + [0]


def hgr_bands():
    """Per band, the row spans of whole byte cells whose center lies in it."""
    img = hgr_eye()
    spans = [[] for _ in HGR_BANDS]
    for y in range(HGR_H):
        for k, (lo, hi) in enumerate(HGR_BANDS):
            run = None
            for b in range(40):
                r = hgr_radius(b * 7 + 3, y)
                solid = all(img[y][b * 7 + i] == HGR_ORANGE for i in range(7))
                inside = solid and lo <= r < hi
                if inside and run is None:
                    run = [b, 0]
                if inside:
                    run[1] += 1
                if (not inside) and run is not None:
                    spans[k].append((y, run[0], run[1])); run = None
            if run is not None:
                spans[k].append((y, run[0], run[1]))
    return spans


def emit_eye_hgr():
    img = hgr_eye()
    stream = [v for y in range(HGR_H) for v in hgr_pack_row(img[y])]
    packed = hgr_rle(stream)
    print(f"; HAL's eye, run-length coded: (count, byte) pairs over the 160 x 40 bytes")
    print(f"; of the mixed-mode picture, row-major; a count of 0 ends it. {len(packed)} bytes.")
    print("eyeRle")
    print(fmt(packed, 16))
    print()
    print("; Base address of each of the 160 graphics rows, low then high.")
    print("hgrRowLo")
    print(fmt([row_base(y) & 0xFF for y in range(HGR_H)], 16))
    print("hgrRowHi")
    print(fmt([row_base(y) >> 8 for y in range(HGR_H)], 16))
    print()
    sp = hgr_bands()
    print("; The flash bands, innermost first. Each span is rowLo, rowHi, first byte")
    print("; column, byte count; a rowHi of 0 ends the band.")
    for k, s in enumerate(sp):
        print(f"; band {k}: {len(s)} spans, {sum(n for _, _, n in s)} bytes")
        print(f"glowBand{k}")
        data = []
        for y, b, n in s:
            data += [row_base(y) & 0xFF, row_base(y) >> 8, b, n]
        print(fmt(data + [0, 0, 0, 0], 16))
    print()
    #  The engine charges the delay after a paint for the time the paint took,
    #  and does it the same way in both demos. Here there is nothing to charge:
    #  a band is a fill rather than a copy, and BurnUnits already runs its inner
    #  loop short by the eye's share. Saying so with a zero costs one subtract
    #  and keeps the shared code free of a conditional.
    print("; What a paint costs the delay that follows, in speech units and in")
    print("; song laps left after it. Nothing, here: a band is a fill, and the")
    print("; delay loop is already short by the eye's share.")
    print("FLASHU    = $00")
    print(f"SNGFLASHL = ${SONG_LAPS:02X}")
    print("decayUnits")
    print(fmt([0, 0, 0]))
    print("decayLaps")
    print(fmt([SONG_LAPS] * 3))
    print()


# ----------------------------------------- HAL's eye, double hi-res ---
#  140 color cells by 160 rows above four rows of 80-column text. A cell is
#  four dots and a byte is seven, so a cell's color comes from the dots of one
#  or two bytes and a byte's value depends on the cells it covers. The colors
#  are the lo-res sixteen. Which dot carries which bit is fixed by Apple IIe
#  Technical Note #3, whose four-byte fill patterns (aux, main, aux, main) are
#  the definition: magenta is $08 $11 $22 $44, only the LAST dot of each cell
#  lit, so the last dot is the color's low bit and the first three are bits 1
#  to 3.
#
#  The eye is a gray bezel around a lens that shades magenta, orange, yellow
#  to a white pinpoint. The flash is the three inner rings each stepping one
#  shade brighter, and the decay steps them back from the outside in. A ring
#  boundary rarely lands on a byte, so a paint cannot be a fill; it is a copy.
#  The picture is rendered in each of its four states and, per transition, the
#  bytes that change are emitted with their new values, as spans: rowLo,
#  rowHi, column with bit 7 set for the auxiliary bank, count, then the
#  bytes. A rowHi of 0 ends the list.
DH_W, DH_H = 140, 160
DH_BLACK, DH_MAGENTA, DH_ORANGE, DH_GRAY2, DH_YELLOW, DH_WHITE = 0, 1, 9, 10, 13, 15
DH_CX, DH_CY, DH_RX, DH_RY = 69.5, 79.5, 35.0, 66.0   # cells twice as wide as a row is tall
DH_ZONES = [(0.07, DH_WHITE), (0.18, DH_YELLOW), (0.42, DH_ORANGE),
            (0.925, DH_MAGENTA), (1.0, DH_GRAY2)]
#  The flash bands, innermost first: (inner radius, outer radius, lit color).
#  Each lies within one zone and lights to the next shade in.
DH_BANDS = [(0.07, 0.18, DH_WHITE), (0.18, 0.30, DH_YELLOW), (0.30, 0.42, DH_YELLOW)]
#  Cycle costs of PaintStream, for the constants that give the time back.
CYC_PER_BYTE, CYC_PER_SPAN = 19, 70
SPEECH_UNIT, SONG_LAP, SONG_LAPS = 4096, 1280, 33


def dh_radius(x, y):
    return (((x - DH_CX) / DH_RX) ** 2 + ((y - DH_CY) / DH_RY) ** 2) ** 0.5


def dh_cells(lit):
    """The picture, with the bands in `lit` one shade brighter."""
    img = [[DH_BLACK] * DH_W for _ in range(DH_H)]
    for y in range(DH_H):
        for x in range(DH_W):
            r = dh_radius(x, y)
            c = DH_BLACK
            for edge, color in DH_ZONES:
                if r <= edge:
                    c = color
                    break
            for k, (lo, hi, bright) in enumerate(DH_BANDS):
                if k in lit and lo < r <= hi:
                    c = bright
            img[y][x] = c
    return img


def dh_pack_row(cells):
    """The row's 80 bytes in screen order: aux, main, aux, ... Dot j of a cell
    carries bit (j + 1) & 3 of its color."""
    dots = [0] * (DH_W * 4)
    for k, c in enumerate(cells):
        for j in range(4):
            if (c >> ((j + 1) & 3)) & 1:
                dots[4 * k + j] = 1
    out = []
    for s in range(80):
        v = 0
        for i in range(7):
            v |= dots[7 * s + i] << i
        out.append(v)
    return out


#  The packer against the technote's own table.
assert dh_pack_row([DH_MAGENTA] * DH_W)[:4] == [0x08, 0x11, 0x22, 0x44]
assert dh_pack_row([DH_ORANGE] * DH_W)[:4] == [0x4C, 0x19, 0x33, 0x66]
assert dh_pack_row([DH_YELLOW] * DH_W)[:4] == [0x6E, 0x5D, 0x3B, 0x77]
assert dh_pack_row([DH_GRAY2] * DH_W)[:4] == [0x55, 0x2A, 0x55, 0x2A]


def dh_rle(stream):
    """(count, byte) pairs; a count with bit 7 set is followed by TWO bytes and
    the run alternates them. A solid color's pattern repeats every four bytes
    across the screen, so within one bank it alternates two values, and
    without the pair a fill would pack to runs of one."""
    out, i, n = [], 0, len(stream)
    while i < n:
        a = stream[i]
        b = stream[i + 1] if i + 1 < n else None
        if b is not None and b != a:
            k = 2
            while i + k < n and k < 127 and stream[i + k] == (a if k % 2 == 0 else b):
                k += 1
            if k >= 4:
                out += [0x80 | k, a, b]
                i += k
                continue
        k = 1
        while i + k < n and k < 127 and stream[i + k] == a:
            k += 1
        out += [k, a]
        i += k
    return out + [0]


def dh_transition(src, dst):
    """Spans of the bytes that differ between two pictures, per row and bank,
    carrying the new values: (row, aux, first column, bytes). A gap of up to
    three bytes is bridged, since a span header costs more than three bytes
    do; the bridged bytes are written with the value they already hold."""
    spans = []
    for y in range(DH_H):
        a, b = dh_pack_row(src[y]), dh_pack_row(dst[y])
        for aux, off in ((1, 0), (0, 1)):
            pa, pb = a[off::2], b[off::2]
            runs = []
            for c in range(40):
                if pa[c] == pb[c]:
                    continue
                if runs and c - runs[-1][1] <= 4:
                    runs[-1][1] = c
                else:
                    runs.append([c, c])
            for c0, c1 in runs:
                spans.append((y, aux, c0, pb[c0:c1 + 1]))
    return spans


def dh_span_bytes(spans):
    data = []
    for y, aux, c0, vals in spans:
        base = row_base(y)
        data += [base & 0xFF, base >> 8, c0 | (0x80 if aux else 0), len(vals)] + list(vals)
    return data + [0, 0]


def emit_eye_dhgr():
    rest = dh_cells(set())
    s3, s2, s1 = dh_cells({0, 1, 2}), dh_cells({0, 1}), dh_cells({0})
    for name, off in (("eyeRleAux", 0), ("eyeRleMain", 1)):
        stream = [v for y in range(DH_H) for v in dh_pack_row(rest[y])[off::2]]
        packed = dh_rle(stream)
        print(f"; HAL's eye, {'auxiliary' if off == 0 else 'main'} bank, run-length coded over the")
        print(f"; 160 x 40 bytes of the mixed-mode picture, row-major; a count of 0 ends it.")
        print(f"; {len(packed)} bytes.")
        print(name)
        print(fmt(packed, 16))
        print()
    print("; Base address of each of the 160 graphics rows, low then high.")
    print("hgrRowLo")
    print(fmt([row_base(y) & 0xFF for y in range(DH_H)], 16))
    print("hgrRowHi")
    print(fmt([row_base(y) >> 8 for y in range(DH_H)], 16))
    print()
    print("; The flash and its three decay steps, outermost ring first. Each is the")
    print("; bytes that change, as spans: rowLo, rowHi, column (bit 7 = aux), count,")
    print("; then the count bytes; a rowHi of 0 ends the list.")
    costs = {}
    for name, a, b in (("eyeFlash", rest, s3), ("eyeDecay2", s3, s2),
                       ("eyeDecay1", s2, s1), ("eyeDecay0", s1, rest)):
        sp = dh_transition(a, b)
        nbytes = sum(len(v) for *_, v in sp)
        costs[name] = CYC_PER_BYTE * nbytes + CYC_PER_SPAN * len(sp)
        print(f"; {name}: {len(sp)} spans, {nbytes} bytes, about {costs[name]} cycles")
        print(name)
        print(fmt(dh_span_bytes(sp), 16))
        print()
    flash = costs["eyeFlash"]
    decays = [costs[n] for n in ("eyeDecay0", "eyeDecay1", "eyeDecay2")]
    print("; What the paints cost, so the delays around them can give the time back:")
    print("; speech units of 4096 cycles, and laps of the song's 1280-cycle delay loop")
    print("; left in the unit after the paint. The decay tables are indexed by ring.")
    print(f"FLASHU    = ${max(1, round(flash / SPEECH_UNIT)):02X}")
    print(f"SNGFLASHL = ${max(1, SONG_LAPS - round(flash / SONG_LAP)):02X}")
    print("decayUnits")
    print(fmt([max(1, round(c / SPEECH_UNIT)) for c in decays]))
    print("decayLaps")
    print(fmt([max(1, SONG_LAPS - round(c / SONG_LAP)) for c in decays]))
    print()


#  A silence to close a sentence on, in delay units of about 41.5 ms each.
#  A phoneme-length PA cannot do this job: the longest one at the speech rate
#  is 128 ms, which reads as a breath rather than a full stop, so each line
#  ran straight into the next and the interjection between them got lost.
def hold(units):
    return [WAIT, units]


def emit(name, data, comment, tail = 0):
    print(f"; {comment}")
    print(name)
    print(fmt(data + (hold (tail) if tail else []) + [END]))
    print()


def emit_all(path, emit_eye, screen):
    """One version's whole .inc: the shared speech and song, and its own eye."""
    saved = sys.stdout
    #  Default newline translation gives this file the CRLF the tree uses.
    with open(path, "w", encoding = "ascii") as out:
        sys.stdout = out
        try:
            print("; ==== GENERATED by gen-speech-demo-data.py -- do not hand-edit ====")
            print(f"; {screen}")
            print(f"; speech rate ${RATE_SPEECH:02X}, baseline pitch {BASE_HZ:.0f} Hz")
            print()
            emit("wargData", wargames(),
                 '"Shall we play a game?" -- declination, dip, then a drawn rise', tail = 7)
            emit("halData", HAL,
                 '"I\'m sorry Mark. I\'m afraid I can\'t do that." -- slow and level', tail = 7)
            emit("cylonData", cylon(),
                 '"By your command, Imperious Leader." -- a Centurion, low and level',
                 tail = 8)
            emit_eye()

            print("; Daisy Bell chorus. Caption escapes carry the syllable; records are")
            print("; {phoneme, pitch-hi, pitch-lo, units}, $FF ends. Rate nibble 0 is")
            print("; deliberate here: the song paces itself and wants the longest frames.")
            print("songData")
            print(fmt(clear_caption()))
            total = 0
            for syl, note, beats, parts, word in SONG:
                hi, lo = pitch_bytes(hz(note), 0x00)
                units = beats * BEAT
                total += units
                if word:
                    print(fmt(cap(word)))
                fixed = sum(u for _, u in parts if u is not None)
                rows = [(PH[p], hi, lo, (units - fixed) if u is None else u) for p, u in parts]
                body = ",".join(",".join(f"${v:02X}" for v in r) for r in rows)
                print(f"    .byte {body}   ; {syl or 'rest':5s} {note} {beats}b")
            print("    .byte $FF")
            print()
            print(f"; total {total} units ~= {total * 0.0415:.1f} s")
        finally:
            sys.stdout = saved


emit_all(os.path.join (_HERE, "mockingboard-speech-demo-hgr.inc"),
         emit_eye_hgr, "hi-res eye over 40-column captions")
emit_all(os.path.join (_HERE, "mockingboard-speech-demo-dhgr.inc"),
         emit_eye_dhgr, "double hi-res eye over 80-column captions")
print("wrote mockingboard-speech-demo-hgr.inc and mockingboard-speech-demo-dhgr.inc")
