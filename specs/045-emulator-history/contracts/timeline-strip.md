# Contract: The Timeline Strip, Marks and Fork Flyout

**Requirements**: FR-003 to FR-005, FR-010 to FR-015, FR-028 | **Research**: R-009

The rewind bar and the debugger timeline are both a `DxuiImageStrip` over a `HistoryThumbnails` source. This contract lists what the strip and its source gain.

## `IDxuiImageStripSource` additions

```cpp
//  A mark along the strip.
enum class DxuiStripMarkKind
{
    Gap,
    Reset,
    PowerCycle,
    DiskChange,
    WriteProtect,
    StateLoad,
    Fork,
};

struct DxuiStripMark
{
    float              offset = 0.0f;     // in cells from the leading edge
    DxuiStripMarkKind  kind   = DxuiStripMarkKind::Reset;
    int                count  = 1;        // branches at a fork mark
    uint64_t           id     = 0;        // the source's key for it
    std::wstring       tip;
};

//  One row of a fork mark's flyout.
struct DxuiStripForkRow
{
    uint64_t           id      = 0;       // the branch, as the source keys it
    Image              picture;           // its last frame; null until drawn
    std::wstring       ranFor;            // how long it ran past the fork
    std::wstring       leftAt;            // when the user left it
};

virtual void  GetMarks         (std::vector<DxuiStripMark> & outMarks)                              { outMarks.clear(); }
virtual void  GetForkRows      (uint64_t markId, std::vector<DxuiStripForkRow> & outRows)           { (void) markId; outRows.clear(); }
virtual void  OnForkRowHovered (uint64_t rowId)                                                    { (void) rowId; }
virtual void  OnForkRowClicked (uint64_t rowId)                                                    { (void) rowId; }
```

All on the UI thread, as the existing members are. A source with no marks uses the defaults.

## `DxuiImageStrip` behavior

| Event | Result |
|---|---|
| Paint | Each mark is drawn at its offset across the pictures, with a glyph per kind from `UnicodeSymbols.h`; a fork mark with a count above 1 has the count beside it |
| Pointer over a mark | The mark's tip; a fork mark opens the flyout instead, after the tooltip delay |
| Pointer over a flyout row | `OnForkRowHovered`; the host draws that picture over the viewport |
| Click on a flyout row | `OnForkRowClicked`; the flyout closes |
| Pointer moves off the flyout and the mark | The flyout closes; `OnForkRowHovered (0)` |
| Marks closer than their glyph width | Drawn as one, its tip listing each |

The flyout is a popup from the strip's popup host, as the preview is, and takes neither focus nor the pointer's capture.

## `HistoryThumbnails` as the source

| Member | Behavior |
|---|---|
| `GetMarks` | Built on the machine thread in `Service` from the journal records, keyframe gaps and segment tree on the active path, and published under the lock with the cells |
| `GetForkRows` | The branches that begin at the mark's fork position; each row's picture is the branch's live-end keyframe, drawn by the worker as a preview is |
| `OnForkRowClicked` | Calls the switch function the host set, with the branch's leaf segment |
| `KeyframeDrop::PathChanged` | `ForgetPictures` from the fork position, and the marks rebuilt |

## The emulator window's rewind bar

| Event | Result |
|---|---|
| Pointer enters the bottom 8 DIPs of the viewport, scaled for the monitor's DPI | The bar opens over the bottom of the viewport within 150 ms (SC-008) |
| Pointer moves off the bar | The bar closes |
| Pointer captured (paddle mode), or history off | The bar does not open |
| Click or drag on the bar | The strip's, never the guest mouse's |
| Drag along the strip | A silent seek to the point under the pointer, as the debugger timeline's scrub does (`HistoryTimelineScrub`) |

## Tests

- `DxuiImageStrip`: marks at offsets paint and hit-test; a fork mark with a count; merged marks; the flyout opens, routes a hover and a click, and closes.
- `HistoryThumbnails`: marks from a synthetic journal and segment tree; fork rows for a fork position; pictures dropped after `PathChanged`.
- The rewind bar's opening rule as a pure function of the pointer, the viewport, the DPI and the capture state.
