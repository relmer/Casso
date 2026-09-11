# Contract: DxuiCommand

**Feature**: 032-dxui-command-widgets | **Date**: 2026-09-10

`Dxui/Core/DxuiCommand.h`. Signatures are intent; the header is
authoritative once it exists.

```cpp
struct DxuiCommand
{
    int                                id = 0;
    std::wstring                       label;
    std::wstring                       shortLabel;
    const wchar_t *                    glyph = nullptr;
    std::wstring                       tip;
    std::wstring                       accelerator;
    std::function<void ()>             dispatch;
    std::function<bool ()>             isChecked;
    std::function<bool ()>             isEnabled;
    std::function<std::wstring ()>     labelText;

    bool          IsChecked    () const;   // false when absent
    bool          IsEnabled    () const;   // true when absent
    std::wstring  GetLabelText () const;   // labelText() when present, else label
    std::wstring  GetShortText () const;   // shortLabel when present, else GetLabelText()
};
```

Rules a surface owes a command:

- Read label, glyph, tip, accelerator, checked and enabled at paint time and
  at click time. Never cache them across frames.
- Do not dispatch when `IsEnabled` is false.
- Hold the command by pointer. The application owns the object and keeps
  it alive for as long as any surface references it.

Rules a command owes a surface:

- `dispatch` is non-null.
- Functors are cheap and side-effect free; they run every frame.

Emulator use: `id` is the `IDM_*` value and `dispatch` calls the shell's
existing command handler with it, so thread routing and `WM_COMMAND`
driving are unchanged.
