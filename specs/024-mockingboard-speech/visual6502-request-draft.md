# Draft request to visual6502

**To**: `visual6502@gmail.com` — address published at
<http://www.visual6502.org/donate_hw.html>

**Status**: DRAFT — not sent. Review and send from your own account.

**Context**: T057. Asks for the full-resolution SSI-263P die shot so the phoneme
parameter ROM can be extracted (research.md D11c).

---

**Subject:** SSI-263P die shot — full-resolution master available?

Hi,

Thank you for the SSI-263P die shots — the ones on this page:

    http://www.visual6502.org/images/pages/Silicon_Systems_SSI_263P_die_shots.html

I'm working on emulating that chip for an Apple II emulator, and your images
have already gotten me further than I expected.

Some background on why I'm asking. The SSI-263A (the Votrax SC-02) is the speech
synthesizer on the Mockingboard C, and as far as I can tell **its phoneme
parameter ROM has never been extracted**. MAME and AppleWin both substitute the
SC-01A's data — `sc01a.bin`, decapped back in 2007 — but the two parts have 64
phonemes in different orders that don't map 1:1, so nobody currently has accurate
SSI-263 speech. The chip's datasheet documents its registers, its phoneme codes,
and its timing formulas, but publishes no formant values at all.

Working from your published 7000 × 5803 image
(http://www.visual6502.org/images/263P/SSI_263P_20x_1a_7000w.jpg), I've
located what I'm fairly
confident is that parameter ROM, at approximately x 2350–3645, y 3100–3745. By
autocorrelation it has a column pitch of 19.0 px and a row pitch of 13.3 px,
giving a **68 × 48 array**. That geometry is a good match for what the Votrax
patents describe: 64 phonemes plus spare columns, and twelve control parameters
at 4 bits each — 48 rows. The metal is intact, but the cells look
contact-programmed and the programming features are visible through it, so this
doesn't appear to be an implant-programmed ROM.

The problem is resolution. At 19 × 13.3 px per cell the features are only a few
pixels across — enough to see that they're there, not enough to read reliably.

**So my question: is the full 17,265 × 14,313 stitched master available?** The
die shot page mentions it, but the largest download is the 7000-wide version.
At full resolution the cell pitch would be roughly 47 × 33 px, which I think
would make extraction straightforward.

Two smaller questions, if it's easy to answer:

1. Was this die delayered, or is it an as-is shot? I've assumed as-is, since I
   don't see per-layer variants on the page.
2. The page mentions a donor sent you two SSI-263P chips. If the second is still
   around and a delayered shot would be more useful than the master, I'd be glad
   to know — I'm not asking you to do the work, just trying to understand what
   exists.

If I do get the ROM out of it, I'll publish the extracted data and the method
openly so it's available to MAME, AppleWin, and anyone else who wants it. That
seems like the right outcome for images you made public in the first place, and
it would close a gap that's been open in speech emulation for a long time.

Happy to donate toward bandwidth or hardware either way — the die shots have
already been worth it.

Thanks for the work you do,

[your name]
[optional: link to the project]

---

## Notes before sending

- The technical claims above are **my measurements from your published image**,
  stated as such. If any turn out wrong, the ask still stands on its own.
- Consider mentioning the project by name and linking the repo — they preserve
  hardware for public benefit, and a concrete open-source use is the strongest
  argument for spending their time.
- If nothing comes back in a couple of weeks, the fallback is D11 route B: a
  community recording of all 64 phonemes from real hardware. That path needs no
  cooperation from anyone holding unique assets.
