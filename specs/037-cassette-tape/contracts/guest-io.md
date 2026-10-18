# Contract: Guest-visible cassette I/O

What emulated software observes. This is the hardware contract the feature
must honor; it is independent of how Casso implements it.

| Address | Models | Access | Behavior |
|---|---|---|---|
| $C020-$C02F | ][, ][+, //e | read or write | Toggles the cassette output flip-flop. A read returns what that address returned before this feature (floating bus on the ][/][+, 0 on the //e). |
| $C060, $C068 | ][, ][+, //e | read | Bit 7 = cassette input level at the cycle of the access. Bits 0-6 unchanged from today. |
| $C060 | //c | read | Unchanged: RD80SW in bit 7. No cassette. |
| $C020 | //c | any | Unchanged. |

Timing: the input level is sampled at the bus cycle of the access, not at the
start of the instruction. Tape time advances only while the CPU runs.

Idle line: with no tape playing, bit 7 reads 0.

The load path does no decoding: a guest sees transitions exactly where the
recording has them, and nothing else.
