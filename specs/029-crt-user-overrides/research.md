# Research: Per-field CRT user overrides

Every decision below was reached before implementation began, through two
adversarial review panels over the proposed design and one inventory pass over
the code. Findings were verified against the tree rather than accepted from the
panels. Line references are as of `9cba5504`.

---

## D1. Override granularity: per field, not per group

**Decision**: A user's adjustment is stored as eleven independently optional
values. Theme groups stay atomic under their existing `has*` flags. "Group"
survives only as a presentation unit on the Display page.

**Rationale**: The original proposal was per group, justified as mirroring
`ThemeCrtDefaults`. That justification was factually wrong. The theme variant
merge at `Casso/Ui/ThemeLoader.cpp:617-636` is already a per-field patch applied
under a per-group flag, so per-group storage would mirror nothing.

Per-group storage also keeps the frozen-snapshot defect one level down. A touch
is one control: each toggle fires only its own callback
(`DisplayPage.cpp:548-553`) and each bridge lambda writes one field
(`SettingsDisplayCrtBridge.cpp:535-546`). Sibling fields in the group can only
come from a seeding step, so the seeded siblings would then outlive every later
theme. With real shipped data: Retro Terminal's //e bloom is
`{on, 1.8, 0.65}` (`Resources/Themes/RetroTerminal/theme.json:48`), a user drags
strength to 0.50, switches to Skeuomorphic whose //e bloom is `{on, 0.8, 0.25}`
(`Resources/Themes/Skeuomorphic/theme.json:32`), and gets radius 1.8 with no
badge to explain it.

Per field also gives gamma and persistence a home. Neither appears in
`ThemeCrtDefaults` (`Casso/Ui/ThemeLoader.h:45-66`), so under group storage they
belonged to no group.

**Alternatives considered**: Per group, as above. Per whole block with a
"follow theme" reset added, which is the minimum change but leaves the README
sentence false. A stored base snapshot plus deltas, which doubles the state that
can disagree.

---

## D2. The persisted key: the pair of monitor and mode

**Decision**: `<monitorConfigName>/<mode>`. The monitor segment is the resolved
`MonitorSpec::configName`. The mode segment is one of the four strings already
on disk. Both segments always present, no bare form, no wildcard tier, no fifth
`mono` token.

**Rationale**: This decision was made twice. The first panel recommended keeping
the four phosphor strings as the permanent key, reasoning that an override is a
delta against the preset so it should be keyed the way the preset is. That
recommendation was sound against the catalog as it stood, two monitors both
green, with a color entry that had never been written.

The owner then supplied a roadmap: an Apple //e color monitor, then the IIgs
mono and color pair, then monitors used with a VIC-20, a C64 and an Atari 800.
A second panel reconsidered the key against that growth and scored the
phosphor-only key fatal.

The breaking case is roadmap step two, which puts two real color tubes in the
catalog at once. A composite //e set and an analog RGB IIgs set would both write
the single member `color`, so tuning color bleed on the composite tube would
change a picture that is not even a composite signal path. Step one leans on the
same defect more mildly: `DisplayPage.cpp:242` offers all four modes on every
machine, so `color` today already holds tuning for a green tube driven in color.

The first panel's escape hatch, that a new monitor gets its own preset and
therefore its own key, is the failure rather than the fix. It means shipping a
preset-table edit silently resets a user's tuning, triggered by a change they
cannot see, in a file with no migration hook.

The first panel's objections to the pair both turned out to be artifacts of the
two-monitor catalog. "Entries for fictional pairs" does not apply to a sparse
map, which enumerates nothing and stores only touched pairs. "A migration table"
is not required either, as D4 shows.

The pair also answers the owner's concern that "white" will mean two things once
a color tube exists, and it does so with no new vocabulary. A P4 mono tube is
`A2M6016/white`; a colorburst-killed color tube is `MonitorIIeColor/white`.
Different members of the same map.

**Why the resolved name and not the raw one**: `MonitorCatalog::ByName` returns
`Default()` for an unrecognized name with no diagnostic
(`Casso/Config/MonitorCatalog.h:88-98`). Keying on the raw string from a
machine's JSON would file a user's tuning under a tube that never lights a pixel.

**Alternatives considered**: Phosphor-only, as above. Two maps split by axis,
tube-driven fields keyed by monitor and phosphor-driven fields keyed by
phosphor, which scored last because it freezes the tube-versus-phosphor
partition into the file, making every later correction a lossy migration. An
opaque string produced by a documented function, which defers the axis decision
but breaks every stored key the day the function changes.

---

## D3. Storage shape: a new top-level key, sparse, emitted even when empty

**Decision**: A `std::map` under a new top-level JSON key `crtOverrides`, added
to `s_kKnownTopLevel`, written only for pairs that have at least one override,
keys sorted, and the object emitted even when the map is empty.

**Rationale**: A new key rather than a reshaped `crt` is what lets an older
build round-trip the data. Unknown top-level keys are captured at
`GlobalUserPrefs.cpp:1222-1228` and re-emitted at `:1010-1013`, so an older
build preserves `crtOverrides` without understanding it.

Emitting the object even when empty is what retires the conversion trigger.
Without that rule the common case fires forever, because most files have four
blocks with the flag false and therefore convert to nothing. `monitorTilt`
already establishes the pattern: its `emplace_back` at `:987` sits outside any
emptiness test.

**The trap this decision walks around**: `crt` must stay in `s_kKnownTopLevel`
even though it stops being emitted. Dropping a key from the emit path invites
dropping it from the known set, and if `crt` leaves that set the passthrough
loop captures the legacy block and echoes it forever, so the shape trigger never
resolves and the conversion re-runs on every load. The tree already has the
correct pattern for this: `printerAudioMuted` sits in the known set at
`GlobalUserPrefs.cpp:61` commented "legacy (pre-toggle); consumed, no longer
emitted."

---

## D4. Conversion: shape-triggered, pure, fanned onto both v1-era monitors

**Decision**: Trigger on shape, legacy `crt` present and `crtOverrides` absent,
never on the version stamp. A pure function of the parsed document. Each legacy
block with the flag false yields nothing; each with the flag true yields all
eleven fields under both `AppleMonitorII/<mode>` and `AppleMonitorIIc/<mode>`,
with those two names as hardcoded string literals frozen against the v1-era
catalog.

**Rationale for shape rather than the stamp**: Nothing branches on
`$cassoGlobalPrefsVersion` anywhere. `FromJson` reads it with a fallback and no
upper bound and `ToJson` echoes what it read. One `UserPrefs.json` is shared by
worktrees running builds of different ages, so an older build would read a
stamped file, find no mode keys, write four default blocks, and re-emit the
stamp, destroying the data while claiming the new version. All three migrations
already shipped in this file are shape-detected with the legacy key kept in the
known set and no stamp bump.

**Rationale for hardcoded names**: A migration must be frozen against the
history it migrates. Reading `s_kMonitors` means an eight-monitor build fans a
//e-era user's tuning onto a Commodore tube they have never booted.

**Rationale for both monitors rather than the default**: A v1 block was
monitor-independent, because the render path indexed by color mode alone
(`EmulatorShell.cpp:7273`). The two monitors in the v1 catalog are therefore the
only two tubes the user could have been looking at, so writing both is the
honest projection and the picture is identical on each afterwards. Fanning only
to the default would take a //c-only user's 9-inch numbers off the //c and put
them on a //e they never tuned.

**Rationale for lossless rather than decomposing**: An earlier proposal compared
each stored block against the enumerable set of resolvable defaults and treated
an exact match as "never customized". That was dropped. It needs a distance
metric that has no natural definition across incommensurable units, it ties on
real shipped data (one 10% brightness click on the Skeuomorphic base ties the
base against its //c variant), it needs the theme manager at a point in startup
where prefs load first, and it needs the preset generations that have already
shipped and changed. Lossless conversion dissolves all four problems and can be
refined later inside the legacy-read branch, because the sparse format is final.

**What is honestly lost**: nothing that was applied, but provenance is not
preserved. `PromoteActiveToOverride` defers to `ApplyActiveDefaults`, which
seeds all eleven fields from preset plus theme before setting the flag
(`SettingsDisplayCrtBridge.cpp:416`, `:441`). So a user who nudged one slider
has one chosen value and ten seeded ones, and after conversion the Display page
reports all eleven as user-set. This is a property of v1 storing no per-field
provenance, not of this design, and it self-corrects the first time the user
presses Restore Defaults on that pair.

---

## D5. The legacy block is dropped, not frozen and re-emitted

**Decision**: `crt` stops being emitted after conversion. It is not maintained
for older builds.

**Rationale**: The alternative was to keep writing a frozen `crt` block so older
worktree builds keep their picture. It was rejected once the actual degradation
was checked. An older build reading a file with no `crt` section keeps its
struct defaults, and `userOverride` defaults to false
(`GlobalUserPrefs.h:134`), so it renders preset plus theme, which is the
fresh-install look rather than a broken one. The user's own overrides survive in
`crtOverrides` through the passthrough, and the newer build still reads them.

Maintaining the frozen block also had a cost with no clean answer: it could not
be regenerated from the new map, because producing resolved values needs the
theme at save time, which would drag a UI header into `Config` and make the
block flap depending on which machine was open.

---

## D6. One pure resolver, returning values and provenance

**Decision**: `ResolveCrt (preset, themeDefaults, overrides)` returns resolved
values plus a per-field source. It takes the preset by value rather than by mode
index. `MakeCrtParams` becomes a projection of it and keeps geometry out.

**Rationale**: The layering chain is currently written four times, at
`CrtPostProcess.cpp:103-125` and three times in the settings bridge
(`ReseedFromActiveMode` at `:127-165`, `PublishDefaultsHint` at `:217-269`,
`ApplyActiveDefaults` at `:394-439`). Only the first is compiled by the
`UnitTest` project. That duplication has shipped two defects: the
resize-changes-brightness bug recorded at `EmulatorShell.cpp:7262-7265`, and the
base-theme read fixed on this branch in `98d37eb2`.

Taking the preset by value rather than by index is what makes the queued
`MonitorSpec` CRT parameter work a caller change instead of a resolver change.
When a preset becomes a composition of tube geometry and phosphor chemistry,
`ResolveCrt` does not move.

Returning a per-field source is what lets the Display page stop deciding
provenance by comparing values. That comparison is currently eight float
comparisons with an epsilon, and it is wrong in a way the source lookup cannot
be: a value the user set that happens to equal the default reads as a default.

---

## D7. Monitor identifiers are frozen, and were renamed while that was free

**Decision**: A shipped `configName` is permanent, stated in the catalog banner
and pinned by a test. The two existing names were changed first, in `d3f45e1b`,
from `MonitorII` and `MonitorIIc` to `AppleMonitorII` and `AppleMonitorIIc`.

**Rationale**: `configName` is internal today. It has three uses: the
`monitorTilt` map key (`EmulatorShell.cpp:1494`, `:1518`), the `ByName` lookup,
and one shipped machine JSON. Nothing displays it and `MonitorSpec` has no
display-name field. So everything that references it renames atomically with the
code, except the user's prefs file.

That scoped the risk exactly: a rename is free now and expensive once
`crtOverrides` holds entries. The rename was also likely to be wanted, because
`MonitorII` reads generic the moment several Apple monitors are cataloged, and
vendor qualification is what the growth to Commodore and Atari tubes requires.
The cost at the time of the rename was one empty `monitorTilt` map.

Freezing needs enforcement rather than a comment, because `ByName` recovers
silently. A test over `s_kMonitors` makes a rename fail the build instead of
orphaning data.

---

## D8. The version stamp stays at 1

**Decision**: `$cassoGlobalPrefsVersion` is not bumped. A comment records that
it is reserved for changes of meaning rather than of shape.

**Rationale**: Nothing branches on it and the conversion trigger is deliberately
shape-based, so a bump would advertise a trigger that does not exist. It would
also be able to lie: `ToJson` echoes the member it read rather than writing the
current constant, so a file stamped 2 could have had its contents written by an
older build afterwards. The stamp keeps its real job, which is a change of
meaning such as a units change requiring stored values to be converted. The
emulated-pixel change to bloom radius was that kind of change and is the example
worth recording.

---

## Findings that changed the plan after the design was settled

These came from the inventory pass and were verified directly.

**`monitorTilt` grows without bound.** It was described as emitted twice. It
compounds one member per save, because the passthrough capture takes the stale
copies a previous save emitted. The live user file holds twelve. One line fixes
it and the file self-heals on the next save.

**The shape trigger cannot use `HasObject` for the absence half.**
`JsonValue::HasObject` is type-checked through `GetObject`, so a hand-edited
`"crtOverrides": null` or `[]` reads as absent and re-fires the conversion over
live data. The file is documented as meant to be read and hand-edited, so this
is reachable.

**`MonitorCatalog.h` is not self-contained.** It names `JsonValue` and
`JsonType` while including neither, and compiles today only because both current
includers reach `Core/JsonValue.h` first. Any new translation unit that includes
it fails.

**The legacy-file upgrade writes the unconverted document.**
`UserConfigStore::MigrateLegacyFiles` keeps the raw parsed value at
`UserConfigStore.cpp:1542` and writes that, while its sibling branch at `:1550`
writes `prefs.ToJson()`.

**Map order is not enum order.** The four mode tokens are stored in
`SettingsColorMode` order, but a map keyed by the pair emits amber, color,
green, white. Any expected-order assertion written from the enum will be wrong.

**`DisplayPage.cpp` can be added to the test project cheaply.** The earlier
claim that the settings pages are untestable was too broad.
`HardwarePage.cpp` is already a project entry and adding `DisplayPage.cpp`
pulls in nothing new.

**Widget setters cannot fire change callbacks.** `DxuiSlider::SetValue`,
`DxuiToggle::SetChecked` and `DxuiDropdown::SetSelected` all assign directly,
so reseeding the page cannot create an override and no suppression flag is
needed. The real hazard there is quantize-and-clamp: the color bleed slider has
a floor of 1.0 while three presets carry 0.0, so a value round-tripped through
the widget comes back different. The rule that follows is to never decide
whether an override exists by reading a value back out of a widget, which the
source-driven badge makes unnecessary anyway.
