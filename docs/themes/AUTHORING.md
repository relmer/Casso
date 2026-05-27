# Casso Theme Authoring Guide

> [!IMPORTANT]
> This guide is under reconstruction. Casso's UI shell was rebuilt on a
> native D3D11 + DirectWrite pipeline (spec
> [007-ui-overhaul](../../specs/007-ui-overhaul/)); the previous RmlUi-based
> theming surface (`.rml` / `.rcss` documents, `entryDocuments` map, RCSS
> custom properties) is gone. The new token-based theming surface is
> still being wired through the native widgets.

## What works today

Themes are still discovered at `Themes/<Name>/theme.json` next to
`Casso.exe`. `theme.json` is parsed and the following fields are
preserved by `ThemeLoader`:

- `$cassoThemeVersion` (required, currently `1`)
- `$cassoBuiltIn` (reserved for built-in themes; never set this on a user
  theme — `AssetBootstrap` only overwrites directories marked built-in)
- `name`, `familyId`, `variantId`, `author`, `description`
- `useMicaBackdrop`
- `crtDefaults` — preferred CRT post-processing presets
- `uiTokens` — opaque JSON blob reserved for the native renderer
- `driveVisualProfile` — drive visual treatment selector

The authoritative schema lives at
[`specs/007-ui-overhaul/contracts/theme-metadata.schema.json`](../../specs/007-ui-overhaul/contracts/theme-metadata.schema.json).

The three built-in themes — `Skeuomorphic`, `DarkModern`, `RetroTerminal`
— under `Resources/Themes/` are the working reference.

## What is in flight

The native renderer does not yet consume `uiTokens` end-to-end. Once it
does (P5 of 007), this guide will document the supported token shape, how
hot-swap works, and how to ship a user theme.

Until then: copy one of the built-in themes, edit `theme.json` metadata,
and verify it appears in the theme picker once that lands.
