# Contract: `UserPrefs.json` shape and conversion

Applies to the `global` section of `%LOCALAPPDATA%\Casso\UserPrefs.json`.

## The new key

```json
"crtOverrides": {
  "AppleMonitorII/green": {
    "brightness": 1.12,
    "scanlines": { "intensity": 0.55 },
    "bloom":     { "enabled": true, "radius": 3.0 }
  },
  "AppleMonitorIIc/amber": {
    "persistence": 0.4
  }
}
```

The value object **reuses the v1 group structure verbatim**: `brightness`,
`contrast`, `gamma` and `persistence` at the top level, with `scanlines`,
`bloom` and `colorBleed` as sub-objects. This is the shape `CrtToJson` already
writes (`GlobalUserPrefs.cpp:279-309`) and the shape the field table already
describes (`:131-144`). `userOverride` is gone and every member is optional.

Reusing the grouping rather than flattening keeps the file legible to someone
hand-editing it and lets the existing per-group read path be adapted rather than
replaced.

## Rules

**Sparse.** Only fields the user set are present. Absent means no opinion, which
is not the same as any value. A pair with no overrides is not written at all.

**Emitted even when empty.** `"crtOverrides": {}` is written when the map holds
nothing. This is not cosmetic: it is what makes the conversion trigger stop
firing, since most files convert to nothing and would otherwise re-convert on
every load. `monitorTilt` already does this, with its `emplace_back` outside any
emptiness test at `GlobalUserPrefs.cpp:987`.

**Keys sorted.** So the file does not churn between saves.

**In `s_kKnownTopLevel`.** Required. A key that is both written by `ToJson` and
absent from that set is captured by the unknown-key sweep and emitted a second
time, and the file accretes one more stale copy per save. `monitorTilt` is in
exactly that state today and the live user file holds twelve copies of it.

**Unrecognized pair keys round-trip.** The map is not validated against
`s_kMonitors`. A key naming a monitor this build lacks is loaded, held and
written back unchanged.

## The legacy key

`crt` is **consumed but no longer emitted**. It stays in `s_kKnownTopLevel`.

That combination is deliberate and the file already has a precedent for it:
`printerAudioMuted` sits in the set commented "legacy (pre-toggle); consumed, no
longer emitted" (`GlobalUserPrefs.cpp:61`).

If `crt` were removed from the known set when it stopped being emitted, the
unknown-key sweep would capture the legacy block and echo it forever. The shape
trigger below would then never resolve, and the conversion would re-run and
re-clobber the user's overrides on every single load.

## Conversion

**Trigger, on shape**: `crt` present and `crtOverrides` absent. Never on
`$cassoGlobalPrefsVersion`, which nothing branches on and which an older build
will happily echo back after writing v1 data.

**Absence must not be tested with `HasObject`.** It is type-checked through
`GetObject`, so a hand-edited `"crtOverrides": null` or `"crtOverrides": []`
reads as absent and re-fires the conversion over live data. Test for the member
itself, then handle a wrong-typed value as present-but-empty.

**Rule**:

```
for mode in { color, green, amber, white }:
    if  crt[mode].userOverride == false:  emit nothing
    else:                                 emit "AppleMonitorII/<mode>"  with all eleven fields
                                          emit "AppleMonitorIIc/<mode>" with all eleven fields
```

**Pure.** A function of the parsed document only. No theme access, no catalog
access, no file system. This matters because prefs load before any machine or
theme is resolved, and it is what makes the conversion unit-testable as
`JsonValue` in, map out.

**The two monitor names are hardcoded string literals**, frozen against the
v1-era catalog. They are never read from `s_kMonitors`. A migration must be
frozen against the history it migrates, or an eight-monitor build fans a
//e-era user's tuning onto a Commodore tube.

**Both keys present**: `crtOverrides` wins, `crt` is ignored, and the next save
drops `crt`. This is also the state an older build leaves behind, since it
re-emits `crtOverrides` from its passthrough and adds its own default `crt`
block.

**Idempotent.** Running the conversion twice over the same document produces the
same result, and after one save the trigger cannot fire again.

## Downgrade

A file written by this version and then read and rewritten by an older build
keeps its overrides. The older build does not understand `crtOverrides`, so it
captures the key in its unknown-key sweep and re-emits it verbatim. It also
writes its own `crt` block of defaults, which this version ignores.

The visible consequence, stated plainly: the older build renders preset plus
theme rather than the user's tuning, because it has no `crt` block with the flag
set. That is the fresh-install look rather than a broken one, and the newer
build still shows the user's tuning.

An edit made in the older build lands in `crt` and is discarded by the newer
build. Accepted, because these are development worktrees rather than shipped
versions.

## Required coverage

In `GlobalUserPrefsTests.cpp` unless noted.

- A v1 document with the flag set converts to two keys per overridden mode, with
  all eleven values on each.
- A v1 document with the flag clear converts to no overrides.
- Load, save, load is a fixed point, and the conversion does not fire the second
  time.
- An empty map still emits the `crtOverrides` object.
- A document holding both keys loads from `crtOverrides` and drops `crt` on
  save. This is the same fixture as the downgrade round trip, written as a JSON
  literal rather than by simulating an older binary.
- A document whose `crtOverrides` is `null` or an array does not re-convert.
- Every top-level key appears exactly once in the saved text, asserted on the
  reparsed document rather than by substring. This covers the new key and
  `monitorTilt` together.
- A pair key naming an unknown monitor survives a load and save unchanged.
- An out-of-range value in a hand-edited file is clamped, not rejected.
- In `UserConfigStoreTests.cpp`: a legacy-layout upgrade writes the converted
  document rather than the raw parse.
