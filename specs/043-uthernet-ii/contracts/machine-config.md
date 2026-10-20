# Contract: Machine Configuration

## Shipped definitions

`Resources/Machines/Apple2`, `Apple2Plus`, `Apple2e` and `Apple2eEnhanced` gain, in `slots`:

```json
{ "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false }
```

Each of those four definitions takes the next `$cassoMachineVersion` (Apple2 11, Apple2Plus 14, Apple2e 13, Apple2eEnhanced 8), and each one's previous normalized SHA-256 is added to the prior-hash table. The //c definition does not change.

## Card settings

On the card's slot entry, all keys optional:

```json
"network": {
    "mac": "02:CA:55:12:34:56",
    "lanAccess": false,
    "portForwards": [
        { "protocol": "tcp", "hostPort": 8080, "guestPort": 80 },
        { "protocol": "udp", "hostPort": 6502, "guestPort": 6502 }
    ]
}
```

Loading rules:

- A missing `network` object, or missing keys, take the defaults: no MAC, `lanAccess` false, no forwards.
- An invalid `mac`, `protocol` or port is replaced by its default; the machine loads.

## Existing user configurations

Loading merges the shipped slot entries with the user's by slot number, so a user file saved before this feature gets the card's shipped entry, turned off, with no migration. The user file contains only what differs from the shipped entry: `enabled`, and the `network` object once it differs from the defaults. A card moved to a slot with no shipped entry (by editing the user file) is saved whole and replaces the shipped entry when loaded. The //c has no `slots` key and no card.

## Device registry

`ComponentRegistry::RegisterBuiltinDevices` registers `"uthernet2"` with `UthernetCard::Create`. The Settings hardware tree captions it "Uthernet II (Ethernet)".
