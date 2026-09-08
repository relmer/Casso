# SSI-263 / SC-02 primary source

`Votrax_SC-02_SSI-263A_Data_Sheet_1985.pdf` -- the Votrax SC-02 / SSI-263A
Phoneme Speech Synthesizer data sheet, 8 pages, retrieved 2026-09-08 from
<https://www.bitsavers.org/pdf/federalScrewWorks/Votrax_SC-02_SSI-263A_Phoneme_Speech_Synthesizer_Data_Sheet_1985.pdf>.

It is a page-image scan with no text layer, so grep will not find anything in
it. Render a page to read it:

```
python -c "import pymupdf;pymupdf.open('Votrax_SC-02_SSI-263A_Data_Sheet_1985.pdf')[2].get_pixmap(dpi=150).save('p3.png')"
```

Page index for the parts this feature keeps returning to:

| Page | Content |
|---|---|
| 2 | Operation description; attribute register classes; Selectable Operation Modes |
| 3 | Duration and frame formulas; **Control Bit and Power Down Mode**; register reading; phoneme chart (all 64 codes) |
| 4 | Pin assignments (A/R is pin 4); **Register Input Formats** |
| 5 | Write/read timing; **Mode Selection Chart**; absolute maximum ratings |

`research.md` was written against a copy of this document that was not kept.
That is why it can quote the A/R row and the duration formula but cannot answer
questions about the CTL transition -- this file closes that gap.

## The CTL one-to-zero transition

Page 3, "Control Bit and Power Down Mode":

> Upon a Control bit logic one-to-zero transition, the present settings of DR1
> and DR0 determine the operation mode as described in the Mode Selection Chart.

Page 5, Mode Selection Chart:

| DR1 | DR0 | CTL | Function |
|---|---|---|---|
| HI | HI | HI->LO | A/R active; phoneme timing response; transitioned inflection (most commonly used mode) |
| HI | LO | HI->LO | A/R active; phoneme timing response; immediate inflection |
| LO | HI | HI->LO | A/R active; frame timing response; immediate inflection |
| LO | LO | HI->LO | Disables A/R output only; does not change previous A/R response |

Page 4, pin 4: A/R is an open-collector output that "changes from high to low
level after phoneme is generated."

**Reading**: in the top three rows the transition leaves Power Down with A/R
*active*, so the chip generates the phoneme already sitting in P5-P0 and then
pulls A/R low. Only the DR1=DR0=LO row leaves the request line alone. The
datasheet describes no other event that could produce a *first* A/R -- page 2
has software load the attribute registers after power up and then take CTL low
-- so if that transition did not begin generating, the documented
interrupt-driven mode could never start.

## The defect this closed

`Ssi263::LatchMode` used to clear `m_sounding` and `m_request` on every CTL
1->0, whatever the mode, while `Ssi263::WriteRegister` starts a phoneme on a
duration/phoneme write only when the chip is NOT powered down. Software that
loaded DR/P first and then dropped CTL -- the datasheet's own order -- got no
first A/R at all. `LatchMode` now begins the loaded phoneme in the three
A/R-active modes and keeps the old behavior for DR1=DR0=LO.

Two tests cover it, both of which fail against the old code:
`CtlTransitionSoundsThePhonemeLoadedWhilePoweredDown` and
`DetectionSequenceGetsItsInterruptAtZeroAmplitude`.

**The Mist demake demonstrates it.** deater's `ssi263_detect.s` uses exactly
this order, and a CPU trace of Mist shows the whole handshake working on the
fixed build: `$C48C=$0C`, the attribute registers, `$C443=$80` (CTL high),
`$C440=$C0` (DR/P loaded while powered down), `$C443=$70` (CTL low), `$C48E=$82`
(arm CA1) -- then 124 iterations of read IFR2 / clear CA1 / write the next
phoneme. That loop only runs when detection reported the chip, and the first
A/R it waits on can only come from the CTL transition. Revert `LatchMode` and
Mist derails where speech should start.

Our own speech disks and every `Ssi263Tests` case write DR/P *after* CTL is
already low -- they all route through the `StartSpeaking` helper -- which is
why the whole suite passed over this for so long.
