#include "Pch.h"

#include "Ui/DiskInspector/RepairDialog.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
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
    RepairPlan  full = DiskRepair::Plan (analysis, {});



    Configure (theme, FormatIntro (fileName, full), GetIntroLines (full), full,
               [&analysis] (const std::set<std::wstring> & keys) { return DiskRepair::PlanChosen (analysis, {}, keys); }, chosen.empty() ? DiskRepair::GetKeys (full) : chosen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void RepairDialog::Configure (const IDxuiTheme * theme, const std::wstring & intro, int introLines, const RepairPlan & fullPlan, PlanFn planChosen, const std::set<std::wstring> & chosen)
{
    m_theme      = theme;
    m_introText  = intro;
    m_introLines = introLines;
    m_fullPlan   = fullPlan;
    m_planChosen = std::move (planChosen);
    m_chosen     = chosen;
    m_plan       = m_planChosen (m_chosen);
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
    m_intro.SetText       (m_introText);
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
    body->Init (m_intro, m_checks, m_repairs, m_status, m_introLines);

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
    if (m_planChosen)
    {
        m_plan = m_planChosen (m_chosen);
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
//  RepairDialog::ShowModal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RepairDialog::ShowModal (HWND owner, const IDxuiTheme * theme)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    params.title                    = L"Repair disk image";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = SIZE { kWidthDip, kChromeDip + RepairDialogBody::GetHeightDip (m_fullPlan.changes.size(), m_introLines) };
    params.minSizeDip               = params.initialSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = Create (params);
    CHRA (hr);

    SetTheme (theme);
    ShowModalDialog (IDOK);

Error:
    return hr;
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
    HRESULT       hr       = S_OK;
    DiskAnalysis  analysis;
    RepairDialog  dialog;
    std::wstring  name     = fs::path (TextEncoding::Utf8ToWide (imagePath)).filename().wstring();
    std::wstring  reason;
    bool          canWrite = false;



    outCopyPath.clear();
    outPlan = RepairPlan();

    hr = RepairCopy::AnalyzeFile (io, imagePath, analysis, reason);
    CHRF (hr, outNote = L"Repair isn't available. " + reason);

    dialog.Configure (theme, name, analysis, chosen);

    canWrite = dialog.GetFullPlan().canRepair;
    CBRF (canWrite, outNote = L"Repair isn't available. " + dialog.GetFullPlan().reason);

    hr = dialog.ShowModal (owner, theme);
    CHRA (hr);

    canWrite = dialog.IsConfirmed();
    BAIL_OUT_IF (!canWrite, S_OK);

    hr = RepairCopy::Write (io, imagePath, "", dialog.GetPlan(), outCopyPath, reason);
    CHRF (hr, outNote = L"Repair failed: " + reason);

    outPlan = dialog.GetPlan();
    outNote = L"Repaired copy written: " + fs::path (TextEncoding::Utf8ToWide (outCopyPath)).filename().wstring();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::Init
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportBody::Init (const std::wstring & heading, const vector<std::wstring> & fixes, const std::wstring & footer)
{
    auto  addLabel = [this] (const std::wstring & text) -> DxuiLabel *
    {
        DxuiLabel  & label = Add<DxuiLabel> (text, DxuiTextRole::Body, DxuiTextHAlign::Left, DxuiTextVAlign::Top);

        return &label;
    };



    m_rows.push_back ({ nullptr, addLabel (heading) });

    for (const std::wstring & fix : fixes)
    {
        m_rows.push_back ({ addLabel (s_kpszGreenCheck), addLabel (fix) });
    }

    m_rows.push_back ({ nullptr, addLabel (footer) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::GetGapAfterDip
//
//  The heading and the fixes, and the fixes and the bottom line, are set
//  apart; the fixes are close together.
//
////////////////////////////////////////////////////////////////////////////////

int RepairReportBody::GetGapAfterDip (size_t row) const
{
    int  gap = 0;



    if (row == 0 || row + 2 == m_rows.size())
    {
        gap = kGapDip;
    }
    else if (row + 1 < m_rows.size())
    {
        gap = kFixGapDip;
    }

    return gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::GetFontPx
//
////////////////////////////////////////////////////////////////////////////////

float RepairReportBody::GetFontPx (const DxuiLabel & label, const IDxuiTheme & theme) const
{
    return m_scaler.ToPxf ((label.GetFontSizeDip() > 0.0f) ? label.GetFontSizeDip() : theme.BodyFont().sizeDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::GetPreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int RepairReportBody::GetPreferredHeightDip() const
{
    int  height = 0;



    for (size_t i = 0; i < m_rows.size(); i++)
    {
        height += kLineDip + GetGapAfterDip (i);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::TryMeasureWidthPx
//
//  Each line on one line, a fix after the marks' column, and two pixels to
//  spare so rounding doesn't wrap the widest line.
//
////////////////////////////////////////////////////////////////////////////////

bool RepairReportBody::TryMeasureWidthPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int maxPx, int & widthPx)
{
    HRESULT  hr      = S_OK;
    float    widest  = 0.0f;
    float    measure = 0.0f;
    float    height  = 0.0f;
    int      mark    = m_scaler.ToPx (kMarkDip);



    for (const Row & row : m_rows)
    {
        hr = text.MeasureString (row.label->GetText().c_str(), GetFontPx (*row.label, theme), DxuiTheme::GetUiFace(), measure, height);
        CHR (hr);

        widest = std::max (widest, measure + ((row.mark != nullptr) ? mark : 0));
    }

    widthPx = std::clamp (static_cast<int> (std::ceil (widest)) + 2, std::min (m_scaler.ToPx (kMinDip), maxPx), maxPx);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::TryMeasureHeightPx
//
//  Each line wrapped at the width its label gets, as Layout places it.
//
////////////////////////////////////////////////////////////////////////////////

bool RepairReportBody::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    HRESULT  hr      = S_OK;
    int      line    = m_scaler.ToPx (kLineDip);
    int      mark    = m_scaler.ToPx (kMarkDip);
    float    measure = 0.0f;
    float    height  = 0.0f;



    heightPx = 0;

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        Row  & row = m_rows[i];

        hr = text.MeasureStringWrapped (row.label->GetText().c_str(), GetFontPx (*row.label, theme), DxuiTheme::GetUiFace(),
                                        static_cast<float> (widthPx - ((row.mark != nullptr) ? mark : 0)), measure, height);
        CHR (hr);

        row.heightPx  = std::max (line, static_cast<int> (std::ceil (height)));
        heightPx     += row.heightPx + m_scaler.ToPx (GetGapAfterDip (i));
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportBody::Layout
//
//  A fix's mark at the left, and its text from the marks' column to the
//  right edge, so its wrapped lines start under its first.
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportBody::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  line = 0;
    int  mark = 0;
    int  y    = boundsPx.top;
    int  h    = 0;



    SetBounds (boundsPx);
    m_scaler = scaler;
    line     = scaler.ToPx (kLineDip);
    mark     = scaler.ToPx (kMarkDip);

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        Row  & row  = m_rows[i];
        int    left = boundsPx.left + ((row.mark != nullptr) ? mark : 0);

        h = (row.heightPx > 0) ? row.heightPx : line;

        if (row.mark != nullptr)
        {
            row.mark->Layout (RECT { boundsPx.left, y, left, y + line }, scaler);
        }

        row.label->Layout (RECT { left, y, boundsPx.right, y + h }, scaler);
        y += h + scaler.ToPx (GetGapAfterDip (i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportDialog::Configure (const RepairPlan & plan, const std::wstring & footer)
{
    m_fixes.clear();

    for (const RepairChange & change : plan.changes)
    {
        m_fixes.push_back (change.text);
    }

    m_footer = footer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::FormatHeading
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairReportDialog::FormatHeading()
{
    return L"Applied these fixes:";
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::FormatFooter
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RepairReportDialog::FormatFooter (bool canOpen, const std::wstring & copyName)
{
    return canOpen ? std::wstring (L"Press OK to load the repaired disk image into Disk Inspector.")
                   : L"The repaired copy is " + RepairDialog::FormatName (copyName) + L".";
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportDialog::OnCreate()
{
    DxuiButton  * ok = nullptr;



    m_body = CreateDialogContent<RepairReportBody>();
    m_body->Init (FormatHeading(), m_fixes, m_footer);

    ok = AddDialogButton (L"OK", IDOK);
    ok->SetOnClick ([this] () { m_isOk = true; EndDialog (IDOK); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::Layout
//
//  The resize lays the window out again, and that layout doesn't fit the
//  width again.
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportDialog::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    DxuiDialogWindow::Layout (boundsPx, scaler);

    if (!m_isFitting)
    {
        m_isFitting = true;
        FitWidth (scaler);
        m_isFitting = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::FitWidth
//
//  The window is created at its widest, kMaxWidthDip; the body is narrowed
//  to its widest line, up to the body's width in a window that wide. The
//  frame and the margins around the body are what the window has beyond the
//  body's width now.
//
////////////////////////////////////////////////////////////////////////////////

void RepairReportDialog::FitWidth (const DxuiDpiScaler & scaler)
{
    HRESULT              hr      = S_OK;
    IDxuiTextRenderer  * text    = GetTextRenderer();
    const IDxuiTheme   * theme   = GetTheme();
    RECT                 window  = {};
    RECT                 body    = {};
    int                  chrome  = 0;
    int                  needPx  = 0;
    bool                 ok      = false;



    BAIL_OUT_IF (m_body == nullptr || GetHwnd() == nullptr || text == nullptr || theme == nullptr, S_OK);

    ok = GetWindowRect (GetHwnd(), &window) != FALSE;
    CBR (ok);

    body   = m_body->GetBounds();
    chrome = (window.right - window.left) - (body.right - body.left);
    ok     = m_body->TryMeasureWidthPx (*text, *theme, scaler.ToPx (kMaxWidthDip) - chrome, needPx);
    CBR (ok);

    hr = ResizeWidthBy (needPx - static_cast<int> (body.right - body.left));
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairReportDialog::Run
//
////////////////////////////////////////////////////////////////////////////////

bool RepairReportDialog::Run (HWND owner, const IDxuiTheme * theme, const RepairPlan & plan, const std::wstring & footer)
{
    HRESULT                   hr       = S_OK;
    RepairReportDialog        dialog;
    DxuiWindow::CreateParams  params;
    RepairReportBody          estimate;



    dialog.Configure (plan, footer);
    estimate.Init (FormatHeading(), dialog.m_fixes, footer);

    params.title                    = L"Repair complete";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kMaxWidthDip, kChromeDip + estimate.GetPreferredHeightDip() };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

Error:
    return dialog.IsOk();
}
