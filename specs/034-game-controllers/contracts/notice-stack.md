# Contract: Notice Stack

**Feature**: `034-game-controllers` | **Research**: [../research.md](../research.md) R20, R21 | **Data model**: [../data-model.md](../data-model.md#notice-stack-2026-09-27)

Replaces the single `DxuiTimedInfoBanner` the shell shows notices on, whose `Show` replaces the text, with a Dxui control that keeps every notice up for its full time (FR-044).

## Interface

`Dxui/Widgets/DxuiNoticeStack.h`

```cpp
class DxuiNoticeStack : public IDxuiControl
{
public:

    //  Appends a notice below those showing; it stays up for its own full
    //  duration from nowMs.
    void    Push                 (const std::wstring & text, int64_t nowMs);

    //  Takes every notice down now.
    void    Clear                ();

    bool    IsShowing            (int64_t nowMs) const;
    bool    IsAnimating          (int64_t nowMs) const;

    //  The next time something changes by itself: an expiry, or the end of a
    //  slide. Absent while nothing is showing.
    std::optional<int64_t>  GetNextChangeMs () const;

    size_t                  GetCount        () const;
    const std::wstring &    GetText         (size_t index) const;

    //  Where notice `index` sits at nowMs, relative to the top of the bounds.
    float   GetOffsetPx          (size_t index, int64_t nowMs) const;

    void    SetDurationMs        (int64_t durationMs);
    void    SetDpi               (UINT dpi);

    //  The caller passes DxuiSystemSettings::AreMenuAnimationsEnabled();
    //  the control never reads the singleton, so tests pin it.
    void    SetAnimationsEnabled (bool isEnabled);

    float   GetMeasuredHeightPx  (IDxuiTextRenderer   &  text,
                                  float                  widthPx,
                                  const DxuiDpiScaler &  scaler) const;

    void    Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void    Tick   (int64_t nowMs) override;
};
```

`Dxui/Core/DxuiSlide.h`

```cpp
struct DxuiSlide
{
    //  Starts moving `distancePx` toward zero at startMs, over the menus'
    //  open duration and easing; with animations off it is already done.
    static DxuiSlide  Start      (float distancePx, int64_t startMs, bool isAnimated);

    float             GetOffset  (int64_t nowMs) const;
    bool              IsDone     (int64_t nowMs) const;
};
```

Exact declarations may differ in the implementation; the behavior below is the contract.

## Behavior

| Rule | Detail |
|---|---|
| Order | Notices stack top to bottom in arrival order, the first at the top of the bounds |
| Duration | Each notice keeps its own full duration (`DxuiTimedInfoBanner::ResolveDefaultDurationMs()` unless set); a later arrival never shortens or restarts an earlier one |
| Expiry | At its expiry a notice is removed; every notice below it slides up by the removed notice's height plus the gap |
| Slide | Over `DxuiPopupMenu::kRevealMs` with `DxuiTweenEase::EaseOut`, the curve menus open with. A slide starting while another is running starts from the current offset. With animations off (the shell passes `DxuiSystemSettings::AreMenuAnimationsEnabled()` through `SetAnimationsEnabled`), notices move at once |
| Arrival | A new notice appears in place below the last, without a slide (FR-044 requires the slide on expiry only) |
| Clock | Every time is passed in; the control reads no clock, so tests expire and slide notices without waiting |
| Threading | UI thread only, like every Dxui control |
| Layout | The control fills the bounds it is given and places notices from their top edge; where the bounds sit is the caller's |
| Accessibility | Each notice keeps `DxuiTimedInfoBanner`'s label role and text |

## Shell side (`CassoEmuCore/Shell/EmulatorShellPresent.cpp`)

| Piece | Stays in the shell |
|---|---|
| `PostNotice` | Unchanged: the hand-off from other threads through `WM_APP_SHOW_NOTICE` |
| `ShowNotice` | Becomes `m_notices.Push (text, nowMs)` plus a redraw request; wording is still composed in core |
| `SyncNotice` | Computes the anchor (`ComputeTopOverlayEdgePx`), the width and the DPI, lays the stack out, and keeps frames coming while `IsAnimating` or until `GetNextChangeMs` |

Every existing caller of `ShowNotice` and `PostNotice` goes through the stack with no change at the call site.

## Unit-test obligations

`UnitTest/Dxui/DxuiNoticeStackTests.cpp` and `UnitTest/Dxui/DxuiSlideTests.cpp`:

- Two notices pushed 1 s apart each stay up for their own full duration.
- The second appears below the first; when the first expires the second slides up and ends at the top.
- The slide's offset at start, midway and end follows the ease-out curve over `kRevealMs`; with menu animations off the notice is at its end at once.
- A notice expiring mid-slide starts the next slide from the current offset.
- `GetNextChangeMs` reports the nearest expiry or slide end, and nothing when empty.
- Three notices, the middle one pushed with a shorter duration so it expires first: the top one stays and only the bottom one moves.
