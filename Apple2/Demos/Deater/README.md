# Deater SSI-263 speech disks

Vince Weaver's (deater) free, open-source Apple II software, downloaded
2026-09-08. These are the T060 Tier-1 acceptance titles from
`specs/024-mockingboard-speech/validation-sets.md`: unlike the period talking
games, they drive the **SSI-263** directly, so they are the ones expected to
speak on Casso's Mockingboard C.

Source for all three: <http://www.deater.net/weave/vmwprod/> (plain HTTP -- the
site's HTTPS certificate covers securedata.net, not deater.net). Build sources
live in <https://github.com/deater/dos33fsprogs>.

| Disk | Title | Version | Speech |
|---|---|---|---|
| `mist.dsk`, `mist_side2.dsk`, `mist_side3.dsk` | Mist demake | v1.03, 2021-09-22 | intro voiceover |
| `wargames.dsk` | WarGames demo | v0.1, 2021-09-13 | the "Joshua" computer voice |
| `peasant_disk1.dsk` .. `peasant_disk5.dsk` | Peasant's Quest demake | v0.98.2, 2026-08-30 | Trogdor's speech |

All nine are 140 KB DOS-order images.

## What these actually do, as of 2026-09-08

**Mist speaks the phonemes but at volume zero, and the fault is upstream.**
Everything on our side works: the card is detected, the CTL transition delivers
the first A/R, the CA1 interrupt paces the loop, and 124 phonemes are
synthesized. But Mist's init sets the amplitude field to `$C` and its detection
routine then writes `$C443 = $70` -- CTL low, **amplitude 0** -- and the phoneme
loop only ever touches `$C440`, so the amplitude is never restored. A chip that
honors A3-A0 renders silence. Floor the amplitude at `$C` in a scratch build and
the voiceover comes out: 159 Hz voiced, formants moving, 77-86% of the energy in
the 300-3000 Hz band, at exactly the timestamps the trace shows phoneme writes.
AppleWin behaves the same way -- music, no voice -- so the demo is silent
wherever the amplitude register is honored.

The clobbering write lives in the shared `ssi263_detect.s`, but whether it
matters depends on the speech player layered above it, and they differ.
WarGames' `ssi263_simple_speech.s` re-writes `CAA = $7F` -- amplitude $F -- from
its interrupt handler on **every** phoneme, so it would recover the volume the
detect routine dropped. Mist's player never touches `$C443` in the pacing loop
(three writes to it in the whole traced run: `$5C`, `$80`, `$70`), which is why
only Mist goes silent.

**WarGames fails earlier, and differently.** It never executes its detection at
all. Two CPU traces -- one complete from power-on to 22s, one covering the
closing monologue -- show not a single access to the `$C4` page and not one IRQ.
It prints "GREETINGS PROFESSOR FALKEN" and the rest as plain text, then sits in
a keyboard-poll loop at `$0D73`. The detect routine's bytes *are* on the disk
(offset `0x1B480`), so this is a routine that got assembled in with its call
site missing -- a published image built from a state the speech was not wired
up in. AppleWin is silent on it too. The amplitude bug above never gets a
chance to bite here.

**Peasant's Quest boots, plays music, and detects the chip.** The floppy set
works despite the upstream warning. A complete power-on-to-45s trace shows the
detect sequence and then, 13,629 instructions later, the interrupt handler
answering it:

```
$C48C=$0C  $C443=$80  $C440=$C0  $C443=$70  $C48E=$82   detect_ssi263
$C48D=$02  $C443=$80  $C440=$00  $C443=$70  $C48E=$02   mb_irq
```

That gap is ~41,000 cycles against the ~37,400 the datasheet formulas predict
for a duration-3 phoneme at the default rate, so the A/R arrived when it should
have. **This is the strongest confirmation of the CTL fix on real software**:
`mb_irq` runs only if the request fired, and the request can only come from the
CTL one-to-zero transition sounding the phoneme loaded while powered down.

No speech followed in 45 s, and a second trace covering 54-120 s shows none
either -- ten slot-4 writes in total, all of them the detect handshake. The
speech is presumably tied to game events (the Trogdor scenes), which need typed
input to reach.

## Caveats

**The Peasant's Quest floppies are untested upstream.** The download page says
so and steers users to `peasant.2mg.zip`, a 1.6 MB ProDOS hard-disk image.
Casso has no SmartPort or hard-disk support and rejects `2mg` containers, so
the floppy set is the only option here; if a side fails to boot, check the
upstream image before suspecting Casso.

**WarGames has never run on real hardware.** The author does not own an
SSI-263, so the disk is verified only against AppleWin. It will not speak under
MAME, which emulates the SC-01 instead.

## Why these and not Crypt of Medea

The famous talking Apple II games -- Crypt of Medea, Berzap!, Crimewave, The
Spy Strikes Back -- feed **SC-01** phonemes to a first-generation board. On an
SSI-263 they need the community's phoneme conversion, so a faithful Mockingboard
C is not expected to speak them. A failure there is not a Casso bug.
