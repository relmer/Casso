#include "Pch.h"

#include "Ui/Debugger/ByteDialog.h"
#include "Debugger/DebugMemoryImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog::TryParseByte
//
////////////////////////////////////////////////////////////////////////////////

bool ByteDialog::TryParseByte (const std::wstring & text, Byte & value)
{
    size_t        first  = text.find_first_not_of (L" \t");
    size_t        last   = text.find_last_not_of (L" \t");
    std::wstring  digits = (first == std::wstring::npos) ? std::wstring() : text.substr (first, last - first + 1);



    if (digits.starts_with (L'$'))
    {
        digits.erase (0, 1);
    }

    if (digits.empty() || digits.size() > 2 || !std::ranges::all_of (digits, [] (wchar_t ch) { return iswxdigit (ch) != 0; }))
    {
        return false;
    }

    value = (Byte) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void ByteDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_valueLabel, &m_error })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_error.SetTextRole   (DxuiTextRole::Error);
    m_valueLabel.SetText  (m_label);

    m_value.SetTheme        (m_theme);
    m_value.SetHwnd         (GetHwnd());
    m_value.SetTextRenderer (GetTextRenderer());
    m_value.SetMaxLength    (4);
    m_value.SetPlaceholder  (L"$00");

    m_body = CreateDialogContent<FieldDialogPanel>();

    if (!m_description.empty())
    {
        m_descriptionLabel.SetTextRole (DxuiTextRole::Body);
        m_descriptionLabel.SetText     (m_description);
        m_body->SetDescription         (m_descriptionLabel);
    }

    m_body->Add      (m_valueLabel, m_value);

    if (m_example)
    {
        m_value.SetOnChange ([this] (const std::wstring & text)
        {
            Byte  value = 0;



            m_error.SetTextRole (DxuiTextRole::Body);
            m_error.SetText     (TryParseByte (text, value) ? m_example (value) : std::wstring());
            Invalidate();
        });
    }

    m_body->SetError (m_error);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        Byte  value = 0;



        if (!TryParseByte (m_value.GetText(), value))
        {
            m_error.SetTextRole (DxuiTextRole::Error);
            m_error.SetText     (L"A byte is one or two hex digits, as $A9.");
            Invalidate();
            return;
        }

        m_result = value;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> ByteDialog::RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label,
                                          const std::wstring & description, ExampleFn example)
{
    HRESULT                   hr     = S_OK;
    ByteDialog                dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme = theme;
    dialog.m_label       = label;
    dialog.m_description = description;
    dialog.m_example     = std::move (example);

    params.title                    = title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 360, description.empty() ? 190 : 270 };
    params.minSizeDip               = params.initialSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return std::nullopt;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    return dialog.m_result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ByteDialog::FormatPointerExample
//
//  The address the zero-page byte and the one after it form, then the
//  first bytes there; a byte a peek doesn't read, an I/O byte, is "--".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ByteDialog::FormatPointerExample (Byte address, const DebugMemoryImage * memory)
{
    static constexpr int  kShownBytes = 3;
    Word                  target      = 0;
    std::wstring          text;



    if (memory == nullptr)
    {
        return text;
    }

    target = (Word) (memory->bytes[address] | (memory->bytes[(Byte) (address + 1)] << 8));
    text   = std::format (L"${:02X}: ${:04X}:", address, target);

    for (int each = 0; each < kShownBytes; each++)
    {
        Word  at = (Word) (target + each);

        text += memory->readable[at] ? std::format (L" {:02X}", memory->bytes[at]) : std::wstring (L" --");
    }

    return text + L" ...";
}
