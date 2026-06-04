# Contract: `DxuiDialog` and `ShowParams`

```cpp
// Dxui/Dialog/DxuiDialog.h
#pragma once

#include "Pch.h"
#include "DxuiPanel.h"


struct ShowParams
{
    bool modalScrim = false;
};


class DxuiDialog : public DxuiPanel
{
public:
    DxuiDialog ();

    void SetTitle   (const std::wstring & title);
    void SetContent (std::unique_ptr<DxuiPanel> content);
    void AddButton  (const std::wstring & label,
                     int returnCode,
                     bool isDefault = false);
};
```

## Contract notes

- `DxuiDialogManager::Show` owns dialogs via `std::unique_ptr<DxuiDialog>` and receives `ShowParams` by value.
- `modalScrim` defaults to `false`; callers opt in per dialog.
- Dialog strings use `const std::wstring &` inputs per FR-080.
