#include "Pch.h"

#include "Ui/Debugger/FlagsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialogPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void FlagsDialogPanel::Init (std::span<DxuiCheckbox> boxes)
{
    m_boxes = boxes;

    for (DxuiCheckbox & box : m_boxes)
    {
        Adopt (box);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialogPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FlagsDialogPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row = scaler.ToPx (kRowDip);
    int  y   = boundsDip.top;



    SetBounds (boundsDip);

    for (DxuiCheckbox & box : m_boxes)
    {
        box.Layout (RECT { boundsDip.left, y, boundsDip.right, y + row }, scaler);
        y += row;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialog::GetFlags
//
////////////////////////////////////////////////////////////////////////////////

const std::array<FlagsDialog::Flag, 7> & FlagsDialog::GetFlags()
{
    static const std::array<Flag, 7>  kFlags =
    { {
        { L'N', 0x80, L"Negative"          },
        { L'V', 0x40, L"Overflow"          },
        { L'B', 0x10, L"Break"             },
        { L'D', 0x08, L"Decimal mode"      },
        { L'I', 0x04, L"Interrupt disable" },
        { L'Z', 0x02, L"Zero"              },
        { L'C', 0x01, L"Carry"             },
    } };



    return kFlags;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialog::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FlagsDialog::Describe (Byte p)
{
    std::wstring  text;



    for (const Flag & flag : GetFlags())
    {
        if (!text.empty())
        {
            text += L"\n";
        }

        text += std::format (L"{}  {:<18} {}", flag.letter, flag.name, (p & flag.bit) ? 1 : 0);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialog::OnCreate
//
//  OK keeps its own click so the boxes are read before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void FlagsDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    for (size_t i = 0; i < m_boxes.size(); i++)
    {
        const Flag & flag = GetFlags()[i];

        m_boxes[i].SetLabel   (std::format (L"{}  {}", flag.letter, flag.name));
        m_boxes[i].SetChecked ((m_p & flag.bit) != 0);
    }

    m_body = CreateDialogContent<FlagsDialogPanel>();
    m_body->Init (m_boxes);

    ok = AddDialogButton (L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        for (size_t i = 0; i < m_boxes.size(); i++)
        {
            m_p = m_boxes[i].IsChecked() ? (Byte) (m_p | GetFlags()[i].bit) : (Byte) (m_p & ~GetFlags()[i].bit);
        }

        m_confirmed = true;
        EndDialog (IDOK);
    });

    SetInitialFocus (&m_boxes[0]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlagsDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

bool FlagsDialog::Ask (HWND owner, const IDxuiTheme * theme, Byte p, Byte & outP)
{
    HRESULT                   hr     = S_OK;
    FlagsDialog               dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme = theme;
    dialog.m_p     = p;

    params.title                    = L"Flags";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 280, 320 };
    params.minSizeDip               = { 240, 320 };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return false;
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    outP = dialog.m_p;
    return dialog.m_confirmed;
}
