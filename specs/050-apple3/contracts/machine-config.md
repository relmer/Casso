# Contract: Apple /// machine definition

`Resources/Machines/Apple3/Apple3.json`. The Apple /// Plus file differs in
`name`, `releaseYear`, and `memory`.

```jsonc
{
    "$cassoMachineVersion": 1,
    "monitor": "AppleMonitorIII",
    "name": "Apple ///",
    "releaseYear": 1980,
    "timing": {
        "videoStandard": "ntsc",
        "clockSpeed": 2040968,          // average ticks per second
        "cyclesPerScanline": 130        // ticks per line
    },
    "systemRom": {
        "address": "0xF000",
        "file": "Apple3.rom"
    },
    "memory": "128K",                   // "128K" or "256K"; the Plus accepts "256K" only
    "externalDrives": 1,                // 0 to 3
    // slots: only occupied slots are listed, as { "slot": n, "device": "..." }
    "ports": [
        { "name": "serial",    "device": "" },
        { "name": "joystickA", "device": "" },
        { "name": "joystickB", "device": "" }
    ]
}
```

## Rules

- No `characterRom`: the character set is in RAM. A `characterRom` key in an
  Apple /// file is an error reported when the file loads.
- `memory` other than the machine's options is an error with the options
  listed.
- `externalDrives` outside 0 to 3 is an error.
- A slot `device` the Apple /// does not support is an error with the
  supported devices listed. The first release supports `""` and `"profile"`.
- `"profile"` in a slot before 048 is merged into the branch fails the
  same way; the default file lists slot 4 as empty until then.
- The timing numbers are from the hardware map's timing rows: 130 ticks per
  line, one stretched by two master clock periods, so 912 periods of
  14.31818 MHz per line (SVC 5.1, 5.5).
- Each embedded file has a separate `$cassoMachineVersion`, so the Apple /// file
  starts at 1 and the Apple II files do not change.

## Command line

`--machine Apple3` and `--machine Apple3Plus`. `--disk1` to `--disk4` mount
the four drives; `--disk3` and `--disk4` on an Apple II machine are errors.
