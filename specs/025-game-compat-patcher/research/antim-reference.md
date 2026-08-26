# anti-m — reference playbook (MIT)

4am & qkumba's **anti-m** (`github.com/a2-4am/anti-m`, MIT-licensed, pinned at
commit `7a36756f`, 2024-12-09) defeats exactly this protection family. It is a
reference, NOT a dependency: its source is 6502 assembly for a guest-side boot
tool and is never vendored, compiled, linked, or shipped by Casso. Under the
project constitution's dependency rule (which governs *shipped* material) it
needs no allowlist entry; under the clean-room emulation doctrine, reading it is
allowed and rules derived from it must cite it.

## How it works (three layers)

1. **`src/compare.a`** — a byte matcher with a wildcard byte `$97` ("like the
   '.' character in a regular expression"). Used to IDENTIFY which known
   bootloader family is present, not to find the check itself. This is the
   design precedent for a **wildcard/don't-care byte** in Casso's rule patterns.
2. **Staged `JMP` hooks** — anti-m runs *as a guest*, so it gets control exactly
   once (before the game loads) and must re-hook at each load/decrypt stage to
   survive. For Choplifter it hooks four callbacks deep ("regain control after
   it loads 1 sector into `$0300`" -> "after it decrypts itself into `$0100`" ->
   "after it loads 4 sectors into `$0400`" -> ...).
   **Casso's GamePatcher does NOT need any of this**: it scans all of RAM every
   frame from *outside* the machine, so it only needs a post-relocation
   signature + idempotent writes. This is the single biggest simplification the
   emulator seat buys us over anti-m.
3. **Per-family defuse.** For Choplifter, anti-m stores a single `$60` (RTS):
   ```
   @choplifterCallback3
   ; patch bootloader to bypass ROM check at $6300
            lda   #$60
            sta   $0421
            jmp   $0400
   ```
   Its own comment names "ROM check at `$6300`" — the exact region Casso traced.
   (`$0421` is a pre-relocation address in anti-m's staged copy; by the time
   Casso's per-frame scan sees the code it lives at `$62xx/$63xx`.)

## The hazard to design around

anti-m's Choplifter path contains:
```
; restore bytes on stack page that we patched earlier
; because they're checksummed soon
```
i.e. **the protection checksums its own loader**. A patch applied too early can
be caught by that self-checksum or overwritten by a later load stage. anti-m
un-patches its hooks before the checksum runs and re-hooks after.

For Casso, GamePatcher's per-frame *idempotent* rescan should naturally land on
the right side of this (it patches the final running code and re-applies if the
guest restores a region), but this MUST be validated: confirm the verifier
region Casso patches is not itself inside a region that some later stage
checksums after the patch lands. If it is, the rule may need to target a later/
different site (e.g. anti-m's caller-RTS approach on the post-relocation call
site) or apply only once execution has passed the self-check.

## Catalog (scope for User Story 4)

`src/idbroderbund.a`'s tested-titles list (~25) — the follow-on coverage this
one mechanism can reach:

Broderbund/Sensible/Sirius/etc.: Bug Attack, **Choplifter**, David's Midnight
Magic, Dueling Digits, Genetic Drift, Labyrinth, Quadrant 6112, Red Alert,
Seafox, Serpentine, Sky Blazer, Space Eggs, **Space Quarks** (GH #99), Star
Blazer, Disk Recovery, ABM, Palace in Thunderland.

Gebelli: Eggs-It, High Orbit, Horizon V, Lazer Silk, Neptune, Phaser Fire,
Russki Duck, Zenith.

(Karateka's //c hang is a *different* mechanism — a VBL-poll incompatibility,
not this ROM check — but is the same class of "runs on the machine it was
written for, fails by design on a later one" and shares the GamePatcher vehicle.
See project memory `project-karateka-2c-boot-fail`.)

## Re-reading the source

```
gh api repos/a2-4am/anti-m/contents/src/<file>.a --jq .content   # then base64-decode
```
Key files: `compare.a` (matcher + `$97` wildcard), `idbroderbund.a` (the
Broderbund/Gebelli tracers incl. the Choplifter patch), `anti-m.a` (driver).
Usage as a guest tool: boot `anti-m.dsk`, insert the game at the prompt, press
Return.
