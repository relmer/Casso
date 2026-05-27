# Window Architecture (Native UI Reset)

## Overview

Casso's main window is a borderless custom window class that owns one D3D11
swap chain. Native-rendered chrome (title/nav/settings/drive widgets) and the
emulated viewport are composed in the same frame pipeline.

## Core principles

1. Single swap chain and one render ownership path.
2. Native-rendered chrome and settings surfaces.
3. Win32-based input/hit-testing for non-client behavior.
4. Theme-driven visuals through JSON metadata and assets.
5. Deterministic drive animation/sound synchronization via shared events.

## Frame model

1. Emulation produces video frame.
2. Renderer draws viewport + CRT post-process.
3. Renderer draws chrome/nav/settings/drive widgets using current theme tokens.
4. Present.

## Input model

1. Window messages are translated into native UI events.
2. Chrome/nav hit-testing routes commands through one command map.
3. Unhandled input is forwarded to emulation input layer.

## Drive synchronization model

Drive actions (open/close/spin) publish `DriveSyncEvent` records consumed by
both animation and audio layers. Both consumers must remain within one frame of
the same event id.
