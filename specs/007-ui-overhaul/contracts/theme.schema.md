# Contract: Theme JSON

**Location**: `<assetBaseDir>/Themes/<Name>/<Name>.json` plus sibling assets.
**Version field**: `$cassoThemeVersion` (integer).
**Owner**: `ThemeManager` + `ThemeLoader` (Casso/Theme/).

## Purpose

Define all visual parameters of the custom D3D chrome: colors, textures,
geometry, animation timing, LED glow, and CRT effect defaults (FR-029, FR-032,
FR-040). Hot-swappable at runtime (FR-033). User-authored themes drop into
`Themes/` alongside built-ins (FR-035).

## Schema (v1)

```json
{
    "$cassoThemeVersion": 1,
    "name": "Skeuomorphic",
    "description": "Beige/cream palette mimicking physical Apple II hardware.",
    "palette": {
        "chromeBg":           "#D8C9A3",
        "chromeBgAccent":     "#B8A47A",
        "titleBarText":       "#2A2018",
        "ledIdleCenter":      "#4A1F1F",
        "ledIdleGlow":        "#2A0F0F",
        "ledActiveCenter":    "#FF3030",
        "ledActiveGlow":      "#FF8040",
        "focusRing":          "#1E78D8"
    },
    "textures": {
        "driveFace":          "drive_face.png",
        "ejectButton":        "eject.png",
        "titleBarBackground": "titlebar.png",
        "navItemHover":       "nav_hover.png"
    },
    "geometry": {
        "titleBarHeightPx":   32,
        "driveWidget":  { "widthPx": 220, "heightPx": 96 },
        "ejectRect":    { "xPx": 180, "yPx": 60, "widthPx": 30, "heightPx": 16 },
        "ledRadiusPx":  6,
        "navItemPaddingPx": 12
    },
    "animation": {
        "doorOpenMs":  220,
        "doorCloseMs": 280,
        "spinRpm":     300,
        "ledFadeMs":   140
    },
    "ledGlow": {
        "innerRadiusPx": 5,
        "outerRadiusPx": 18,
        "falloffExp":    1.6
    },
    "crtDefaults": {
        "brightness":        1.0,
        "scanlinesEnabled":  false,
        "scanlineIntensity": 0.35,
        "bloomEnabled":      false,
        "bloomRadius":       2.0,
        "bloomStrength":     0.8,
        "bleedEnabled":      false,
        "bleedWidth":        1.0
    }
}
```

All fields are required for v1 except `description`. Missing required fields →
theme excluded from list with warning (FR-036).

## Built-in theme defaults

| Theme | Notable defaults |
|-------|------------------|
| Skeuomorphic | `crtDefaults`: all CRT FX off, brightness 1.0 |
| Dark Modern | `crtDefaults`: all CRT FX off, brightness 1.0; neon LED palette |
| Retro Terminal | `crtDefaults`: scanlines + bloom + bleed all on, brightness 1.05; phosphor-green palette |

## Validation

| Condition | Action |
|-----------|--------|
| File malformed | excluded from list, warning logged, app continues (FR-036) |
| Texture file referenced but missing on disk | excluded from list, warning logged |
| Unknown palette/geometry token referenced by chrome | falls back to compiled-in default, warning logged |
| Active theme deleted or made invalid while running | revert to previous theme; if no previous, fall back to "Skeuomorphic" |
| Two themes share a name | first one loaded wins, second logged as duplicate |

## Upgrade

`ThemeUpgrade::Plan` is invoked when:
- a theme file exists on disk, AND
- its `$cassoThemeVersion` < current embedded version (for built-ins) or
  current schema version (for user themes).

For user themes the migration is best-effort: fill in any new required fields
from compiled defaults, log unknown old fields.

For built-in themes whose contents are unchanged from the embedded default
(SHA-256 match against `ThemeUpgradePriorHashes` table), silently overwrite
(`OverwriteSilent`). For user-modified built-ins (hash mismatch), back up to
`<Name>.json.bak` then write the new default (`BackupAndReplace`).

## Hot-swap protocol

`ThemeManager::SetActiveTheme(name)` must:
1. Parse the new theme's JSON.
2. Upload all referenced textures to GPU memory (new `ThemeAssetCache` entries).
3. Atomically swap the active `ThemeData` pointer used by `D3DUiContext`.
4. Release the previous theme's GPU textures.

The swap must complete within one rendered frame (SC-002). If any step fails,
abort the swap and keep the previous theme (FR-036).

## Test scenarios

- Parse all three built-in themes → ThemeData round-trip equality.
- Malformed JSON → excluded, no crash.
- Missing texture → excluded, no crash.
- Hot-swap from theme A to theme B → active pointer changes; previous textures
  released (verified via mock asset cache).
- User drops new theme into Themes/ while running → appears on next panel open.
