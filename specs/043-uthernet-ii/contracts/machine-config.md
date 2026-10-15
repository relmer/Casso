# Contract: Machine Configuration

## Shipped definitions

`Resources/Machines/Apple2`, `Apple2Plus`, `Apple2e` and `Apple2eEnhanced` gain, in `slots`:

```json
{ "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false }
```

`$cassoMachineVersion` becomes 13 in those four definitions, and each one's version-12 normalized SHA-256 is added to the prior-hash table. The //c definition does not change.

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
- An invalid `mac`, `protocol` or port is replaced by its default and produces a load warning; the machine loads.

## User configuration upgrade

`MachineConfigUpgrade::MigrateUserConfig` adds the disabled card entry to a user `slots` array that has no `uthernet2` entry, at slot 3 when slot 3 is free and otherwise at the lowest free slot from 1 to 7, and makes no change when every slot is taken. It runs only on a user array that already contains a `slots` key, so the //c, which has none, is unaffected. The operation is idempotent.

## Device registry

`ComponentRegistry::RegisterBuiltinDevices` registers `"uthernet2"` with `UthernetCard::Create`. The Settings hardware tree captions it "Uthernet II (Ethernet)".
