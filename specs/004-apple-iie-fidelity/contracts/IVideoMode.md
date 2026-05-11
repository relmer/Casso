# Contract: IVideoMode

Composable video mode. Each mode renders into a shared framebuffer,
optionally over a row range (for MIXED + 80COL composition per FR-017a).

## Header (per-mode shape)

```cpp
class IVideoMode {
public:
    virtual          ~IVideoMode () = default;
    virtual HRESULT   RenderFullScreen (Framebuffer & fb) = 0;
    virtual HRESULT   RenderRowRange   (int firstRow, int lastRow, Framebuffer & fb) = 0;
};
```

`Apple80ColTextMode::RenderRowRange` is the routine used by mixed-mode
bottom-4-rows when 80COL=1, satisfying FR-017a's "same composed path"
requirement.

## Composition rules (in `VideoOutput`)

- Full-screen TEXT, 80COL=0 → `AppleTextMode::RenderFullScreen`.
- Full-screen TEXT, 80COL=1 → `Apple80ColTextMode::RenderFullScreen`.
- Full-screen HIRES, DHR=0 → `AppleHiResMode::RenderFullScreen` (NTSC artifact on).
- Full-screen HIRES + 80COL + DHR-enabling switches → `AppleDoubleHiResMode`.
- MIXED:
  - Top rows 0..19 → graphics mode per HIRES/LORES/DHR.
  - Bottom rows 20..23 → 80COL=1: `Apple80ColTextMode::RenderRowRange(20,24,...)`;
    80COL=0: `AppleTextMode::RenderRowRange(20,24,...)`.

## Test obligations

`VideoModeTests`:
- 80-col aux/main interleave correctness with a known memory pattern.
- ALTCHARSET selects MouseText glyphs.
- Flash semantics: only when ALTCHARSET selects flashing set.
- DHR aux/main interleave with a known memory pattern.

`VideoRenderTests`:
- HiRes NTSC artifact framebuffer matches recorded golden hash.
- DHR test pattern matches recorded golden hash.
- MIXED + 80COL bottom-rows render via the 80-col path (verified by
  comparing the bottom-4-rows region of the framebuffer against the
  same region rendered by `Apple80ColTextMode::RenderFullScreen`).
