#include "Pch.h"

#include "Ui/Debugger/ByteDialog.h"





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
    m_body->Add      (m_valueLabel, m_value);
    m_body->SetError (m_error);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        Byte  value = 0;



        if (!TryParseByte (m_value.GetText(), value))
        {
            m_error.SetText (L"A byte is one or two hex digits, as $A9.");
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

std::optional<Byte> ByteDialog::RunModal (HWND owner, const IDxuiTheme * theme, const std::wstring & title, const std::wstring & label)
{
    HRESULT                   hr     = S_OK;
    ByteDialog                dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme = theme;
    dialog.m_label = label;

    params.title                    = title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 360, 190 };
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
