# Feature Specification: Per-field CRT user overrides

**Feature Branch**: `claude/theme-crt-defaults-handoff-f2c5b1`

**Created**: 2026-09-03

**Status**: Draft

**Input**: User description: "Per-field CRT user overrides, keyed by monitor and mode, replacing the whole-block userOverride flag."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A tweak stays a tweak (Priority: P1)

Someone likes the picture their theme gives them but wants it a little brighter.
They open Settings, go to Display, and nudge brightness. Everything else about
the picture stays exactly as the theme intended. Later they switch to a
different theme. The new theme's scanlines, bloom and color bleed all take
effect, and their brightness nudge is still there on top.

**Why this priority**: This is the promise the product already makes in its own
README and does not keep. Today that one nudge freezes every other value for
that monitor forever, so the user stops receiving theme changes and preset
improvements without ever being told. Everything else in this feature is in
service of making this true.

**Independent Test**: Change one value on the Display page, switch themes, and
confirm that the changed value survived and every unchanged value followed the
new theme. Fully testable on its own and delivers the headline value by itself.

**Acceptance Scenarios**:

1. **Given** a monitor showing theme defaults, **When** the user changes only
   brightness, **Then** brightness takes the user's value and contrast, gamma,
   persistence, scanlines, bloom and color bleed all still follow the preset and
   theme chain.
2. **Given** a user who has changed only brightness, **When** the active theme
   changes to one declaring different bloom settings, **Then** the new theme's
   bloom applies and the user's brightness is unchanged.
3. **Given** a user who has changed brightness, **When** a future release ships
   a different preset value for scanline intensity on that monitor, **Then** the
   user sees the new scanline intensity.
4. **Given** a theme that declares a value for a field the user has also
   changed, **When** the picture is drawn, **Then** the user's value wins for
   that field only, and the theme still wins for every field the user has not
   changed.

---

### User Story 2 - Two tubes, two sets of tweaks (Priority: P1)

Someone tunes the picture on the monitor sitting on their desk. Later they run a
different machine with a different monitor, tune that one too, and go back to
the first. Each monitor kept its own tuning. Neither one changed the other.

**Why this priority**: Equal first because it is the half of the design that
cannot be retrofitted. Storage shape is a migration; getting it wrong now means
a second migration over the same data once the catalog holds an Apple //e color
monitor, the IIgs pair, and later Commodore and Atari tubes. Two color tubes
sharing one settings block would mean tuning one silently changes the other.

**Independent Test**: Tune a value on one machine's monitor, switch to a machine
with a different monitor, confirm the second monitor shows its own defaults
rather than the first monitor's tuning, tune it differently, and switch back.

**Acceptance Scenarios**:

1. **Given** two machines that use different monitors, **When** the user changes
   bloom radius on the first, **Then** the second machine's bloom radius is
   unaffected.
2. **Given** a monitor with settings tuned in green, **When** the user switches
   that monitor to amber, **Then** amber shows its own defaults rather than the
   green tuning.
3. **Given** settings stored for a monitor, **When** a build is run that does
   not have that monitor in its catalog, **Then** those settings are preserved
   in the file rather than discarded.

---

### User Story 3 - The page says where a value came from (Priority: P2)

Someone opens the Display page and can see, for every row, whether that value is
coming from the monitor itself, from the theme they picked, or from an edit they
made. Any row they have edited can be put back with a single click, without
disturbing the rows they have not edited.

**Why this priority**: Second because the resolution rules are what the product
does and this is how the user perceives them. Without it, an override is visible
only as the absence of a badge, so a user who tweaked bloom months ago has no
way to understand why a new theme's bloom never appears. It also makes the
per-field model discoverable rather than invisible.

**Independent Test**: Edit some rows and not others, then read the page and
confirm each row's stated source matches where its value actually came from.
Reset a single row and confirm only that row changed.

**Acceptance Scenarios**:

1. **Given** a row whose value comes from the monitor's own defaults, **When**
   the user views the Display page, **Then** the row is labeled as showing a
   monitor default.
2. **Given** a row whose value comes from the active theme, **When** the user
   views the page, **Then** the row is labeled as showing a theme default.
3. **Given** a row the user has changed, **When** the user views the page,
   **Then** the row is labeled as custom.
4. **Given** three changed rows, **When** the user resets one of them, **Then**
   that row returns to the preset and theme chain and the other two keep the
   user's values.
5. **Given** any row, **When** the user resets it and its resolved value happens
   to equal what they had set, **Then** the row still reports itself as a
   default rather than as custom.

---

### User Story 4 - Existing settings survive the upgrade (Priority: P1)

Someone who has already tuned their picture installs a build with this change.
Their picture looks the same as it did before. Nothing they had set is lost.

**Why this priority**: Equal first because it is a correctness floor rather than
a feature. A settings change that silently alters or discards what someone
already chose is a defect regardless of how good the new model is. The settings
file is also shared between builds of different ages on the same machine, so the
upgrade must be safe in both directions.

**Independent Test**: Take a settings file written by the current release, load
it with the new build, and compare the resolved picture values field by field
against what the old build produced.

**Acceptance Scenarios**:

1. **Given** a settings file in which a monitor was marked as user-set, **When**
   the new build loads it, **Then** every value that monitor was displaying is
   still displayed.
2. **Given** a settings file in which no monitor was marked as user-set,
   **When** the new build loads it, **Then** no user overrides exist and every
   value follows the preset and theme chain.
3. **Given** a settings file already upgraded, **When** it is loaded again,
   **Then** the upgrade does not run a second time and nothing changes.
4. **Given** a settings file written by the new build, **When** an older build
   reads and rewrites it, **Then** the new build's stored overrides are still
   present afterwards.
5. **Given** a settings file that a person has hand-edited into an unusual
   shape, **When** it is loaded, **Then** the upgrade does not run over data it
   has already converted.

---

### Edge Cases

- A settings file names a monitor this build does not have. Its settings are
  kept and round-tripped rather than dropped, because the monitor may exist in
  another build sharing the same file.
- A person hand-edits the settings file and leaves an override section that is
  empty, or is present but not an object. Neither case may cause the upgrade to
  re-run over already-converted data.
- A monitor offers a color mode that makes no physical sense for it, such as an
  amber setting on a color tube. Storing settings for that combination is
  harmless, because they apply only when the user has explicitly selected it.
- A field has a value from the preset but the theme declares nothing for it, as
  is the case for gamma and persistence, which no theme can currently set. Those
  fields have only two possible sources rather than three.
- Restoring defaults on a monitor whose resolved values happen to equal the
  values the user had chosen. The row must report itself as a default, since the
  source is what changed, not the number.
- A monitor's stored settings are read while the machine is being switched. The
  values used to draw a frame must all come from the same monitor and mode, and
  never pair one monitor's overrides with another's defaults.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST store a user's CRT adjustments as individually
  optional values, so that adjusting one leaves every other following the
  preset and theme chain.
- **FR-002**: The system MUST store adjustments separately for each combination
  of monitor and color mode, so that the same phosphor on two different monitors
  holds two independent sets of adjustments.
- **FR-003**: The system MUST resolve a displayed value by consulting, in order,
  the monitor's preset, then the active theme where the theme declares a value,
  then the user's adjustment where one exists.
- **FR-004**: A user's adjustment to a field MUST take precedence over a theme's
  value for that field only, leaving the theme's values for all other fields in
  the same group in effect.
- **FR-005**: Changing the active theme MUST NOT discard any user adjustment.
- **FR-006**: Restoring defaults MUST remove the user's adjustments for the
  current monitor and color mode and MUST NOT record any new state to express
  that they were removed.
- **FR-007**: The Display page MUST show, for each adjustable row, whether the
  displayed value originates from the monitor, from the theme, or from the
  user's own adjustment.
- **FR-008**: Users MUST be able to reset a single row without affecting other
  rows.
- **FR-009**: The system MUST convert settings files written by earlier releases
  so that every value the user could previously see applied is still applied.
- **FR-010**: The conversion MUST run at most once for a given settings file and
  MUST NOT depend on a version number to decide whether it has already run.
- **FR-011**: The system MUST preserve stored adjustments belonging to monitors
  it does not recognize, rather than discarding them.
- **FR-012**: A settings file written by this version MUST survive being read
  and rewritten by an earlier version without losing its stored adjustments.
- **FR-013**: The rules that resolve preset, theme and user values into a final
  picture MUST be expressed in one place that automated tests can exercise
  directly, and MUST NOT be restated in the settings user interface.
- **FR-014**: A monitor identifier that has shipped MUST NOT be renamed, and the
  system MUST fail its own automated checks if one is.
- **FR-015**: Determining which stored adjustments apply MUST NOT re-read
  machine configuration from disk while drawing frames.
- **FR-016**: The documentation that describes how presets, themes and user
  adjustments combine MUST be corrected to match the delivered behavior.

### Key Entities

- **Monitor**: A physical display model the emulated machine sits in front of.
  Owns a durable identifier that is used to file the user's adjustments, and a
  default phosphor. Identifiers are permanent once shipped.
- **Color mode**: The phosphor or color treatment applied to the picture. Any
  mono monitor offers green, amber and white. Combined with a monitor, it
  identifies one set of user adjustments.
- **Preset**: The baseline picture values that belong to a monitor. Not
  user-editable and not stored in the user's settings.
- **Theme CRT defaults**: Optional picture values a theme declares, in groups,
  possibly refined per machine. A theme that declares nothing for a group leaves
  that group to the preset.
- **User adjustment set**: The sparse collection of picture values a person has
  deliberately changed for one monitor and color mode. Absent means no opinion.
- **Resolved picture values**: The final values used to draw, together with the
  origin of each one, so the interface can report where each value came from.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After adjusting one picture setting and switching themes, the
  adjusted setting is retained and all ten other settings follow the new theme.
  Verified for all eleven settings.
- **SC-002**: Adjusting a setting on one monitor produces no change to any
  stored or displayed value for any other monitor or color mode.
- **SC-003**: For every settings file written by the current release, the eleven
  resolved picture values after upgrade are identical to the eleven values the
  current release produced from the same file.
- **SC-004**: Loading and saving an upgraded settings file any number of times
  produces an unchanged file after the first save.
- **SC-005**: A settings file that has been round-tripped through an earlier
  release still yields the same resolved picture values in this release.
- **SC-006**: Every one of the eleven settings can be individually reset, and
  each reset changes exactly one row's reported source.
- **SC-007**: For each of the eleven settings, the source reported on the
  Display page matches the tier that actually supplied the value, across all
  combinations of theme-declares and user-adjusts.
- **SC-008**: The resolution rules are covered by automated tests that run
  without launching the application.
- **SC-009**: Drawing a frame performs no file reads or configuration parsing to
  determine which adjustments apply.
- **SC-010**: Renaming a shipped monitor identifier causes an automated test to
  fail.

## Assumptions

- The eleven adjustable picture values are the set currently exposed:
  brightness, contrast, gamma, persistence, scanlines enabled and intensity,
  bloom enabled, radius and strength, and color bleed enabled and width.
- Gamma and persistence have only two possible sources, the preset and the user,
  because no theme can currently declare them. The interface reports them
  accordingly rather than pretending a theme tier exists.
- All color modes remain selectable on every monitor. A monitor's own phosphor
  is the starting default rather than a restriction. Restricting the list to
  what a tube physically supports belongs to the later monitor work.
- The two monitors in the catalog at the time of the upgrade are the only
  monitors a person could have been looking at when they made an adjustment, so
  an adjustment from an older file applies to both of them.
- Preset values and theme-declared values are not retuned by this work. They are
  unverified in both directions and require reference hardware or photographs.
- The monitor picker interface, the split of monitor model from phosphor into
  two separate choices, and the mono versus color renderer change are all later
  work. This feature must not foreclose them, and specifically must not assume a
  fixed number of monitors.
- A separate fix, owned elsewhere, prevents a settings save from overwriting the
  global settings section with defaults. This feature depends on that fix
  landing, because an upgrade that is subsequently overwritten is worse than no
  upgrade.
- The settings file is shared between builds of different ages on one machine.
  This is a development condition rather than a shipped one, but it is the
  normal working condition here and the design treats it as a requirement.
