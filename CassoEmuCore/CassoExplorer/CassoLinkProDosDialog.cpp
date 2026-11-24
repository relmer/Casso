#include "Pch.h"

#include "CassoExplorer/CassoLinkProDosDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosPanel::Init (DxuiLabel & note, DxuiLabel & label, DxuiComboBox & disks, DxuiButton & browse)
{
    m_note   = &note;
    m_label  = &label;
    m_disks  = &disks;
    m_browse = &browse;

    Adopt (note);
    Adopt (label);
    Adopt (disks);
    Adopt (browse);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scaler = scaler;

    Arrange (boundsDip, scaler, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkProDosPanel::OnMouse (const DxuiMouseEvent & ev)
{
    if (m_onMouse)
    {
        m_onMouse (ev);
    }

    return m_disks->OnMouse (ev) || m_browse->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkProDosPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { 0, 0, widthPx, 0 };



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    heightPx = Arrange (bounds, m_scaler, false);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosPanel::Arrange
//
//  Returns the height of the explanation and the row under it.
//
////////////////////////////////////////////////////////////////////////////////

int CassoLinkProDosPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    HRESULT  hr       = S_OK;
    int      width    = bounds.right - bounds.left;
    float    widthDip = (float) width * (float) DxuiDpiScaler::kBaseDpi / (float) scaler.GetDpi();
    float    textDip  = 0.0f;
    int      noteH    = scaler.ToPx (ExplorerMetrics::kStackLabelHeightDip);
    int      gap      = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int      field    = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int      fieldX   = bounds.left + scaler.ToPx (kLabelWidthDip);
    int      browseX  = bounds.right - scaler.ToPx (kBrowseWidthDip);
    int      top      = 0;



    hr = m_note->MeasureWrappedDip (widthDip, textDip);

    if (SUCCEEDED (hr))
    {
        noteH = (std::max) (noteH, scaler.ToPx ((int) std::ceil (textDip)));
    }

    top = bounds.top + noteH + 3 * gap;

    if (place)
    {
        m_note->Layout   (RECT { bounds.left, bounds.top, bounds.right, bounds.top + noteH }, scaler);
        m_label->Layout  (RECT { bounds.left, top, fieldX, top + field }, scaler);
        m_disks->Layout  (RECT { fieldX, top, browseX - gap, top + field }, scaler);
        m_browse->Layout (RECT { browseX, top, bounds.right, top + field }, scaler);
    }

    return top + field - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::OnCreate
//
//  The confirm button has a click handler so the chosen disk is read
//  before the dialog ends.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosDialog::OnCreate()
{
    m_note.SetText (m_request.note);
    ExplorerTextStyles::Apply (m_note, ExplorerTextStyles::kFormLabel);
    m_note.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_label.SetText (L"ProDOS disk:");
    ExplorerTextStyles::Apply (m_label, ExplorerTextStyles::kFormLabel);
    m_label.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_combo.SetPopupHost (GetPopupHost());
    m_combo.SetElide     (DxuiElide::Middle);
    m_combo.SetOnHighlightChange ([this] (int index) { ShowListTip (index); });
    m_browse.SetLabel    (L"Browse...");

    m_tip.SetPopupHost (GetPopupHost());

    if (m_theme != nullptr)
    {
        m_tip.SetTheme (*m_theme);
    }

    m_body = CreateDialogContent<CassoLinkProDosPanel>();
    m_body->Init (m_note, m_label, m_combo, m_browse);
    m_body->SetOnMouse ([this] (const DxuiMouseEvent & ev) { ShowTip (ev); });
    SetDialogTickIntervalMs (kTipTickMs);

    m_ok = AddDialogButton (m_request.confirm, IDOK);
    m_ok->SetVariant (m_request.destructive ? DxuiButton::Variant::Danger : DxuiButton::Variant::Default);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_disks = m_request.history;
    ShowDisks (m_disks.empty() ? -1 : 0);

    m_browse.SetOnClick ([this]()
    {
        int           index = m_combo.GetSelectedIndex();
        std::wstring  disk  = (index >= 0 && index < (int) m_disks.size()) ? m_disks[(size_t) index] : std::wstring();



        if (m_request.browse && m_request.browse (GetHwnd(), disk) && !disk.empty())
        {
            std::erase_if (m_disks, [&disk] (const std::wstring & listed) { return _wcsicmp (listed.c_str(), disk.c_str()) == 0; });
            m_disks.insert (m_disks.begin(), disk);
            ShowDisks (0);
        }
    });

    m_ok->SetOnClick ([this]()
    {
        int  index = m_combo.GetSelectedIndex();



        if (index >= 0 && index < (int) m_disks.size())
        {
            m_chosen    = m_disks[(size_t) index];
            m_confirmed = true;
            EndDialog (IDOK);
        }
    });

    SetInitialFocus (m_disks.empty() ? &m_browse : m_ok);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::ShowDisks
//
//  The dropdown lists the disks by file name, and the confirm button is
//  enabled once one is chosen.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosDialog::ShowDisks (int selected)
{
    std::vector<std::wstring>  files;



    for (const std::wstring & disk : m_disks)
    {
        files.push_back (std::filesystem::path (disk).filename().wstring());
    }

    m_combo.SetItems    (files);
    m_combo.SetSelected (selected);
    m_combo.SetEnabled  (!m_disks.empty());
    m_ok->SetEnabled    (selected >= 0);

    if (GetHwnd() != nullptr)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::ShowTip
//
//  With the pointer over the closed dropdown, the tip has the chosen
//  disk's full path; anywhere else, the tip is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosDialog::ShowTip (const DxuiMouseEvent & ev)
{
    int64_t  now   = (int64_t) GetTickCount64();
    int      index = m_combo.GetSelectedIndex();



    if (ev.kind != DxuiMouseEventKind::Move || m_combo.IsOpen())
    {
        return;
    }

    if (m_combo.HitTest (ev.positionDip.x, ev.positionDip.y) && index >= 0 && index < (int) m_disks.size())
    {
        m_tip.RequestShow (m_combo.GetRect(), m_disks[(size_t) index], now);
    }
    else
    {
        m_tip.RequestHide (now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::ShowListTip
//
//  With the list open, the tip under the pointer has the full path of the
//  row the pointer is over.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosDialog::ShowListTip (int index)
{
    int64_t  now     = (int64_t) GetTickCount64();
    POINT    pointer = {};



    if (m_combo.IsOpen() && index >= 0 && index < (int) m_disks.size() && DxuiTooltip::LocatePointer (GetHwnd(), pointer))
    {
        m_tip.RequestShowNowBelowPointer (pointer, m_disks[(size_t) index], now);
        m_listTip = true;
    }
    else
    {
        m_tip.RequestHide (now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::OnDialogTick
//
//  The tip opens and closes on its dwell timers, and is hidden once the
//  list is closed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkProDosDialog::OnDialogTick()
{
    int64_t  now = (int64_t) GetTickCount64();



    if (m_listTip && !m_combo.IsOpen())
    {
        m_tip.RequestHide (now);
        m_listTip = false;
    }

    m_tip.Tick (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDialog::Choose
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkProDosDialog::Choose (HWND owner, const IDxuiTheme * theme, const Request & request, std::wstring & outDisk)
{
    HRESULT                   hr     = S_OK;
    CassoLinkProDosDialog     dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_request = request;
    dialog.m_theme   = theme;

    params.title                    = request.title;
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 560, 240 };
    params.minSizeDip               = { 440, 180 };
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
    dialog.ShowModalDialog (IDCANCEL);

    outDisk = dialog.m_chosen;

    return dialog.m_confirmed;
}
