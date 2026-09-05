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
  theme, `AssetBootstrap` only overwrites directories marked built-in)
- `name`, `familyId`, `variantId`, `author`, `description`
- `useMicaBackdrop`
- `crtDefaults`: preferred CRT post-processing presets
- `uiTokens`: opaque JSON blob reserved for the native renderer
- `driveVisualProfile`: drive visual treatment selector

The authoritative schema lives at
[`specs/007-ui-overhaul/contracts/theme-metadata.schema.json`](../../specs/007-ui-overhaul/contracts/theme-metadata.schema.json).

The three built-in themes (`Skeuomorphic`, `DarkModern`, `RetroTerminal`
) under `Resources/Themes/` are the working reference.

## How `crtDefaults` layers

A theme's `crtDefaults` sit between the monitor's own preset and whatever
the user has adjusted. Four rules follow from that.

**A group is all or nothing.** Declaring `bloom` supplies `enabled`,
`radius` and `strength` together. There is no way to set a radius and
leave the strength to the preset.

**Omitting a group leaves the preset alone.** A theme that declares
nothing for scanlines gets the monitor's scanlines, not zeros. So
declaring a group is how a theme turns an effect off, and omitting it is
how a theme defers. `DarkModern` uses both: its `Apple //c` variant
declares `scanlines` with `enabled: false` to switch them off on that
machine, while its base omits `contrast` entirely and takes whatever
contrast the monitor brings.

**A user's adjustment wins per field.** Someone who changes bloom
strength keeps their strength and still gets the theme's radius. Picking
a theme discards nothing they have adjusted, and adjusting one control
discards nothing the theme declared.

**`gamma` and `persistence` cannot be declared.** They resolve from the
monitor preset or from the user, never from a theme.

`variantOverrides`, keyed by machine display name, refine the base one
field at a time rather than replacing a group wholesale.

## What is in flight

The native renderer does not yet consume `uiTokens` end-to-end. Once it
does (P5 of 007), this guide will document the supported token shape, how
hot-swap works, and how to ship a user theme.

Until then: copy one of the built-in themes, edit `theme.json` metadata,
and verify it appears in the theme picker once that lands.
