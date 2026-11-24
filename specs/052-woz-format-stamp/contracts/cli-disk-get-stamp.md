# Contract: `casso disk get-stamp`

```
casso disk get-stamp <image> [--json]
```

| Case | Output | Exit |
|---|---|---|
| WOZ with a stamp | a table, below | 0 |
| WOZ with a stamp, `--json` | the stored JSON, byte for byte, then a line break | 0 |
| WOZ without a stamp | `<image> has no format stamp.` on stdout | 2 |
| WOZ with an unreadable stamp | `<image> has a format stamp Casso can't read: <reason>.` on stderr; with `--json` the stored bytes are printed | 3 |
| not a WOZ, or unreadable file | error on stderr | 1 |

Table:

```
Format stamp: capture, Casso 1.24.0, 2026-11-23 21:04 UTC
Capture: 3 reads, revolution 199,851 cycles, synchronized

Track  Encoding   Address    Data       Sectors  Good  Volume  Notes
  0    13-sector  D5 AA B5   D5 AA AD   13       13    254
  1    13-sector  D7 AA B7   D7 AA BD   13       13    254
 17.5  16-sector  D5 AA 96   D5 AA AD   16       16    254     own stream
 20    13-sector  DD AA F5   DD AA ED   13       12    254     weak, replaced 4
```

Tracks print as whole numbers, with .25, .5 and .75 for quarter positions. Several marks on one track print separated by commas.
