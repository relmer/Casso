#pragma once

#include "Ui/Debugger/FieldDialogPanel.h"

struct DebugMemoryImage;





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog
//
//  One byte typed in hex, for a command that takes a value, such as the
//  Stack pane's Push byte...: OK takes it only once it is one or two hex
//  digits, with or without a dollar sign.
//
////////////////////////////////////////////////////////////////////////////////

class ByteDialog : public DxuiDialogWindow
{
public:
    static bool  TryParseByte (const std::wstring & text, Byte & value);

    //  Runs the dialog modally over `owner`; the byte when OK. A description,
    //  when given, is drawn above the field.
    //  An example, when given, is drawn under the field for each byte typed,
    //  and cleared while the text isn't a byte.
    using ExampleFn = std::function<std::wstring (Byte value)>;

    static std::optional<Byte>  RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label,
                                          const std::wstring & description, ExampleFn example = {});

    //  A pointer watch's row for a zero-page address, from the memory given:
    //  "$3E: $2000: A9 00 8D ..."; empty without memory.
    static std::wstring  FormatPointerExample (Byte address, const DebugMemoryImage * memory);

    //  Add pointer watch's description.
    static constexpr const wchar_t * kPointerWatchText = L"A pointer watch reads the two bytes at a zero-page address as an address, "
                                                         L"and lists the bytes there. For $3E = $00 and $3F = $20, its row has $2000 "
                                                         L"and the bytes at $2000.";

protected:
    void  OnCreate() override;

private:
    const IDxuiTheme     * m_theme = nullptr;
    std::wstring           m_label;
    std::wstring           m_description;
    ExampleFn              m_example;
    std::optional<Byte>    m_result;
    DxuiLabel              m_descriptionLabel;
    DxuiLabel              m_valueLabel;
    DxuiTextInput          m_value;
    DxuiLabel              m_error;
    FieldDialogPanel     * m_body  = nullptr;
};
