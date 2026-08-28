# SSI-263P phoneme parameter ROM — the decoded data

**Feature**: `024-mockingboard-speech` | companion to `README.md` in this
directory, which covers provenance, extraction method, and validation.

This file presents WHAT the ROM says. The annotated plate
(`rom-decoded-plate.png`) shows every bit on the die photograph, boxed by
value and labeled with its decoded meaning.

## The per-phoneme record: 29 bits

Each of the 64 columns holds one phoneme (codes $3F down to $00, left to
right — the on-die address decoder below the array proves the mirrored
order). Reading down a column, top to bottom:

| Rows | Contents |
|---|---|
| b00 | Closure flag — clear for phonemes that begin with a sealed tract (B, D, HVC, HFC) |
| b01, b02 | Manner-class bits |
| b03 | Fricative flag — oval means NOT fricative (sole exception: PA) |
| b04 | Voiced flag — oval means NOT voiced (sole exception: PA) |
| b05–b10 | Bit 3 (MSB) of FA, VA, F3, NAS, F2, F1 — in that row order |
| b11–b16 | Bit 2 of the same six parameters |
| b17–b22 | Bit 1 |
| b23–b27 + lone row | Bit 0 (LSB) |

The six 4-bit parameters are **significance-interleaved**: the ROM stores
one bit-plane of all six parameters per row group, not one parameter per
row group — the same organization the SC-01 decap revealed for its
predecessor. The unpaired 29th row at the array's bottom edge is simply
F1's bit 0, which is why phoneme pairs with adjacent F1 codes (E/AY,
AI/:A) differ only there.

| Param | Meaning |
|---|---|
| FA | Fricative (noise) amplitude — 0 for every vowel and sonorant |
| VA | Vocal (voiced) amplitude — 0 for every unvoiced phoneme |
| F3 | Third-formant filter code (R has the chip's lowest — the retroflex signature) |
| NAS | Nasal coupling — 3 for exactly M, N, NG, HN; 0 elsewhere (its two high planes are the array's all-zero rows) |
| F2 | Second-formant filter code — monotone front-to-back across the vowel space |
| F1 | First-formant filter code — traces the vowel triangle |

## Structural cross-checks the data passes

- **b03/b04 match the datasheet's fricative/voiced classification 63/64**,
  the only exception being PA (silence — both flags moot).
- **E ($01) ≡ AY ($05)** and **AI ($09) ≡ :A ($3A)** bit-for-bit except
  F1 bit 0 — the phonetics literature lists each pair with identical
  formants.
- **HV/HVC and HF/HFC** differ only in the closure-region bits.
- **PA decodes to zero amplitudes with a mid-tract filter position** —
  silence with a parked vocal tract, the hardware behavior behind
  "pauses hold the tract."
- The NAS field's exact partition (M, N, NG, HN vs everything else) was
  discovered from the data, not assumed.

## Cross-check against the SC-01A decap — and what it settled

The one dataset other emulators use is the 2007 decap of the predecessor
chip (`sc01a.bin`, published by the decapper at og.kervella.org/sc01a/,
CRC `fc416227`; MAME documents its layout). Aligning the two chips'
phoneme sets by mnemonic gives 46 matched pairs, and the comparison is
decisive:

- **22 of 46 phonemes carry identical F1/F2/F3 codes, digit for digit**
  (A, AE, AH, AW, AY, B, D, E, EH, ER, I, K, O, OO, P, PA, SCH, TH, UH2,
  V, W); 29/46 agree within ±1 on all three.
- Rank correlations: F2 +0.93, F1 +0.88, F3 +0.87, VA +0.85, FA +0.99
  (38/46 FA values exactly equal). **Closure flag: 46/46.**
- The differences are the SSI-263's refinements: duration-variant vowels
  that were byte-identical duplicates on the SC-01 (E1, AE1, UH1, UH3)
  get distinct acoustics; R becomes more retroflex (F3 code 3 → 1); S and
  F drop F1; U1 separates from U.
- 18 SSI-263 phonemes have no SC-01 counterpart at all (YI, IE, AI, the
  five holds, KV, R1, R2, L1, LF, IU1, and the international set) — this
  extraction is their only source.

Beyond validating the extraction end-to-end against independent silicon,
this settled the Hz scale: the chips share one code scale, so the SC-01
decap's **measured capacitor network** (the `bits_to_caps` weights and
filter topology MAME models) applies to these codes. Casso's phoneme
table now derives its frequencies from that network — e.g. AH1's F1 code
15 maps to 731 Hz, where the phonetics literature lists "father" at 730.
The earlier affine fits (F1 ≈ 264 + 27.9c, F2 ≈ 820 + 97.4c,
F3 ≈ 1360 + 94.4c) are retained here only as history.

## Hz scale

Starred columns in the table below still show the affine-fit values from
the original decode pass; the emulator itself now uses the SC-01
capacitor-network curves described above. Amplitude codes are 0–15,
linear DAC assumed.

## The decoded table

| Code | Phon | Example | Flags b00-b04 | FA | VA | F3 | NAS | F2 | F1 | F1 Hz* | F2 Hz* | F3 Hz* |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| $00 | PA | (pause) | `10100` | 0 | 0 | 12 | 0 | 9 | 7 | � | � | � |
| $01 | E | meet | `11110` | 0 | 12 | 14 | 0 | 14 | 2 | 319 | 2184 | 2682 |
| $02 | E1 | bent | `11110` | 0 | 10 | 13 | 0 | 14 | 5 | 403 | 2184 | 2588 |
| $03 | Y | before | `11110` | 0 | 11 | 14 | 0 | 13 | 1 | 291 | 2086 | 2682 |
| $04 | YI | year | `11110` | 0 | 6 | 11 | 0 | 12 | 2 | 319 | 1989 | 2399 |
| $05 | AY | please | `11110` | 0 | 12 | 14 | 0 | 14 | 3 | 347 | 2184 | 2682 |
| $06 | IE | any | `11110` | 0 | 9 | 15 | 0 | 15 | 1 | 291 | 2281 | 2776 |
| $07 | I | six | `11110` | 0 | 8 | 12 | 0 | 10 | 5 | 403 | 1794 | 2493 |
| $08 | A | made | `11110` | 0 | 8 | 11 | 0 | 11 | 6 | 431 | 1892 | 2399 |
| $09 | AI | care | `11110` | 0 | 8 | 10 | 0 | 9 | 6 | 431 | 1697 | 2304 |
| $0A | EH | nest | `11110` | 0 | 8 | 11 | 0 | 8 | 9 | 515 | 1600 | 2399 |
| $0B | EH1 | belt | `11110` | 0 | 8 | 11 | 0 | 9 | 10 | 542 | 1697 | 2399 |
| $0C | AE | dad | `11110` | 0 | 6 | 11 | 0 | 9 | 13 | 626 | 1697 | 2399 |
| $0D | AE1 | after | `11110` | 0 | 6 | 11 | 0 | 7 | 15 | 682 | 1502 | 2399 |
| $0E | AH | got | `11110` | 0 | 6 | 11 | 0 | 3 | 15 | 682 | 1112 | 2399 |
| $0F | AH1 | father | `11110` | 0 | 7 | 11 | 0 | 4 | 15 | 682 | 1210 | 2399 |
| $10 | AW | office | `11110` | 0 | 6 | 10 | 0 | 2 | 13 | 626 | 1015 | 2304 |
| $11 | O | store | `11110` | 0 | 8 | 11 | 0 | 1 | 7 | 459 | 918 | 2399 |
| $12 | OU | boat | `11110` | 0 | 9 | 11 | 0 | 1 | 5 | 403 | 918 | 2399 |
| $13 | OO | look | `11110` | 0 | 8 | 10 | 0 | 2 | 8 | 487 | 1015 | 2304 |
| $14 | IU | you | `11110` | 0 | 10 | 10 | 0 | 6 | 3 | 347 | 1405 | 2304 |
| $15 | IU1 | could | `11110` | 0 | 9 | 10 | 0 | 4 | 4 | 375 | 1210 | 2304 |
| $16 | U | tune | `11110` | 0 | 10 | 8 | 0 | 1 | 3 | 347 | 918 | 2116 |
| $17 | U1 | cartoon | `11110` | 0 | 10 | 7 | 0 | 0 | 1 | 291 | 820 | 2021 |
| $18 | UH | wonder | `11110` | 0 | 10 | 11 | 0 | 4 | 8 | 487 | 1210 | 2399 |
| $19 | UH1 | love | `11110` | 0 | 8 | 11 | 0 | 3 | 10 | 542 | 1112 | 2399 |
| $1A | UH2 | what | `11110` | 0 | 6 | 11 | 0 | 3 | 12 | 598 | 1112 | 2399 |
| $1B | UH3 | nut | `11110` | 0 | 7 | 11 | 0 | 5 | 12 | 598 | 1307 | 2399 |
| $1C | ER | bird | `11110` | 0 | 8 | 3 | 0 | 4 | 6 | 431 | 1210 | 1644 |
| $1D | R | roof | `11110` | 0 | 8 | 1 | 0 | 1 | 3 | 347 | 918 | 1455 |
| $1E | R1 | rug | `11110` | 0 | 8 | 4 | 0 | 3 | 2 | 319 | 1112 | 1738 |
| $1F | R2 | mutter | `11110` | 0 | 6 | 9 | 0 | 6 | 7 | 459 | 1405 | 2210 |
| $20 | L | lift | `11110` | 0 | 7 | 14 | 0 | 3 | 3 | 347 | 1112 | 2682 |
| $21 | L1 | play | `11110` | 0 | 8 | 15 | 0 | 5 | 1 | 291 | 1307 | 2776 |
| $22 | LF | fall | `11110` | 0 | 9 | 14 | 0 | 1 | 5 | 403 | 918 | 2682 |
| $23 | W | water | `11110` | 0 | 8 | 9 | 0 | 0 | 3 | 347 | 820 | 2210 |
| $24 | B | bag | `01110` | 0 | 8 | 12 | 0 | 3 | 1 | 291 | 1112 | 2493 |
| $25 | D | paid | `01110` | 0 | 8 | 14 | 0 | 9 | 1 | 291 | 1697 | 2682 |
| $26 | KV | tag | `11110` | 0 | 10 | 8 | 0 | 10 | 3 | 347 | 1794 | 2116 |
| $27 | P | pen | `01101` | 15 | 0 | 8 | 0 | 2 | 4 | 375 | 1015 | 2116 |
| $28 | T | tart | `01001` | 15 | 0 | 14 | 0 | 9 | 4 | 375 | 1697 | 2682 |
| $29 | K | kit | `01101` | 4 | 0 | 8 | 0 | 10 | 3 | 347 | 1794 | 2116 |
| $2A | HV | (hold vocal) | `10110` | 0 | 6 | 12 | 0 | 9 | 7 | 459 | 1697 | 2493 |
| $2B | HVC | (hold vocal closure) | `00110` | 0 | 15 | 12 | 0 | 9 | 7 | 459 | 1697 | 2493 |
| $2C | HF | heart | `10101` | 8 | 0 | 12 | 0 | 9 | 7 | 459 | 1697 | 2493 |
| $2D | HFC | (hold fric closure) | `00101` | 8 | 0 | 12 | 0 | 9 | 7 | 459 | 1697 | 2493 |
| $2E | HN | (hold nasal) | `10110` | 0 | 4 | 12 | 3 | 9 | 7 | 459 | 1697 | 2493 |
| $2F | Z | zero | `11000` | 10 | 1 | 13 | 0 | 2 | 3 | 347 | 1015 | 2588 |
| $30 | S | same | `11001` | 15 | 0 | 12 | 0 | 7 | 0 | 264 | 1502 | 2493 |
| $31 | J | measure | `11100` | 10 | 1 | 14 | 0 | 11 | 2 | 319 | 1892 | 2682 |
| $32 | SCH | ship | `11101` | 6 | 0 | 14 | 0 | 11 | 2 | 319 | 1892 | 2682 |
| $33 | V | very | `11100` | 4 | 3 | 9 | 0 | 3 | 2 | 319 | 1112 | 2210 |
| $34 | F | four | `11101` | 4 | 0 | 9 | 0 | 3 | 2 | 319 | 1112 | 2210 |
| $35 | THV | there | `11000` | 2 | 1 | 14 | 0 | 7 | 3 | 347 | 1502 | 2682 |
| $36 | TH | with | `11001` | 2 | 0 | 10 | 0 | 8 | 5 | 403 | 1600 | 2304 |
| $37 | M | more | `11110` | 0 | 10 | 9 | 3 | 3 | 0 | 264 | 1112 | 2210 |
| $38 | N | nine | `11110` | 0 | 8 | 13 | 3 | 8 | 0 | 264 | 1600 | 2588 |
| $39 | NG | rang | `11110` | 0 | 4 | 14 | 3 | 12 | 2 | 319 | 1989 | 2682 |
| $3A | :A | maerchen | `11110` | 0 | 8 | 10 | 0 | 9 | 7 | 459 | 1697 | 2304 |
| $3B | :OH | loewe | `11110` | 0 | 6 | 9 | 0 | 8 | 2 | 319 | 1600 | 2210 |
| $3C | :U | fuenf | `11110` | 0 | 10 | 9 | 0 | 7 | 1 | 291 | 1502 | 2210 |
| $3D | :UH | menu | `11110` | 0 | 10 | 10 | 0 | 9 | 0 | 264 | 1697 | 2304 |
| $3E | E2 | bitte | `11110` | 0 | 7 | 10 | 0 | 7 | 6 | 431 | 1502 | 2304 |
| $3F | LB | lube | `11110` | 0 | 8 | 14 | 0 | 1 | 1 | 291 | 918 | 2682 |
*Hz columns are fitted estimates as described above; PA has no meaningful
acoustic targets (amplitudes are 0).*
