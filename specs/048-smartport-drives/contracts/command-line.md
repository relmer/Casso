# Contract: command-line flags

Parsed in core beside `--disk1` and `--disk2`. Each attaches an image for this
run only; the saved configuration is left as it was.

| Flag | Meaning |
|---|---|
| `--hd <slot>=<image>` | Attach to unit 1 of the block device card in `<slot>`. |
| `--hd <slot>.<unit>=<image>` | Attach to a unit: a SCSI ID (0-6) on the SCSI card, a drive number on the A2Pico or the Liron card. |
| `--smartport <n>=<image>` | Attach to the nth device of the //c or //c Plus disk port chain, counting from 1. |
| `--a2pico <slot>=<folder>` | Point the A2Pico in `<slot>` at a card folder. |

Errors use the fixed error layout:

```
Error: no block device in slot 6
       The --hd flag attaches to a SCSI, ProFile, CFFA, A2Pico or UniDisk 3.5
       interface card. Slot 6 has a Disk II.
```

The image is validated as an attach from the UI is (FR-017).
