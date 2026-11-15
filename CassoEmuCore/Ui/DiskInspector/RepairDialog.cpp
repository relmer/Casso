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

void RepairDialogBody::Init (DxuiLabel & intro, vector<std::unique_ptr<DxuiCheckbox>> & checks, DxuiLabel & status)
{
    m_intro  = &intro;
    m_checks = &checks;
    m_status = &status;

    Adopt (intro);

    for (std::unique_ptr<DxuiCheckbox> & check : checks)
    {
        Adopt (*check);
    }

    Adopt (status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialogBody::Layout
//
//  The intro on two lines, a row for each change under it, and the status
//  line at the bottom.
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

    for (std::unique_ptr<DxuiCheckbox> & box : *m_checks)
    {
        box->Layout (RECT { x, y, right, y + check }, scaler);
        y += check;
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

int RepairDialogBody::GetHeightDip (size_t changes)
{
    return 2 * kPadDip + 4 * kLineDip + static_cast<int> (changes) * kCheckDip;
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
    return SIZE { kWidthDip, kChromeDip + RepairDialogBody::GetHeightDip (plan.changes.size()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::FormatIntro
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairDialog::FormatIntro (const std::wstring & fileName)
{
    return L"Writes a repaired copy of " + fileName + L" in the same folder, with the changes checked below. " + fileName + L" isn't changed.";
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
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::OnCreate()
{
    RepairDialogBody *  body = nullptr;



    m_intro.SetTextRole   (DxuiTextRole::Body);
    m_intro.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_intro.SetText       (FormatIntro (m_fileName));
    m_status.SetTextRole  (DxuiTextRole::Error);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_checks.clear();

    for (const RepairChange & change : m_fullPlan.changes)
    {
        auto          box = std::make_unique<DxuiCheckbox> (change.text, m_chosen.contains (change.key));
        std::wstring  key = change.key;

        box->SetOnChange ([this, key] (bool isChecked) { SetChosen (key, isChecked); });
        m_checks.push_back (std::move (box));
    }

    body = CreateDialogContent<RepairDialogBody>();
    body->Init (m_intro, m_checks, m_status);

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

    outNote = L"Repaired copy written: " + fs::path (TextEncoding::Utf8ToWide (outCopyPath)).filename().wstring();

Error:
    return hr;
}
