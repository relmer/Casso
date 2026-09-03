# Data Model: Per-field CRT user overrides

## The eleven fields

One vocabulary, shared by every tier. Ranges and defaults are as they exist
today at `Casso/Config/GlobalUserPrefs.h:123-133`.

| Field | Type | Range | Default | Theme group | Notes |
|---|---|---|---|---|---|
| `brightness` | float | 0.0 .. 2.0 | 1.0 | `brightness` | |
| `contrast` | float | 0.0 .. 2.0 | 1.0 | `contrast` | |
| `gamma` | float | 0.5 .. 2.5 | 1.0 | none | 1.0 bypasses the pass |
| `scanlinesEnabled` | bool | | false | `scanlines` | |
| `scanlinesIntensity` | float | 0.0 .. 1.0 | 0.5 | `scanlines` | |
| `bloomEnabled` | bool | | false | `bloom` | |
| `bloomRadius` | float | 0.0 .. 4.0 | 1.0 | `bloom` | emulated pixels |
| `bloomStrength` | float | 0.0 .. 1.0 | 0.5 | `bloom` | |
| `colorBleedEnabled` | bool | | false | `colorBleed` | |
| `colorBleedWidth` | float | 0.0 .. 8.0 | 1.0 | `colorBleed` | emulated pixels |
| `persistence` | float | 0.0 .. 0.99 | 0.0 | none | decay factor |

**Gamma and persistence have no theme group.** `ThemeCrtDefaults`
(`Casso/Ui/ThemeLoader.h:45-66`) carries `hasBrightness`, `hasContrast`,
`hasScanlines`, `hasBloom` and `hasColorBleed` and nothing else. So nine fields
have three possible sources and two have only two. The resolution matrix must
state this rather than leaving the missing arm implicit, and the Display page
must not offer a theme-default label for a row that cannot have one.

**One stale comment to correct.** `colorBleedWidth` is commented "output
pixels" at `GlobalUserPrefs.h:132`. Commit `967f02f2` moved both bloom radius
and color bleed width to emulated pixels. The comment was not updated.

## Types

### `CrtValues`

A complete set of the eleven values. This is `GlobalUserPrefs::Crt` moved out of
`GlobalUserPrefs` and renamed, with `userOverride` deleted. It remains the
element type of the preset table and becomes the resolved output type.

Moved because a nested `Crt` inside a class that no longer stores one is
misleading. 25 references across 6 files.

### `CrtOverrides`

The same eleven fields, each optional. Absent means no opinion, which is not the
same as any value. Carries `IsEmpty()` and a defaulted `operator==`.

The defaulted equality is load-bearing rather than decorative. Cancel restore
compares a snapshot map against the live map, and `std::map::operator==`
requires the mapped type to have one.

### `CrtField`

An enumeration of the eleven fields plus `Count`. Needed because provenance is
reported per field and both the resolver's output array and the Display page's
row walk need a shared index. Without it, a provenance test has no way to name
what it is asserting about.

### `CrtSource`

`Preset`, `Theme`, `User`. What supplied the value that is being displayed.

### The override map

`std::map<std::string, CrtOverrides>`, keyed by `<monitorConfigName>/<mode>`.
Replaces `Crt crtByMode[kCrtModeCount]`.

A `std::map` rather than an unordered one, because the serializer writes keys in
sorted order so the file does not churn between saves. This matches
`monitorTilt`, which is the working precedent in this file for a string-keyed
sparse map that omits untouched entries.

## Key format

```
<monitorConfigName> "/" <mode>
```

**Monitor segment**: the resolved `MonitorSpec::configName`. Frozen once
shipped. Currently `AppleMonitorII` or `AppleMonitorIIc`.

**Mode segment**: one of `color`, `green`, `amber`, `white`, taken from
`s_kpszCrtModeKeys` (`GlobalUserPrefs.cpp:31-33`). These are the strings already
on disk in v1 files, reused rather than re-invented.

Both segments are always present. There is no bare form and no wildcard.

**The key is built in one place**, `MakeCrtOverrideKey` beside `ResolveCrt`, and
that place is compiled by the test project. The format is a contract carrying
the identifier freeze, the sort order and the distinctness guarantee, so it
cannot be assembled ad hoc in the shell or the settings bridge, neither of which
a test can reach. See [contracts/resolver.md](contracts/resolver.md).

**Sort order is not enum order.** The tokens are declared in `SettingsColorMode`
order, but the map emits `amber`, `color`, `green`, `white`. The monitor halves
sort cleanly for an incidental reason worth knowing: `/` is 0x2F, below every
lowercase letter, so every `AppleMonitorII/...` key precedes every
`AppleMonitorIIc/...` key and the shared prefix causes no interleaving.

**Unrecognized keys are kept.** The map is not validated against `s_kMonitors`.
A key naming a monitor this build does not have is loaded, held and written back
unchanged, because another build sharing the same file may have that monitor.
Dropping it would be exactly the silent orphaning the frozen-identifier rule
exists to prevent.

## Relationships

```
MonitorSpec ──── configName ────┐
  (catalog, frozen)              ├──► "AppleMonitorIIc/amber" ──► CrtOverrides
SettingsColorMode ─── token ────┘         (map key)                (sparse)

CrtPresets::GetPreset(mode) ──► CrtValues ──┐
ThemeManager::ActiveCrtDefaults() ──────────┼──► ResolveCrt ──► CrtResolved
map lookup on the pair key ─────────────────┘                    { CrtValues
                                                                 , CrtSource[11] }
```

`CrtResolved` feeds two consumers. `MakeCrtParams` projects it into the shader
constant buffer, adding geometry the resolver does not own. The Display page
reads its values into the widgets and its sources into the row labels.

## State transitions

An override for one field of one pair moves between exactly two states.

| From | Event | To |
|---|---|---|
| absent | user changes that control | present, holding the user's value |
| present | user resets that row | absent |
| present | user presses Restore Defaults | absent, along with the pair's other fields |
| present | active theme changes | present, unchanged |
| present | machine or mode changes | present, and no longer consulted |

There is no third state and nothing latches. A theme change writes nothing,
which is the change from today, where it clears every flag.

The transition that does **not** exist is worth stating: an override is never
created as a side effect of displaying a value. Reseeding the page cannot
produce one, because `DxuiSlider::SetValue`, `DxuiToggle::SetChecked` and
`DxuiDropdown::SetSelected` all assign without firing their change callbacks.

## Validation

**On read**, each present field is clamped to the range in the table above, the
same clamps `CrtModeFromJson` applies today. A field outside its range in a
hand-edited file is clamped rather than rejected, because the file is documented
as meant to be hand-edited and refusing to load would leave the user unable to
fix it by changing a setting.

**A present field is never dropped for equalling the resolved default.** Whether
an override exists is a fact about what the user did, not about the number. An
override that happens to match is kept, and the row reports itself as custom.

**On write**, a pair whose `CrtOverrides` is empty is omitted entirely. The
`crtOverrides` object itself is still emitted when the map holds nothing, which
is what retires the conversion trigger.

## What is deleted

| Removed | Where | Why |
|---|---|---|
| `Crt::userOverride` | `GlobalUserPrefs.h:134` | replaced by per-field presence |
| `crtByMode[4]` | `GlobalUserPrefs.h:142` | no persistence path and no reader once the resolver takes a preset by value |
| `kCrtModeCount` | `GlobalUserPrefs.h:141` | the mode count stops being a storage dimension |
| the `crt` JSON object | emitted at `GlobalUserPrefs.cpp:969` | superseded; the key stays in `s_kKnownTopLevel` as consumed-but-not-emitted |
| `PromoteActiveToOverride` | `SettingsDisplayCrtBridge.cpp:341` | existed only to seed a block before latching |
| `ApplyActiveDefaults` | `SettingsDisplayCrtBridge.cpp:394` | Restore Defaults becomes an erase |
| `DisplayDefaultsHint::values` and the five `*FromTheme` bools | `DisplayPage.h:63-68` | the page stops needing values to decide provenance |
