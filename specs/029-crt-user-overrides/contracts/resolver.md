# Contract: CRT value resolution

The one place the preset, theme and user tiers combine. Every consumer calls
this. No consumer restates it.

## Signature

```cpp
CrtResolved  ResolveCrt (const CrtValues        &  preset,
                         const ThemeCrtDefaults *  themeDefaults,   // may be null
                         const CrtOverrides     &  overrides);
```

The preset arrives **as a value, already selected**. It is not a mode index and
the resolver does not consult the preset table. This is what keeps the queued
`MonitorSpec` composition work a caller change: when a preset becomes a
composition of tube geometry and phosphor chemistry, this signature does not
move.

The resolver takes **no geometry**. Output width and height, pixel scale and the
picture rectangle are frame properties, not preferences. `MakeCrtParams` adds
them when it projects a `CrtResolved` into the shader constant buffer.

## Resolution table

Per field, independently. `has*` refers to the theme group the field belongs to.

| Theme declares the group | User overrides the field | Value | Source |
|---|---|---|---|
| no | no | preset | `Preset` |
| yes | no | theme | `Theme` |
| no | yes | user | `User` |
| yes | yes | user | `User` |

`themeDefaults == nullptr` is treated as "declares nothing" for every group.

**Gamma and persistence have only the first and third rows**, because no theme
group carries them. A resolver that reports `Theme` for either is wrong, and a
Display page that offers a theme-default label for either is wrong.

## Group atomicity is a theme-side property only

A theme group applies as a unit. If a theme declares `bloom`, it supplies
`enabled`, `radius` and `strength` together, because `ThemeLoader` fills the
whole group from one JSON object.

A user override is per field and never atomic. Overriding `bloomStrength` leaves
`bloomEnabled` and `bloomRadius` resolving through the tiers below. This
asymmetry is deliberate: it is what makes a theme's intent survive a user's
single adjustment.

## Provenance

`CrtResolved::source` is indexed by `CrtField` and has one entry per field.

The source is **what supplied the value**, never a comparison of numbers. A user
override whose value equals what the preset would have given still reports
`User`. A theme value that equals the preset still reports `Theme`.

This is the substantive change from the current badge behavior, which infers
provenance by comparing floats with an epsilon and is therefore wrong in exactly
the case where a user deliberately set a value that matches a default.

## What the caller must guarantee

**One monitor and mode per resolution.** The preset and the override lookup must
be selected from the same monitor and the same mode. The current render path
loads the color mode twice in adjacent statements; with a keyed lookup a
preemption between those loads could pair one mode's overrides with another
mode's preset. Load the selector once into a local.

**No file access on the resolution path.** Resolving a monitor identity from a
machine's JSON costs a path lookup, a file read and a parse. The render path
resolves every presented frame. The joined key strings are cached and
invalidated on machine switch and mode change; the resolved values are not
cached, because that would add eleven more invalidation sources in the settings
setters and a missed one presents as a slider that no longer moves the picture.

## Mutation

The override set owns its own edits. Callers do not construct one field-by-field
in a UI callback.

```cpp
void  Touch    (CrtField field, float value);   // or the bool overload
void  Clear    (CrtField field);
void  ClearAll ();
bool  IsEmpty  () const;
```

`Touch` sets exactly one field. It never seeds siblings. This is the whole
difference from `PromoteActiveToOverride`, which seeded eleven fields before
latching a flag and is deleted.

`ClearAll` backs Restore Defaults. It erases and records nothing, so the pair
returns to the preset and theme tiers with no latch.

## Required coverage

The resolver is pure and the test project compiles it, so the matrix is
covered directly with no mock and no fixture.

- The four-row table above, for each of the nine fields that have a theme group.
- The two-row table for gamma and persistence, asserting that `Theme` never
  appears for either.
- A theme change leaves user fields intact and moves every non-overridden field.
- `Clear` on one field falls back to theme where the theme declares it and to
  preset where it does not.
- `Touch` flips provenance to `User` for that field only, leaving the other ten
  sources unchanged.
- An override equal to the resolved default still reports `User`.
- A theme declaring a group supplies every field in that group, and a user
  override of one member does not disturb the others.
