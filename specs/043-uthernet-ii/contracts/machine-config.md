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

A user `slots` array replaces the shipped one wholesale, so a user who saved hardware settings before this feature has no `uthernet2` entry. No migration adds one: enabling the card on the Network page adds the entry to the user array, at slot 3 when it is free and otherwise at the lowest free slot from 1 to 7. A machine with no `slots` key (the //c) does not offer the card.

## Device registry

`ComponentRegistry::RegisterBuiltinDevices` registers `"uthernet2"` with `UthernetCard::Create`. The Settings hardware tree captions it "Uthernet II (Ethernet)".
