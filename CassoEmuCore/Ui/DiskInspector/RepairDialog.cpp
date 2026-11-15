#include "Pch.h"

#include "Ui/DiskInspector/RepairDialog.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::Init
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialogBody::Init (DxuiLabel & intro, DxuiLabel & changes, DxuiCheckbox & noFree)
{
    m_intro   = &intro;
    m_changes = &changes;
    m_noFree  = &noFree;

    Adopt (intro);
    Adopt (changes);
    Adopt (noFree);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::Layout
//
//  The intro on one line, the changes under it a line each, and the check
//  box at the bottom.
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialogBody::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  pad   = scaler.ToPx (kPadDip);
    int  line  = scaler.ToPx (kLineDip);
    int  check = scaler.ToPx (kCheckDip);
    int  x     = boundsPx.left + pad;
    int  right = boundsPx.right - pad;
    int  y     = boundsPx.top + pad;



    SetBounds (boundsPx);

    m_intro->Layout (RECT { x, y, right, y + 2 * line }, scaler);
    y += 2 * line;

    m_changes->Layout (RECT { x, y, right, boundsPx.bottom - pad - check }, scaler);
    m_noFree->Layout    (RECT { x, boundsPx.bottom - pad - check, right, boundsPx.bottom - pad }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool RepairDialogBody::OnMouse (const DxuiMouseEvent & ev)
{
    return m_noFree->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int RepairDialogBody::GetHeightDip (size_t lines)
{
    return 2 * kPadDip + 2 * kLineDip + static_cast<int> (lines) * kLineDip + kCheckDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Configure
//
//  The plan made with sectors that no file uses freed, which has the most
//  changes, so the dialog is sized for it.
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::Configure (const IDxuiTheme * theme, const std::wstring & fileName, Planner planner)
{
    m_theme    = theme;
    m_fileName = fileName;
    m_planner  = std::move (planner);
    m_plan     = m_planner (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::GetSizeDip
//
////////////////////////////////////////////////////////////////////////////////

SIZE RepairDialog::GetSizeDip (const RepairPlan & plan)
{
    return SIZE { kWidthDip, kChromeDip + RepairDialogBody::GetHeightDip (plan.changes.size()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatIntro (const std::wstring & fileName)
{
    return L"Repair writes a copy of " + fileName + L" beside it with these changes. " + fileName + L" isn't changed.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatChanges
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatChanges (const RepairPlan & plan)
{
    std::wstring  text;



    for (const std::wstring & change : plan.changes)
    {
        text += (text.empty() ? L"" : L"\n") + std::wstring (s_kpszBullet) + L" " + change;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::OnCreate()
{
    RepairDialogBody *  body   = nullptr;
    DxuiButton *        repair = nullptr;



    m_intro.SetTextRole   (DxuiTextRole::Body);
    m_intro.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_intro.SetText       (FormatIntro (m_fileName));
    m_changes.SetTextRole (DxuiTextRole::Body);
    m_changes.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_changes.SetText     (FormatChanges (m_plan));

    m_noFree.SetOnChange ([this] (bool) { Replan(); });

    body = CreateDialogContent<RepairDialogBody>();
    body->Init (m_intro, m_changes, m_noFree);

    repair = AddDialogButton (L"Repair", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    repair->SetOnClick ([this] () { m_isConfirmed = m_plan.canRepair; EndDialog (IDOK); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Replan
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::Replan()
{
    m_plan = m_planner (m_noFree.IsChecked());
    m_changes.SetText (FormatChanges (m_plan));
    Invalidate();
}
