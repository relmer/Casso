#include "Pch.h"

#include "DiskPortPage.h"

#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Shell/DiskPortChainRule.h"





static constexpr int  s_kPagePadDp       = 16;
static constexpr int  s_kRowHeightDp     = 28;
static constexpr int  s_kRowGapDp        = 8;
static constexpr int  s_kDropdownWidthDp = 240;
static constexpr int  s_kButtonWidthDp   = 90;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DiskPortPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::GetDriveLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskPortPage::GetDriveLabel (const std::string & device)
{
    std::wstring  label = L"UniDisk 3.5";



    if (device == kpszApple35DriveDevice)
    {
        label = L"Apple 3.5 Drive";
    }
    else if (device == kpszDiskIicDevice)
    {
        label = L"5.25-inch drive";
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::DiskPortPage
//
////////////////////////////////////////////////////////////////////////////////

DiskPortPage::DiskPortPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_heading);
    Adopt (m_subtitle);

    for (size_t i = 0; i < kMaxRows; i++)
    {
        Adopt (m_rowDropdowns[i]);
        Adopt (m_removeButtons[i]);

        m_removeButtons[i].SetLabel   (L"Remove");
        m_removeButtons[i].SetOnClick ([this, i] () { RemoveDrive (i); });
        m_rowDropdowns[i].SetSelect   ([this, i] (int choice) { ChooseDrive (i, choice); });
    }

    Adopt (m_addDropdown);

    m_heading.SetText        (L"Drives on the disk port");
    m_heading.SetTextRole    (DxuiTextRole::Heading);
    m_heading.SetFontWeight  (DxuiFontWeight::SemiBold);
    m_subtitle.SetTextRole   (DxuiTextRole::Muted);
    m_subtitle.SetTextAlign  (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_addDropdown.SetSelect  ([this] (int choice) { AddDrive (choice); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::SetPopupHost (DxuiHwndSource * host)
{
    m_addDropdown.SetPopupHost (host);

    for (DxuiComboBox & dropdown : m_rowDropdowns)
    {
        dropdown.SetPopupHost (host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::GetRowChoices
//
//  The row's own kind, and each kind that fits on the chain without the row's
//  drive, in chain order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DiskPortPage::GetRowChoices (size_t row) const
{
    std::vector<std::string>     others  = m_drives;
    std::vector<std::string>     fitting;
    std::vector<std::string>     choices;
    const SettingsMachineInfo  & info    = m_state->GetMachineInfo();



    others.erase (others.begin() + (ptrdiff_t) row);
    fitting = DiskPortChainRule::GetFittingDevices (info.diskPortApple35, info.diskPortFloppies, others);

    for (const char * device : { kpszApple35DriveDevice, kpszUniDisk35Device, kpszDiskIicDevice })
    {
        bool  isOwn  = m_drives[row] == device;
        bool  isFit  = std::find (fitting.begin(), fitting.end(), device) != fitting.end();

        if (isOwn || isFit)
        {
            choices.push_back (device);
        }
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::GetAddChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DiskPortPage::GetAddChoices() const
{
    const SettingsMachineInfo & info = m_state->GetMachineInfo();



    return DiskPortChainRule::GetFittingDevices (info.diskPortApple35, info.diskPortFloppies, m_drives);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::Rebuild
//
//  The subtitle depends on the machine's internal drive, which the ROM
//  numbers ahead of the drives on the port.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::Rebuild()
{
    std::vector<std::wstring>  addItems    = { L"Add a drive" };
    std::vector<bool>          addEnabled  = { false };
    bool                       isPlus      = false;



    m_drives.clear();

    if (m_state != nullptr)
    {
        m_drives = m_state->GetDiskPortDrives();
        isPlus   = m_state->GetMachineInfo().diskPortApple35 > 0;
    }

    m_subtitle.SetText (std::format (L"Plugged into the back of the machine, after the internal {} drive. The port takes up to {} drives.",
                                     isPlus ? L"3.5-inch" : L"5.25-inch", kMaxRows));

    for (size_t row = 0; row < kMaxRows && row < m_drives.size(); row++)
    {
        std::vector<std::string>   choices = GetRowChoices (row);
        std::vector<std::wstring>  items;
        int                        current = 0;



        for (size_t i = 0; i < choices.size(); i++)
        {
            items.push_back (GetDriveLabel (choices[i]));
            current = (choices[i] == m_drives[row]) ? (int) i : current;
        }

        m_rowDropdowns[row].SetItems    (items);
        m_rowDropdowns[row].SetSelected (current);
    }

    for (const std::string & device : (m_state != nullptr) ? GetAddChoices() : std::vector<std::string>())
    {
        addItems.push_back   (GetDriveLabel (device));
        addEnabled.push_back (true);
    }

    m_addDropdown.SetItems        (addItems);
    m_addDropdown.SetItemsEnabled (addEnabled);
    m_addDropdown.SetSelected     (0);
    m_addDropdown.SetVisible      (addItems.size() > 1);

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    m_baseRect = rect;
    m_scaler   = scaler;

    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::Relayout
//
//  The heading and the subtitle, then a row for each drive, its dropdown and
//  its Remove button, then the add row.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::Relayout()
{
    const DxuiDpiScaler  & s       = m_scaler;
    const RECT           & rect    = m_baseRect;
    UINT                   dpi     = s.GetDpi();
    int                    pad     = s.ToPx (s_kPagePadDp);
    int                    rowH    = s.ToPx (s_kRowHeightDp);
    int                    gap     = s.ToPx (s_kRowGapDp);
    int                    dropW   = s.ToPx (s_kDropdownWidthDp);
    int                    buttonW = s.ToPx (s_kButtonWidthDp);
    int                    x       = rect.left + pad;
    int                    right   = rect.right - pad;
    int                    y       = rect.top + pad;
    float                  noteDip = 0.0f;
    int                    noteH   = 2 * rowH;
    HRESULT                hrNote  = S_OK;



    if (rect.right <= rect.left)
    {
        return;
    }

    m_heading.SetDpi  (dpi);
    m_heading.SetRect (MakeRect (x, y, right - x, rowH));
    y += rowH;

    m_subtitle.SetDpi (dpi);
    hrNote = m_subtitle.MeasureWrappedDip ((float) (right - x) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi, noteDip);

    if (SUCCEEDED (hrNote))
    {
        noteH = (int) std::ceil (s.ToPxf (noteDip));
    }

    m_subtitle.SetRect (MakeRect (x, y, right - x, noteH));
    y += noteH + 2 * gap;

    for (size_t row = 0; row < kMaxRows; row++)
    {
        bool  isShown = row < m_drives.size();

        m_rowDropdowns[row].SetDpi      (dpi);
        m_removeButtons[row].SetDpi     (dpi);
        m_rowDropdowns[row].SetVisible  (isShown);
        m_removeButtons[row].SetVisible (isShown);

        if (isShown)
        {
            m_rowDropdowns[row].SetRect  (MakeRect (x, y, dropW, rowH));
            m_removeButtons[row].Layout  (MakeRect (x + dropW + gap, y, buttonW, rowH));
            y += rowH + gap;
        }
    }

    m_addDropdown.SetDpi  (dpi);
    m_addDropdown.SetRect (MakeRect (x, y, dropW, rowH));

    if (m_addDropdown.IsVisible())
    {
        y += rowH + gap;
    }

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (y + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::SetDrives
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::SetDrives (std::vector<std::string> drives)
{
    m_state->SetDiskPortDrives (std::move (drives));
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::ChooseDrive
//
//  Replaces the row's drive with the kind at `choice` in its dropdown.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::ChooseDrive (size_t row, int choice)
{
    std::vector<std::string>  choices;
    std::vector<std::string>  drives = m_drives;



    if (m_state == nullptr || row >= m_drives.size())
    {
        return;
    }

    choices = GetRowChoices (row);

    if (choice < 0 || (size_t) choice >= choices.size() || choices[(size_t) choice] == m_drives[row])
    {
        return;
    }

    drives[row] = choices[(size_t) choice];
    SetDrives (std::move (drives));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::RemoveDrive
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::RemoveDrive (size_t row)
{
    std::vector<std::string>  drives = m_drives;



    if (m_state == nullptr || row >= drives.size())
    {
        return;
    }

    drives.erase (drives.begin() + (ptrdiff_t) row);
    SetDrives (std::move (drives));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage::AddDrive
//
//  Choice 0 is the row's prompt; the others are the kinds that fit, in the
//  order the dropdown lists them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskPortPage::AddDrive (int choice)
{
    std::vector<std::string>  choices;
    std::vector<std::string>  drives = m_drives;



    if (m_state == nullptr || choice < 1)
    {
        return;
    }

    choices = GetAddChoices();

    if ((size_t) choice > choices.size())
    {
        return;
    }

    drives.push_back (choices[(size_t) choice - 1]);
    SetDrives (std::move (drives));
}
