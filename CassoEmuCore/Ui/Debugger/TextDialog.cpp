#include "Pch.h"

#include "Ui/Debugger/TextDialog.h"
#include "Ui/Debugger/ByteDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextDialog::TryParseBytes
//
////////////////////////////////////////////////////////////////////////////////

bool TextDialog::TryParseBytes (const std::wstring & text, std::vector<Byte> & bytes)
{
    std::wistringstream  tokens (text);
    std::wstring         token;
    Byte                 value = 0;



    bytes.clear();

    while (tokens >> token)
    {
        if (!ByteDialog::TryParseByte (token, value))
        {
            return false;
        }

        bytes.push_back (value);
    }

    return !bytes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextDialog::TryParseAddress
//
////////////////////////////////////////////////////////////////////////////////

bool TextDialog::TryParseAddress (const std::wstring & text, Word & address)
{
    size_t        first  = text.find_first_not_of (L" \t");
    size_t        last   = text.find_last_not_of (L" \t");
    std::wstring  digits = (first == std::wstring::npos) ? std::wstring() : text.substr (first, last - first + 1);



    if (digits.starts_with (L'$'))
    {
        digits.erase (0, 1);
    }

    if (digits.empty() || digits.size() > 4 || !std::ranges::all_of (digits, [] (wchar_t ch) { return iswxdigit (ch) != 0; }))
    {
        return false;
    }

    address = (Word) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void TextDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (DxuiLabel * label : { &m_valueLabel, &m_error })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_error.SetTextRole  (DxuiTextRole::Error);
    m_valueLabel.SetText (m_label);

    m_value.SetTheme        (m_theme);
    m_value.SetHwnd         (GetHwnd());
    m_value.SetTextRenderer (GetTextRenderer());
    m_value.SetMaxLength    (128);
    m_value.SetPlaceholder  (m_placeholder);

    m_body = CreateDialogContent<FieldDialogPanel>();
    m_body->Add      (m_valueLabel, m_value);
    m_body->SetError (m_error);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        std::wstring  error;



        if (m_check && !m_check (m_value.GetText(), error))
        {
            m_error.SetText (error);
            Invalidate();
            return;
        }

        m_result = m_value.GetText();
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextDialog::RunModal
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::wstring> TextDialog::RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label,
                                                  const std::wstring & placeholder, CheckFn check)
{
    HRESULT                   hr     = S_OK;
    TextDialog                dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme       = theme;
    dialog.m_label       = label;
    dialog.m_placeholder = placeholder;
    dialog.m_check       = std::move (check);

    params.title                    = title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 460, 190 };
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
