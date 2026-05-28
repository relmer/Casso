# Contract: Reusable Themed Dialog Primitive

**Location**: `Casso/Ui/Dialog/DialogPrimitive.{h,cpp}` and
`Casso/Ui/Dialog/DialogDefinition.h`
**Modeled on**: `Casso/Ui/Settings/SettingsWindow.cpp`
**Consumers (P1+P2)**: `StartupDownloadDialog`, `BootDiskPicker`,
About / Keymap / Machine Info commands in `WindowCommandManager`,
the stray `MessageBoxW` in `SettingsPanel.cpp`.

## DialogDefinition

```cpp
enum class DialogIcon
{
    None,
    AppPhotoreal,    // IDI_CASSO_PHOTOREAL
    AppFlat,         // IDI_CASSO_FLAT  (etc., as needed)
    Info,
    Warning,
    Error,
};

struct DialogTextRun
{
    std::wstring  text;
    bool          isHyperlink = false;
    std::wstring  hyperlinkUrl;          // ignored when isHyperlink == false
};

struct DialogButton
{
    std::wstring  label;
    int           resultCode = 0;        // returned by DialogPrimitive::Show
    bool          isDefault  = false;    // Enter activates
    bool          isCancel   = false;    // Escape activates
};

struct DialogDefinition
{
    std::wstring                 title;
    DialogIcon                   icon          = DialogIcon::None;
    std::vector<DialogTextRun>   body;         // wraps within the body content area
    std::vector<DialogButton>    buttons;      // rendered left-to-right, right-aligned

    // Optional caller-supplied paint hook for custom body content (used by
    // StartupDownloadDialog progress UI and BootDiskPicker list). When set,
    // `body` is rendered ABOVE the custom area, buttons BELOW it.
    std::function<void (DialogPaintContext &)>  onPaintCustomBody;
    std::function<void (const DialogInputEvent &)> onInputCustomBody;
};
```

## DialogPrimitive (the modal surface)

```cpp
class DialogPrimitive
{
public:
    // Blocks until a button is clicked, Enter / Escape, or programmatic Close.
    // Returns the resultCode of the activated button, or -1 if cancelled
    // via window-close gesture with no isCancel button defined.
    static int Show (
        HWND                       ownerHwnd,
        const ChromeTheme        & theme,
        DxUiPainter              & painter,
        const DialogDefinition   & definition);

    // Programmatic close (used by callers that drive their own progress and
    // want to dismiss the dialog when work completes).
    void Close (int resultCode);
};
```

## Behavioral requirements

- **Modality**: behaves identically to `SettingsWindow`. Owner window is
  disabled for input until the dialog dismisses.
- **Themes**: renders correctly under DarkModern, Skeuomorphic,
  GreenScreen using `ChromeTheme` palette (FR-013, SC-005).
- **DPI**: re-lays out on `WM_DPICHANGED` while open (FR-013, edge case
  "DPI changes while a themed dialog is open").
- **Theme change while open**: repaints with the new palette without
  dismissing (edge case in spec).
- **Hyperlink hit-testing**: links inside the `body` text are clickable
  and dispatch `ShellExecuteW (NULL, L"open", url, …)`. On failure,
  report via `CHRN` (themed dialog, since the primitive itself is up
  by definition).
- **Keyboard**:
  - `Enter` activates the `isDefault == true` button (if any).
  - `Escape` activates the `isCancel == true` button (if any).
  - `Tab` cycles buttons left-to-right; `Shift+Tab` reverse.
- **No TaskDialog command-link buttons** (FR-004 explicitly forbids
  them). Buttons are single-line label only.
- **Window-close gesture** with no `isCancel` button → return `-1`.

## Layout contract (pure, testable in `DialogLayout`)

```cpp
struct DialogLayoutMetrics
{
    float dpiScale;
    float maxBodyWidthPx;       // bounding the wrapped body text
    float buttonHeightPx;
    float buttonPaddingPx;
    float buttonSpacingPx;
    float iconSizePx;           // 0 when icon == DialogIcon::None
    float bodyLineHeightPx;
    float outerPaddingPx;
    std::function<float (std::wstring_view)> measureBodyTextRun;
    std::function<float (std::wstring_view)> measureButtonLabel;
};

struct DialogLayoutResult
{
    SIZE                          totalSizePx;
    RECT                          iconRectPx;            // empty when no icon
    std::vector<RECT>             bodyRunRectsPx;        // 1:1 with body runs
    std::vector<RECT>             hyperlinkHitRectsPx;   // subset of body
    std::vector<RECT>             buttonRectsPx;         // 1:1 with buttons
    RECT                          customBodyRectPx;      // empty when no hook
};

DialogLayoutResult LayoutDialog (
    const DialogDefinition    & def,
    const DialogLayoutMetrics & metrics);
```

This free function is the unit-testable surface (`DialogLayoutTests.cpp`).
Tests supply deterministic `measure*` callbacks so layout math is
verified without DirectWrite.

## Failure modes

| Mode                                          | Handling                                                                |
| --------------------------------------------- | ----------------------------------------------------------------------- |
| Painter / device not initialized              | `CWRA` — bug, asserting variant. The primitive is only valid after the chrome painter is up. |
| Definition has zero buttons and no close gesture available | `CBRA` — bug; callers MUST supply at least one button or rely on the window-close gesture. |
| `ShellExecuteW` fails for a hyperlink         | `CHRN` — user-facing notification through another themed dialog.        |
| Icon resource id not found                    | `CWRA` — bug; icon ids are compile-time enum values mapped to known resources. |

## Out of scope for the primitive

- Asynchronous download / progress engine — the consumer drives its
  own progress and calls `Close` when done; the primitive just hosts
  the paint surface.
- Multi-line command-link buttons (FR-004).
- Rich-text body beyond inline hyperlinks (no bold, no inline images).
- File picker integration (`IFileOpenDialog` stays in
  `PromptForDiskImage`).
