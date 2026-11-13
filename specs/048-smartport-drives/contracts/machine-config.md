# Contract: machine JSON for cards, drives and the //c Plus

Extends the machine JSON of `Resources/Machines/*/*.json` and the user copies
under `Machines/`. Each embedded default's `$cassoMachineVersion` increases by
one, and `MachineConfigUpgrade` adds the new entries to a user copy only when
the user has not configured that slot.

## Slot cards

```jsonc
"slots": [
    // Apple II SCSI Card: ports are SCSI IDs 0-6; an empty string is a free ID
    { "slot": 7, "device": "apple-scsi-card", "rom": "341-0437-A.rom",
      "ports": [ "", "", "", "", "", "",
                 { "device": "hd20sc", "image": "C:/Disks/Total Replay.hdv" } ] },

    // ProFile interface card: one drive, the card ROM matches the model
    { "slot": 5, "device": "profile-card", "rom": "341-0271-A.rom",
      "ports": [ { "device": "profile-5mb", "image": "...", "realTiming": false } ] },

    // CFFA 2.0: one card image, partitioned as CffaLayout reads it
    { "slot": 7, "device": "cffa2", "rom": "cffa20eec02.rom",
      "ports": [ { "device": "cf-card", "image": "C:/Disks/card.img" } ] },

    // A2Pico: a card folder with A2retroNET.txt
    { "slot": 4, "device": "a2pico", "folder": "C:/A2Pico" },

    // UniDisk 3.5 interface card: a SmartPort chain
    { "slot": 5, "device": "liron-card", "rom": "Liron.rom",
      "ports": [ { "device": "unidisk35", "image": "..." },
                 { "device": "unidisk35", "image": "" } ] },

    // Apple II 3.5 Disk Controller Card: up to two Apple 3.5 Drives
    { "slot": 5, "device": "apple35-card", "rom": "341-0438-A.rom",
      "ports": [ { "device": "apple35-drive", "image": "..." }, "" ] }
]
```

## //c and //c Plus disk port

```jsonc
"ports": [
    // A chain: SmartPort devices first, then at most one 5.25-inch drive
    { "name": "disk", "chain": [ { "device": "unidisk35", "image": "..." },
                                 { "device": "disk-iic",  "image": "..." } ] }
]
```

The //c Plus also accepts `apple35-drive` in the chain. Its internal drive is
built in and not a port.

## Rules

- A device a machine cannot host fails to load, with an error that lists
  the machines that accept it, and the slot is left empty.
- `realTiming` is valid on `hd20sc`, `scsi-disk`, `profile-5mb` and
  `profile-10mb`, and defaults to `false`.
- Image paths are stored as the user picked them; with a missing file the
  drive is empty and a notice appears, as for the Disk II.
- ROM files resolve under `Devices/<Card>/` in the ROM cache, as `Disk2.rom`
  resolves under `Devices/DiskII/`.
