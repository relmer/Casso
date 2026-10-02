# Draft request to visual6502

**To**: `visual6502@gmail.com`—address published at
<http://www.visual6502.org/donate_hw.html>

**Status**: DRAFT—not sent. Review and send from your own account.

**Context**: T057. Originally asked for the full-resolution SSI-263P die shot so
the phoneme parameter ROM could be extracted at all (research.md D11c). The
extraction has since been done from the published 7000-wide image
(`rom-extraction/`), so the request is now for the master to confirm that read
independently and to finish the one part the published image could not settle:
the filter code-to-Hz mapping.

---

**Subject:** SSI-263P phoneme ROM read from your die shot—is the full-resolution master available?

Hi,

Thank you for the SSI-263P die shots—the ones on this page:

    http://www.visual6502.org/images/pages/Silicon_Systems_SSI_263P_die_shots.html

I've been emulating that chip for Casso, an open-source Apple II emulator, and
your images turned out to be the key to it.

Some background. The SSI-263A (the Votrax SC-02) is the speech synthesizer on the
Mockingboard C. As far as I can tell, **its phoneme parameter ROM had never been
extracted**, so nobody had accurate SSI-263 speech. AppleWin plays back recorded
phoneme samples at a fixed rate, with no inflection or filter response. MAME's
Apple II Mockingboard carries the earlier SC-01A instead, and its only SSI-263
routes phonemes through the SC-01A emulation, which its own source calls
"completely wrong." The datasheet documents the registers, phoneme codes and
timing formulas, but no formant values.

**Working from your published 7000 × 5803 image, I've read that ROM.** It sits
at approximately x 2350–3645, y 3100–3745:

- 64 columns, one per phoneme (mirrored: image column *c* holds phoneme 63 − *c*),
  confirmed by the column address decoder below the array.
- 29 data rows, programmed by stadium-shaped contact ovals visible through the
  metal; 749 of 1,856 cells are set.
- Cells were classified against hand-labeled templates, every disagreement was
  re-checked at 10× zoom, and one column was audited by hand end to end. One
  cell stayed low-confidence and was settled by eye.

The bits also decode. The voiced flag matches the voiced/voiceless chart in the
SSI 263A programming guide for every phoneme the guide classifies, and the
remaining 24 bits are six 4-bit fields stored significance-interleaved, the same
layout as the SC-01A's ROM:
F1, F2 and F3 filter codes, voice amplitude, fricative amplitude, and nasal
coupling. The formant codes track the published vowel formants closely
(rank correlation 0.89 / 0.94 / 0.85 for F1 / F2 / F3), the nasal field is set
for exactly M, N, NG and HN, and of the 46 phonemes the two chips have in common
(matched by mnemonic), 22 carry formant codes identical to the SC-01A's. Casso now synthesizes speech from this
table.

The data, the method and an annotated plate of every bit are public here:

    https://github.com/relmer/Casso/tree/master/specs/024-mockingboard-speech/rom-extraction

**So my question: is the full 17,265 × 14,313 stitched master available?** The
die shot page mentions it, but the largest download is the 7000-wide version.
It would help in two ways:

1. **Independent confirmation of the bit read.** At about 2.5× the linear
   resolution, each contact oval becomes large enough to classify without
   judgment calls, which would let me (or anyone) verify the table from scratch
   rather than trust one extraction.
2. **The code-to-Hz mapping.** Right now I convert filter codes to frequencies
   with the SC-01A's measured capacitor values, on the evidence that the two
   chips share a code scale. That is an inference. At full resolution I could
   trace the ROM outputs to the switched-capacitor filter banks and read this
   chip's own capacitor ratios.

Two smaller questions, if they're easy to answer:

1. Was this die delayered, or is it an as-is shot? I've assumed as-is, since I
   don't see per-layer variants on the page.
2. The page mentions a donor sent you two SSI-263P chips. If the second is still
   around and a delayered shot would be more useful than the master, I'd be glad
   to know—I'm not asking you to do the work, just trying to understand what
   exists.

The extracted data is free for MAME, AppleWin, or anyone else to use, and
anything the master adds will be published the same way. That seems like the
right outcome for images you made public in the first place.

Happy to donate toward bandwidth or hardware either way—the die shots have
already been worth it.

Thanks for the work you do,

[your name]

---

## Notes before sending

- The technical claims above come from `rom-extraction/README.md` and
  `decoded-data.md`. If any turn out wrong, the ask still stands on its own.
- The link points at `master`. Confirm `rom-extraction/` is on master and
  public before sending.
- If nothing comes back in a couple of weeks, the fallback for the code-to-Hz
  mapping is D11 route B: a community recording of all 64 phonemes from real
  hardware, fitted against the decoded filter codes. That path needs no
  cooperation from anyone holding unique assets.
