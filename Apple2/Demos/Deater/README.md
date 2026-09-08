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
