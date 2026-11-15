#include "Pch.h"

#include "Ui/DiskInspector/RepairDialog.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::Init
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialogBody::Init (
    DxuiLabel                              & intro,
    vector<std::unique_ptr<DxuiCheckbox>>  & checks,
    vector<std::unique_ptr<DxuiLabel>>     & repairs,
    DxuiLabel                              & status,
    int                                      introLines)
{
    m_intro   = &intro;
    m_checks  = &checks;
    m_repairs = &repairs;
    m_status  = &status;
    m_lines   = introLines;

    Adopt (intro);

    for (std::unique_ptr<DxuiCheckbox> & check : checks)
    {
        Adopt (*check);
    }

    for (std::unique_ptr<DxuiLabel> & repair : repairs)
    {
        Adopt (*repair);
    }

    Adopt (status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::Layout
//
//  The intro on its lines, then each change's check box and its repair's
//  line under it, a gap after each, and the status line at the bottom.
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialogBody::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  pad    = scaler.ToPx (kPadDip);
    int  line   = scaler.ToPx (kLineDip);
    int  check  = scaler.ToPx (kCheckDip);
    int  indent = scaler.ToPx (kIndentDip);
    int  gap    = scaler.ToPx (kGapDip);
    int  x      = boundsPx.left + pad;
    int  right  = boundsPx.right - pad;
    int  y      = boundsPx.top + pad;



    SetBounds (boundsPx);

    m_intro->Layout (RECT { x, y, right, y + m_lines * line }, scaler);
    y += m_lines * line;

    for (size_t i = 0; i < m_checks->size(); i++)
    {
        (*m_checks)[i]->Layout (RECT { x, y, right, y + check }, scaler);
        y += check;

        if (i < m_repairs->size())
        {
            (*m_repairs)[i]->Layout (RECT { x + indent, y, right, y + line }, scaler);
            y += line;
        }

        y += gap;
    }

    m_status->Layout (RECT { x, boundsPx.bottom - pad - 2 * line, right, boundsPx.bottom - pad }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool RepairDialogBody::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = false;



    for (size_t i = 0; i < m_checks->size() && !isHandled; i++)
    {
        isHandled = (*m_checks)[i]->OnMouse (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int RepairDialogBody::GetHeightDip (size_t changes, int introLines)
{
    return 2 * kPadDip + (introLines + 2) * kLineDip + static_cast<int> (changes) * (kCheckDip + kLineDip + kGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Configure
//
//  The full plan, made with every change, lists the changes; the plan made
//  from the checked ones is the one written.
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::Configure (const IDxuiTheme * theme, const std::wstring & fileName, const DiskAnalysis & analysis, const std::set<std::wstring> & chosen)
{
    m_theme    = theme;
    m_fileName = fileName;
    m_analysis = &analysis;
    m_fullPlan = DiskRepair::Plan (analysis, {});
    m_chosen   = chosen.empty() ? DiskRepair::GetKeys (m_fullPlan) : chosen;
    m_plan     = DiskRepair::PlanChosen (analysis, {}, m_chosen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::SetChosen
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::SetChosen (const std::wstring & key, bool isChosen)
{
    if (isChosen)
    {
        m_chosen.insert (key);
    }
    else
    {
        m_chosen.erase (key);
    }

    Replan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::GetSizeDip
//
////////////////////////////////////////////////////////////////////////////////

SIZE RepairDialog::GetSizeDip (const RepairPlan & plan)
{
    return SIZE { kWidthDip, kChromeDip + RepairDialogBody::GetHeightDip (plan.changes.size(), GetIntroLines (plan)) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatIntro (const std::wstring & fileName)
{
    std::wstring  name = FormatName (fileName);



    return L"Repairing an image writes a repaired copy of " + name + L" in the same folder. " + name + L" isn't changed. "
           L"Select the repairs below to apply to the repaired image.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatName (const std::wstring & fileName)
{
    return (fileName.find (L' ') != std::wstring::npos) ? L"\"" + fileName + L"\"" : fileName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatIntro (const std::wstring & fileName, const RepairPlan & plan)
{
    std::wstring  intro = FormatIntro (fileName);



    if (plan.before == FileSystemVerdict::Suspect)
    {
        intro += L"\n\nThese findings can be normal on a disk made by another DOS or by a program that uses the VTOC or catalog differently. "
                 L"Repair a disk like this only when it doesn't work.";
    }

    return intro;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::GetIntroLines
//
//  Three lines for the intro, and three more for the caution and the blank
//  line before it.
//
////////////////////////////////////////////////////////////////////////////////

int RepairDialog::GetIntroLines (const RepairPlan & plan)
{
    return (plan.before == FileSystemVerdict::Suspect) ? 6 : 3;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatRepair
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatRepair (const RepairChange & change)
{
    return L"Repair: " + change.text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatStatus
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatStatus (const RepairPlan & plan)
{
    return plan.canRepair ? std::wstring() : plan.reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::OnCreate
//
//  Each change's check box has the problem it sets right, and the line
//  under it the repair.
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::OnCreate()
{
    RepairDialogBody *  body = nullptr;



    m_intro.SetTextRole   (DxuiTextRole::Body);
    m_intro.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_intro.SetText       (FormatIntro (m_fileName, m_fullPlan));
    m_status.SetTextRole  (DxuiTextRole::Error);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_checks.clear();
    m_repairs.clear();

    for (const RepairChange & change : m_fullPlan.changes)
    {
        auto          box    = std::make_unique<DxuiCheckbox> (change.problem.empty() ? change.text : change.problem, m_chosen.contains (change.key));
        auto          repair = std::make_unique<DxuiLabel> (FormatRepair (change), DxuiTextRole::Muted, DxuiTextHAlign::Left, DxuiTextVAlign::Top);
        std::wstring  key    = change.key;

        box->SetOnChange ([this, key] (bool isChecked) { SetChosen (key, isChecked); });
        m_checks.push_back  (std::move (box));
        m_repairs.push_back (std::move (repair));
    }

    body = CreateDialogContent<RepairDialogBody>();
    body->Init (m_intro, m_checks, m_repairs, m_status, GetIntroLines (m_fullPlan));

    m_repair = AddDialogButton (L"Repair", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_repair->SetOnClick ([this] () { m_isConfirmed = m_plan.canRepair; EndDialog (IDOK); });

    Replan();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Replan
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::Replan()
{
    if (m_analysis != nullptr)
    {
        m_plan = DiskRepair::PlanChosen (*m_analysis, {}, m_chosen);
    }

    m_status.SetText (FormatStatus (m_plan));

    if (m_repair != nullptr)
    {
        m_repair->SetEnabled (m_plan.canRepair);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RepairDialog::Run (
    HWND                            owner,
    const IDxuiTheme              * theme,
    IDiskFileIo                   & io,
    const std::string             & imagePath,
    const std::set<std::wstring>  & chosen,
    std::string                   & outCopyPath,
    RepairPlan                    & outPlan,
    std::wstring                  & outNote)
{
    HRESULT                   hr       = S_OK;
    DiskAnalysis              analysis;
    RepairDialog              dialog;
    DxuiWindow::CreateParams  params;
    std::wstring              name     = fs::path (TextEncoding::Utf8ToWide (imagePath)).filename().wstring();
    std::wstring              reason;
    bool                      canWrite = false;



    outCopyPath.clear();
    outPlan = RepairPlan();

    hr = RepairCopy::AnalyzeFile (io, imagePath, analysis, reason);
    CHRF (hr, outNote = L"Repair isn't available. " + reason);

    dialog.Configure (theme, name, analysis, chosen);

    canWrite = dialog.GetFullPlan().canRepair;
    CBRF (canWrite, outNote = L"Repair isn't available. " + dialog.GetFullPlan().reason);

    params.title                    = L"Repair disk image";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = GetSizeDip (dialog.GetFullPlan());
    params.minSizeDip               = params.initialSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    canWrite = dialog.IsConfirmed();
    BAIL_OUT_IF (!canWrite, S_OK);

    hr = RepairCopy::Write (io, imagePath, "", dialog.GetPlan(), outCopyPath, reason);
    CHRF (hr, outNote = L"Repair failed: " + reason);

    outPlan = dialog.GetPlan();
    outNote = L"Repaired copy written: " + fs::path (TextEncoding::Utf8ToWide (outCopyPath)).filename().wstring();

Error:
    return hr;
}
