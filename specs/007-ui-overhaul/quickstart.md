# Quickstart — UI Overhaul (Developer)

This guide is for engineers implementing feature `007-ui-overhaul`.

## Prerequisites

- Visual Studio 2026 (v145), x64 + ARM64 workloads.
- Windows 11 for runtime validation.
- PowerShell for scripts.

## First-time setup

```pwsh
scripts/Build.ps1 -Configuration Debug   -Platform x64
scripts/Build.ps1 -Configuration Release -Platform x64
scripts/Build.ps1 -Configuration Debug   -Platform ARM64
scripts/Build.ps1 -Configuration Release -Platform ARM64
scripts/RunTests.ps1
```

## Iteration loop

```pwsh
scripts/Build.ps1
scripts/RunTests.ps1
.\x64\Debug\Casso.exe
```

## Theme iteration

Themes hot-reload at runtime.

1. Edit `Themes/<active>/theme.json` or assets under `images/` and `sounds/`.
2. Use Settings -> Theme -> Reload (dev command), or switch theme away/back.
3. Changes should appear immediately without restart.

## Visual verification checklist (007 reset)

1. Title/nav text matches Windows 11 system UI font.
2. Drag/min/max/close behavior matches expected native behavior.
3. Drive icons read as Disk ][ for Apple II-family variants.
4. Insert/eject triggers visible door close/open animation.
5. Door animation and drive sounds remain synchronized.
6. Variant switching works for Apple II, II+, IIe, and //c.
