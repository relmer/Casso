# Quickstart — Working with Dxui

This document is the "you've just landed on the branch, what do you do" guide for the three most common tasks during the migration.

Prerequisites: PowerShell 7 (`pwsh`), Visual Studio 2026 with the v145 toolset, the branch `013-dxui-framework-extraction` checked out.

## 1. Build Dxui standalone and verify the solution still links

```powershell
# Build the Dxui static library by itself
scripts\Build.ps1 -Configuration Debug -Platform x64 -Project Dxui

# Build the whole solution (Casso.exe + UnitTest.dll link Dxui; CassoCli.exe does not)
scripts\Build.ps1 -Configuration Debug -Platform x64
scripts\Build.ps1 -Configuration Release -Platform ARM64

# Run all tests
scripts\RunTests.ps1

# Code analysis (must pass clean before commit)
scripts\Build.ps1 -RunCodeAnalysis
```

Expected at every phase gate: zero build errors, zero warnings, zero analysis findings, zero failing tests.

## 2. Write a Dxui widget

A widget is an `IDxuiControl` (typically deriving from `DxuiPanel` if it has children, or directly from a leaf base if not). It paints against `IDxuiPainter` / `IDxuiTextRenderer` / `IDxuiTheme` — never against the concretes.

```cpp
// Dxui/Widgets/DxuiMyWidget.h
#pragma once

#include "Pch.h"
#include "IDxuiControl.h"
#include "IDxuiPainter.h"
#include "IDxuiTextRenderer.h"
#include "IDxuiTheme.h"


class DxuiMyWidget : public IDxuiControl
{
public:
    DxuiMyWidget   ();
    void SetLabel  (const std::wstring & label);

    // IDxuiControl
    void Paint     (IDxuiPainter & painter,
                    IDxuiTextRenderer & text,
                    const IDxuiTheme & theme) override;
    bool OnMouse   (const DxuiMouseEvent & ev) override;
    bool OnKey     (const DxuiKeyEvent & ev) override;

private:
    std::wstring   m_label;
    bool           m_hover    = false;
};
```

```cpp
// Dxui/Widgets/DxuiMyWidget.cpp
#include "Pch.h"

#include "DxuiMyWidget.h"


static constexpr float s_kPadDip = 6.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMyWidget::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMyWidget::Paint (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme)
{
    HRESULT      hr        = S_OK;
    const RECT   rectDip   = Bounds();
    const UINT32 fillColor = m_hover ? theme.HoverBackground() : theme.Background();



    hr = painter.FillRect (rectDip, fillColor);
    CHRA (hr);

    hr = text.DrawText (rectDip, m_label, theme.Foreground(), theme.BodyFont());
    CHRA (hr);

Error:
    return;
}
```

Note the EHM pattern in a `void` function (vestigial `HRESULT hr`, `Error:` label with explicit `return;`). The 5-blank-line top-level separator, 3-blank-line var-block separator, 80-slash comment header, function-call spacing (`painter.FillRect (rectDip, fillColor)`), and Hungarian static (`s_kPadDip`) are all required.

## 3. Write a headless widget unit test

```cpp
// UnitTest/Dxui/DxuiMyWidgetTests.cpp
#include "Pch.h"

#include "CppUnitTest.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "DxuiMyWidget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace DxuiTests
{
    TEST_CLASS(DxuiMyWidgetTests)
    {
    public:
        TEST_METHOD(Paint_DrawsBackgroundThenLabel)
        {
            DxuiMyWidget          widget;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            widget.SetBounds ({0, 0, 100, 30});
            widget.SetLabel  (L"Hello");
            widget.Paint     (painter, text, theme);

            const auto & calls = painter.Calls();
            Assert::AreEqual ((size_t) 1, calls.size());
            Assert::IsTrue   (calls[0].kind == RecordedPaintCall::Kind::FillRect);

            const auto & textCalls = text.Calls();
            Assert::AreEqual ((size_t) 1, textCalls.size());
            Assert::AreEqual (std::wstring (L"Hello"), textCalls[0].text);
        }
    };
}
```

No D3D11 device. No HWND. The widget logic — "fill background, then draw label" — is verified at the interface boundary. This is the SC-007 model.

## 4. The migration ritual (per phase)

For each of the 11 migration phases listed in `plan.md`:

1. Branch off the previous phase's commit on `013-dxui-framework-extraction`.
2. Make the phase's file moves / creates / deletes per the plan.
3. `scripts\Build.ps1 -Configuration Debug -Platform x64` — fix any errors.
4. `scripts\Build.ps1 -Configuration Release -Platform ARM64` — DPI / ARM64-specific stuff sometimes bites here first.
5. `scripts\RunTests.ps1` — must pass.
6. `scripts\Build.ps1 -RunCodeAnalysis` — must pass clean.
7. For phases that move existing functionality (2, 3, 4, 7, 11), verify the user-visible behaviour manually — open the app, exercise the affected surface, compare side-by-side with the previous build if visual.
8. Commit with `<type>(dxui): <description>` Conventional Commits format (scope = `dxui` for framework work, `casso/ui` for consumer migration). Include `Co-authored-by: Copilot <…>` trailer if the AI authored material.
9. Merge to master with `--no-ff` (never squash).
10. Move to the next phase.



## 4. Dialog completion without blocking the UI

`DxuiDialogManager::Show` returns `std::future<int>`, but the supported continuation model is to post back to the UI thread. Do not park a worker thread on `future.wait()` and then touch Dxui from that worker. Yes, futures are tempting; no, the UI thread does not enjoy hostage situations.

### Right: post the result back to the owner window

```cpp
constexpr UINT WM_APP_DIALOG_DONE = WM_APP + 42;

void StartDialog (DxuiDialogManager & dialogs, HWND hwnd, int dialogId)
{
    std::future<int> result = dialogs.Show (std::move (dialog), ShowParams {});

    QueueUserWorkItem ([](void * ctx) -> DWORD
    {
        DialogWaitContext * context = static_cast<DialogWaitContext *> (ctx);
        const int returnCode = context->result.get();
        PostMessage (context->hwnd, WM_APP_DIALOG_DONE, context->dialogId, returnCode);
        return 0;
    }, context, WT_EXECUTEDEFAULT);
}

// In the UI-thread WndProc:
// case WM_APP_DIALOG_DONE:
//     HandleDialogResult ((int) wp, (int) lp);
//     return 0;
```

### Wrong: block and touch Dxui from the worker

```cpp
std::future<int> result = dialogs.Show (std::move (dialog), ShowParams {});

std::thread ([&]()
{
    result.wait();
    dialogs.Show (std::move (anotherDialog), ShowParams {}); // Wrong thread.
}).detach();
```

## Showing a dialog (2026-07 API)

```cpp
// Modal message box / picker: pumps its own loop until a button ends the dialog.
auto dlg = std::make_unique<MyDialogWindow> (/* ... */);   // derives DxuiDialogWindow
int result = dlg->ShowModalDialog (IDOK);                  // returns the chosen command id
if (result == IDOK) { /* commit */ }

// Modeless live-preview sheet: does NOT block; input still reaches the owner window,
// so a settings sheet can preview changes on the emulator behind it.
auto sheet = std::make_unique<MySettingsSheet> (/* ... */);  // derives DxuiPropertySheet
sheet->AddPage (&displayPage, L"Display");
sheet->AddPage (&machinePage, L"Machine");
sheet->ShowModelessDialog (IDOK);                          // OK / Cancel / Apply drive commit + dirty tracking
```

> The old `DxuiDialogManager::Show (std::move (dialog), ShowParams {})` returning a
> `std::future<int>` is **gone** (FR-126). The "Wrong: block and touch Dxui from the worker"
> example above is retained only to illustrate the thread-marshalling rule — the `dialogs.Show`
> API it shows no longer exists.

## Common pitfalls

- **Forgot `#include "Dxui.h"` in `Casso/Pch.h`**: every Casso `.cpp` will fail to find `IDxuiControl` etc. Fix once in `Pch.h`; do not add to individual `.cpp` files.
- **Angle-bracket include leaked into a Dxui public header**: code-analysis will not catch this; `rg -n '#include\s*<' Dxui/` should return matches only in `Dxui/Pch.h` and `Dxui/Dxui.h`.
- **Touched Dxui from a worker thread**: the UI-thread assertion will fire in debug builds. Marshal back via `PostMessage`.
- **Created a popup or dialog from a constructor**: the message pump isn't running yet. Defer to a `Tick` callback or a post-construction `OnShow` event.
- **`Dp` suffix in new Dxui code**: use `Dip`. The old `Dp` identifiers migrate opportunistically as files move (FR-082); don't introduce new ones.
