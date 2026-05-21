# Casso Theme Authoring Guide

Casso's UI shell (spec [007-ui-overhaul](../../specs/007-ui-overhaul/)) is built on
[RmlUi](https://github.com/mikke89/RmlUi) — an HTML/CSS-style document model rendered
straight onto the same D3D11 framebuffer as the emulator. **Themes** are first-class:
the three built-in themes (Skeuomorphic, Dark Modern, Retro Terminal) are exactly the
same shape as a theme you author yourself.

This guide walks through everything you need to ship a custom theme.

---

## 1. Directory layout

Themes live under `Themes/<YourTheme>/` next to `Casso.exe`. The directory **must**
contain a `theme.json` metadata file. Everything else is referenced from there.

A complete custom theme looks like:

```
Themes/
  MyTheme/
    theme.json              # metadata + entry-document map
    title_bar.rml           # custom title bar (overrides _shared/title_bar.rml)
    title_bar.rcss          # styles for the above
    nav_layer.rml           # custom menu bar / nav strip
    nav_layer.rcss
    settings.rml            # custom Settings panel
    settings.rcss
    drive_widgets.rml       # custom drive widgets
    drive_widgets.rcss
    theme.rcss              # global RCSS variables (color tokens, fonts)
    fonts/
      MyFont-Regular.ttf
      OFL.txt               # font license
```

The four entry documents (`titleBar`, `navLayer`, `settings`, `driveWidgets`) are the
RML documents Casso loads at runtime. **Any of them may be omitted** — if `theme.json`
doesn't list an entry, Casso falls back to `Themes/_shared/<entry>.rml` so you only
need to override the parts you actually want to restyle.

> [!TIP]
> Start by copying one of the built-in themes from `Themes/Skeuomorphic/`,
> `Themes/DarkModern/`, or `Themes/RetroTerminal/`. Rename the directory and the
> `name` field in `theme.json`, then edit incrementally.

---

## 2. `theme.json` schema

The authoritative schema lives at
[`specs/007-ui-overhaul/contracts/theme-metadata.schema.json`](../../specs/007-ui-overhaul/contracts/theme-metadata.schema.json).
Highlights:

| Field                | Type             | Required | Notes                                                          |
|----------------------|------------------|----------|----------------------------------------------------------------|
| `$cassoThemeVersion` | integer          | yes      | Currently `1`. Bumps when the entry-document contract changes. |
| `name`               | string           | yes      | 1–64 chars. Shown in the Settings → Theme dropdown.            |
| `author`             | string           | no       | Free text. Surfaced in the About panel.                        |
| `description`        | string           | no       | Tooltip / About blurb.                                         |
| `entryDocuments`     | object           | no       | Per-element `.rml` overrides (see § 1).                        |
| `crtDefaults`        | object           | no       | Theme's preferred CRT effect presets. See § 4.                 |
| `useMicaBackdrop`    | boolean          | no       | Request the Win11 Mica backdrop. Falls back to opaque on Win10. |

> [!IMPORTANT]
> **Never set `$cassoBuiltIn: true` on a user theme.** That marker is reserved for the
> three themes Casso ships, and is the signal `AssetBootstrap` uses to know which
> directories it's allowed to overwrite on upgrade. A user theme without the marker
> is **never** touched by Casso — your edits are safe across version bumps.

A minimal valid theme.json:

```json
{
    "$cassoThemeVersion": 1,
    "name":               "MyTheme",
    "author":             "Your Name",
    "description":        "A custom Casso theme.",
    "entryDocuments": {
        "titleBar":     "title_bar.rml",
        "navLayer":     "nav_layer.rml",
        "settings":     "settings.rml",
        "driveWidgets": "drive_widgets.rml"
    },
    "useMicaBackdrop": false
}
```

---

## 3. Inheriting from `Themes/_shared/`

Casso's built-in themes share an `_shared/` directory containing reference RML
documents for each entry point. When a theme omits an entry from
`entryDocuments`, the loader transparently uses the `_shared/` version.

This means you can:

- Ship a theme that **only** restyles the title bar (and let the rest of the UI
  inherit the shared layout).
- Ship a theme that restyles **everything** by overriding all four entries.
- Anything in between.

Restyling without copying is preferred — it keeps your theme tiny and means future
Casso updates to shared documents (new widgets, accessibility fixes) flow through
without you having to update your theme.

---

## 4. RCSS variables and color tokens

The built-in `_shared/` documents are written against a small set of named RCSS
variables (RmlUi's equivalent of CSS custom properties). Override them in your
`theme.rcss` to recolor the entire UI without touching layout:

| Variable               | Used by                                       |
|------------------------|-----------------------------------------------|
| `--color-bg`           | Title bar + nav layer background              |
| `--color-bg-elevated`  | Settings panel, popups                        |
| `--color-fg`           | Default text                                  |
| `--color-fg-muted`     | Disabled text, secondary labels               |
| `--color-accent`       | Active-tab underline, focus rings, LED on     |
| `--color-led-on`       | Drive activity LEDs (active)                  |
| `--color-led-off`      | Drive activity LEDs (idle)                    |
| `--color-border`       | Panel + control borders                       |
| `--font-ui`            | UI font family                                |
| `--font-mono`          | Numeric / monospaced fields (machine info)    |

Example `theme.rcss`:

```rcss
:root
{
    --color-bg:          #1a1a1f;
    --color-bg-elevated: #25252d;
    --color-fg:          #e8e8ec;
    --color-fg-muted:    #8b8b95;
    --color-accent:      #4d9eff;
    --color-led-on:      #4dff8e;
    --color-led-off:     #2a2a30;
    --color-border:      #3a3a44;
    --font-ui:           "Inter";
    --font-mono:         "JetBrains Mono", monospace;
}
```

---

## 5. CRT defaults

Themes can declare their preferred CRT post-processing presets in `crtDefaults`.
On first activation of a theme, Casso applies these presets — but only if the user
hasn't already customised the corresponding controls in `Settings → CRT`. Once the
user has touched a knob, their override sticks across theme switches.

```json
"crtDefaults": {
    "brightness": 1.10,
    "scanlines":  { "enabled": true,  "intensity": 0.35 },
    "bloom":      { "enabled": true,  "radius": 1.5, "strength": 0.6 },
    "colorBleed": { "enabled": true,  "width": 1.0 }
}
```

This lets a "Retro Terminal" theme ship with strong scanlines + bloom enabled by
default, while a "Dark Modern" theme starts with the bare minimum.

---

## 6. Testing your theme

1. Build Casso (`scripts\Build.ps1`) — or use a release `Casso.exe`.
2. Copy your theme directory to `Themes/<YourTheme>/` next to `Casso.exe`.
3. Launch Casso.
4. Open **Settings** (Ctrl+,).
5. **Theme** dropdown should list your theme by `name`. Pick it.
6. The UI hot-swaps. No restart, no machine reset.

Iterate by editing your `.rml` / `.rcss` files and clicking your theme in the
dropdown again — Casso re-parses and re-renders the documents in place.

If your theme fails to load, Casso falls back to the previously active theme and
logs the parse error to the debug console (View → Debug Console, or `Ctrl+D`).
Common causes:

- `theme.json` doesn't validate against the schema → check the field names + types.
- An `.rml` file references an asset (image, font) that isn't in the theme directory.
- A misspelled element id in your `settings.rml` — the C++ side looks up specific
  ids (`#machine-selector`, `#emulation-speed`, `#crt-brightness`, …) listed in
  [`Casso/Ui/SettingsPanel.h`](../../Casso/Ui/SettingsPanel.h).

---

## 7. Sharing your theme

Themes are pure asset directories — no compilation, no rebuild. To ship one:

1. Zip up `Themes/MyTheme/`.
2. Make sure your zip includes the license file for any third-party font you bundle.
3. Drop the unzipped directory next to a friend's `Casso.exe`.

Pull requests adding new built-in themes are welcome — they go under
`Resources/Themes/<Name>/` and need a `$cassoBuiltIn: true` marker plus an entry
in `Casso/Casso.rc` + `Casso/AssetBootstrap.cpp`. See the three existing built-in
themes for the recipe.
