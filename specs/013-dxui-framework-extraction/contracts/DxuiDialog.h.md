# Contract: Dialog API (post-2026-07 reshape)

> **SUPERSEDED / RENAMED (2026-07).** The original `DxuiDialog` + `DxuiDialogManager`
> (`std::future<int> Show(...)`, `ShowParams{ modalScrim }`) documented by FR-070/071/072
> were **deleted** (FR-126, commits `944ce5b`→`a439d66`). `Dxui/Dialog/` no longer exists.
> A dialog is now a **`DxuiWindow`** shown via a modal (or modeless) message loop. This
> file documents the surviving API; the historical struct is kept at the bottom for reference.

## Surviving dialog API

```cpp
// Dxui/Window/DxuiWindow.h  (excerpt)
class DxuiWindow : public DxuiPanel
{
public:
    // Modal: pumps its own message loop until EndDialog(commandId) is called
    // (button click / Enter->default / Escape->cancel). Returns the command id.
    int   ShowModalDialog    (int defaultButtonId);

    // Modeless: shows without blocking; used for live-preview dialogs that must
    // let input reach the owner (e.g. a settings sheet previewing on the emulator).
    void  ShowModelessDialog (int defaultButtonId);

    void  EndDialog          (int commandId);   // closes a ShowModal/ShowModeless dialog
};
```

```cpp
// Dxui/Window/DxuiDialogWindow.h  (excerpt)
// Factors the shared content + button-row shape so consumers build OnCreate()
// then call ShowModalDialog(defaultId) / ShowModelessDialog(defaultId). Buttons
// carry a command id (IDOK / IDCANCEL / IDYES / ...); the base auto-closes via
// EndDialog(commandId) on click.
class DxuiDialogWindow : public DxuiWindow { /* content panel + button row */ };
```

## Paged sheet — `DxuiPropertySheet` / `DxuiPropertyPage` (FR-129)

```cpp
// Dxui/Window/DxuiPropertyPage.h  (excerpt)
class DxuiPropertyPage : public DxuiPanel
{
public:
    void  MarkDirty (bool dirty);          // fires the on-dirty-changed callback
    bool  IsDirty   () const;
    void  SetOnDirtyChanged (std::function<void(bool)> cb);

    virtual bool  OnApply ();              // return false to block OK/Apply and stay on this page
};

// Dxui/Window/DxuiPropertySheet.h  (excerpt)
// Tab strip over registered pages + an OK / Cancel / Apply button row.
// Shown via DxuiWindow::ShowModalDialog(IDOK) (or ShowModelessDialog(IDOK) for a
// live-preview sheet).
class DxuiPropertySheet : public DxuiDialogWindow
{
public:
    void  AddPage             (DxuiPropertyPage * page, const std::wstring & tabLabel);
    bool  ApplyAllDirtyPages ();           // OK/Apply path; false if any OnApply() blocked
    void  RefreshApplyEnabled ();          // Apply enabled iff >= 1 page dirty
    // OK   -> ApplyAllDirtyPages() then EndDialog(IDOK)
    // Apply-> ApplyAllDirtyPages() (stays open), RefreshApplyEnabled()
    // Cancel-> EndDialog(IDCANCEL)  (no built-in undo; pages snapshot their own baseline)
};
```

## Contract notes

- No built-in undo on Cancel: each page captures its own pre-dialog baseline (e.g. in a
  page-local snapshot) and restores it. This mirrors Casso's `SettingsPanel` bookkeeping.
- Dialog / page strings use `const std::wstring &` inputs per FR-080.
- Casso has **not** adopted `DxuiPropertySheet` yet; `SettingsPanel` remains a hand-rolled
  `[OK] [Cancel]` sheet (no Apply button). FR-131 / FR-132 add a staged machine/ROM restart
  notice and a Theme "Apply now" affordance to that bespoke panel; a later migration onto
  `DxuiPropertySheet` is the natural end-state.

## Historical (deleted) — original FR-070/071/072 shape

```cpp
// REMOVED. Kept only to explain what the coverage matrix's FR-070/071/072 rows meant.
struct ShowParams { bool modalScrim = false; };
class DxuiDialog : public DxuiPanel {
    void SetTitle (const std::wstring &);
    void SetContent (std::unique_ptr<DxuiPanel>);
    void AddButton (const std::wstring &, int returnCode, bool isDefault = false);
};
// DxuiDialogManager::Show(std::unique_ptr<DxuiDialog>, ShowParams) -> std::future<int>
```
