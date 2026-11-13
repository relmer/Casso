#include "Pch.h"

#include "CassoExplorer/A2retroNetDrivesDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesPanel::Init (const Parts & parts)
{
    m_parts = parts;

    for (const Row & row : parts.rows)
    {
        Adopt (*row.name);
        Adopt (*row.image);
        Adopt (*row.up);
        Adopt (*row.status);
    }

    Adopt (*parts.numberLabel);
    Adopt (*parts.number);
    Adopt (*parts.delayLabel);
    Adopt (*parts.delay);
    Adopt (*parts.notice);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_scaler = scaler;

    Arrange (boundsDip, scaler, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetDrivesPanel::OnMouse (const DxuiMouseEvent & ev)
{
    for (const Row & row : m_parts.rows)
    {
        for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { row.image, row.up })
        {
            if (control->IsEnabled() && control->OnMouse (ev))
            {
                return true;
            }
        }
    }

    return m_parts.number->OnMouse (ev) || m_parts.delay->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetDrivesPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { 0, 0, widthPx, 0 };



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    heightPx = Arrange (bounds, m_scaler, false);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::Arrange
//
//  Returns the height of the stack. Each drive is a line of its number, its
//  dropdown and its button, with its status below the dropdown.
//
////////////////////////////////////////////////////////////////////////////////

int A2retroNetDrivesPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int  nearGap  = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  gap      = scaler.ToPx (kSettingsGapDip);
    int  field    = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  line     = scaler.ToPx (ExplorerMetrics::kStackLabelHeightDip);
    int  nameW    = scaler.ToPx (kNameWidthDip);
    int  upW      = scaler.ToPx (kUpWidthDip);
    int  boxLeft  = bounds.left + nameW;
    int  boxRight = bounds.right - upW - nearGap;
    int  half     = (bounds.right - bounds.left) / 2;
    int  top      = bounds.top;



    for (const Row & row : m_parts.rows)
    {
        if (place)
        {
            row.name->Layout   (RECT { bounds.left, top, boxLeft, top + field }, scaler);
            row.image->Layout  (RECT { boxLeft, top, boxRight, top + field }, scaler);
            row.up->Layout     (RECT { bounds.right - upW, top, bounds.right, top + field }, scaler);
            row.status->Layout (RECT { boxLeft, top + field, boxRight, top + field + line }, scaler);
        }

        top += field + line + nearGap;
    }

    top += gap - nearGap;

    if (place)
    {
        m_parts.numberLabel->Layout (RECT { bounds.left, top, bounds.left + nameW * 2, top + field }, scaler);
        m_parts.number->Layout      (RECT { bounds.left + nameW * 2, top, bounds.left + half - nearGap, top + field }, scaler);
        m_parts.delayLabel->Layout  (RECT { bounds.left + half, top, bounds.left + half + nameW * 2, top + field }, scaler);
        m_parts.delay->Layout       (RECT { bounds.left + half + nameW * 2, top, bounds.right, top + field }, scaler);
        m_parts.notice->Layout      (RECT { bounds.left, top + field + gap, bounds.right, top + field + gap + line * 2 }, scaler);
    }

    top += field + gap + line * 2;

    return top - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::Edit
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetDrivesDialog::Edit (HWND                        owner,
                                   const IDxuiTheme          * theme,
                                   ActionsA2retroNet         & actions,
                                   const std::wstring        & root,
                                   EditAsTextFn                editAsText,
                                   ActionsTypes::Outcome     & outOutcome)
{
    HRESULT                   hr = S_OK;
    A2retroNetDrivesDialog    dialog (actions, root, std::move (editAsText));
    DxuiWindow::CreateParams  params;



    params.title                    = L"A2retroNET drives";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 560, 640 };
    params.minSizeDip               = { 460, 520 };
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

    outOutcome = dialog.m_outcome;

    return dialog.m_saved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::OnCreate()
{
    A2retroNetDrivesPanel::Parts  parts;
    std::vector<std::wstring>     numbers;
    std::vector<std::wstring>     delays;
    DxuiButton                  * save   = nullptr;
    DxuiButton                  * cancel = nullptr;
    DxuiButton                  * text   = nullptr;



    for (int drive = 0; drive < A2retroNetConfig::kDrivesMax; drive++)
    {
        m_names[drive].SetText (std::format (L"Drive {}", drive + 1));
        ExplorerTextStyles::Apply (m_names[drive], ExplorerTextStyles::kFormLabel);
        m_names[drive].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

        m_imageBoxes[drive].SetPopupHost (GetPopupHost());
        m_imageBoxes[drive].SetSelect ([this, drive] (int index) { ChooseImage (drive, index); });

        m_upButtons[drive].SetLabel   (L"Move up");
        m_upButtons[drive].SetEnabled (drive > 0);
        m_upButtons[drive].SetOnClick ([this, drive]() { MoveUp (drive); });

        m_statuses[drive].SetTextRole  (DxuiTextRole::Muted);
        m_statuses[drive].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

        parts.rows[drive] = { &m_names[drive], &m_imageBoxes[drive], &m_upButtons[drive], &m_statuses[drive] };
    }

    for (int count = 2; count <= A2retroNetConfig::kDrivesMax; count += 2)
    {
        numbers.push_back (std::to_wstring (count));
    }

    for (int seconds = 0; seconds <= A2retroNetConfig::kBootDelayMax; seconds++)
    {
        delays.push_back (std::format (L"{} second{}", seconds, seconds == 1 ? L"" : L"s"));
    }

    for (DxuiLabel * label : { &m_numberLabel, &m_delayLabel })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormLabel);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_numberLabel.SetText (L"Drives in use:");
    m_delayLabel.SetText  (L"Boot delay:");

    m_numberBox.SetPopupHost (GetPopupHost());
    m_numberBox.SetItems     (numbers);
    m_numberBox.SetSelect    ([this] (int index) { ChooseNumber (index); });

    m_delayBox.SetPopupHost (GetPopupHost());
    m_delayBox.SetItems     (delays);
    m_delayBox.SetSelect    ([this] (int index) { ChooseDelay (index); });

    m_notice.SetTextRole  (DxuiTextRole::Error);
    m_notice.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    parts.numberLabel = &m_numberLabel;
    parts.number      = &m_numberBox;
    parts.delayLabel  = &m_delayLabel;
    parts.delay       = &m_delayBox;
    parts.notice      = &m_notice;

    Load();

    m_body = CreateDialogContent<A2retroNetDrivesPanel>();
    m_body->Init (parts);

    text   = AddDialogButton (L"Edit as text", 0, DxuiButtonRow::Anchor::Left);
    save   = AddDialogButton (L"Save", IDOK);
    cancel = AddDialogButton (L"Cancel", IDCANCEL);

    text->SetOnClick ([this]()
    {
        std::wstring  path = (std::filesystem::path (m_root) / A2retroNetConfig::kpszWideFileName).wstring();

        if (m_editAsText)
        {
            m_editAsText (path);
        }
    });

    save->SetVariant (DxuiButton::Variant::Primary);
    save->SetOnClick ([this]()
    {
        m_outcome = m_actions.Save (m_root, m_config);
        m_saved   = m_outcome.Succeeded();

        EndDialog (IDOK);
    });

    SetInitialFocus (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::OnDialogTick
//
//  A save in another program, as Edit as text opens, changes the file's
//  size or time. The file is then read again, unless the dialog has
//  unsaved changes, which Save would write over the other program's.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::OnDialogTick()
{
    FileStamp  stamp;
    HRESULT    hr      = S_OK;
    bool       changed = false;



    hr      = m_actions.GetConfigStamp (m_root, stamp);
    changed = SUCCEEDED (hr) && (stamp.sizeBytes != m_stamp.sizeBytes || stamp.modifiedUnix != m_stamp.modifiedUnix);

    if (!changed)
    {
        return;
    }

    if (m_dirty)
    {
        m_notice.SetText (L"A2retroNET.txt was changed in another program. Save replaces those changes with the ones here.");
        m_stamp = stamp;
    }
    else
    {
        Load();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::Load
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::Load()
{
    ActionsA2retroNet::Card  card;
    HRESULT                  hr = S_OK;



    hr = m_actions.ReadCard (m_root, card);

    if (FAILED (hr))
    {
        m_notice.SetText (L"A2retroNET.txt could not be read.");
        return;
    }

    hr = m_actions.GetConfigStamp (m_root, m_stamp);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = m_actions.ListImages (m_root, m_images);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_config = card.config;
    m_dirty  = false;

    m_notice.SetText (m_config.hasCarriageReturns ? L"A2retroNET.txt has CR LF line ends, so the A2Pico doesn't open any image. Save writes it with LF line ends." : L"");

    Fill();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::Fill
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::Fill()
{
    for (int drive = 0; drive < A2retroNetConfig::kDrivesMax; drive++)
    {
        FillDrive (drive);
    }

    m_numberBox.SetSelected (m_config.number / 2 - 1);
    m_delayBox.SetSelected  (m_config.bootDelay);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::FillDrive
//
//  The dropdown has no image first, then each image on the card, and the
//  drive's image and its commented-out images when they aren't among them,
//  as a path on the other device is not.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::FillDrive (int drive)
{
    std::vector<std::string>  & choices = m_choices[drive];
    std::vector<std::wstring>   labels;
    ActionsA2retroNet::Drive    status;
    const std::string         & current = m_config.paths[drive];
    bool                        inUse   = drive < m_config.number;
    int                         index   = 0;



    choices = { std::string() };
    choices.insert (choices.end(), m_images.begin(), m_images.end());

    for (const std::string & path : m_config.GetAlternatives (drive))
    {
        if (std::find (choices.begin(), choices.end(), path) == choices.end())
        {
            choices.push_back (path);
        }
    }

    if (std::find (choices.begin(), choices.end(), current) == choices.end())
    {
        choices.push_back (current);
    }

    for (const std::string & path : choices)
    {
        labels.push_back (path.empty() ? std::wstring (L"No image") : TextEncoding::NarrowToWide (path));
    }

    index = (int) (std::find (choices.begin(), choices.end(), current) - choices.begin());

    m_imageBoxes[drive].SetItems    (labels);
    m_imageBoxes[drive].SetSelected (index);
    m_imageBoxes[drive].SetEnabled  (inUse);
    m_upButtons[drive].SetEnabled   (inUse && drive > 0);

    status = m_actions.CheckDrive (m_root, drive + 1, current);

    //  A drive past the number in use is dimmed, with no status.
    if (!inUse)
    {
        m_statuses[drive].SetText (std::wstring());
    }
    else if (!status.problem.empty())
    {
        m_statuses[drive].SetText     (status.problem);
        m_statuses[drive].SetTextRole (DxuiTextRole::Error);
    }
    else
    {
        m_statuses[drive].SetText     (current.empty() ? std::wstring() : std::format (L"{}{}", TextEncoding::NarrowToWide (PartitionAccess::FormatSize (status.bytes)), status.isWriteProtected ? L", write-protected" : L""));
        m_statuses[drive].SetTextRole (DxuiTextRole::Muted);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::ChooseImage
//
//  An image commented out for the drive is switched to, so the image it
//  replaces is commented out in its place.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::ChooseImage (int drive, int index)
{
    std::vector<std::string>  alternatives  = m_config.GetAlternatives (drive);
    std::string               path;
    bool                      isAlternative = false;



    if (index < 0 || (size_t) index >= m_choices[drive].size() || m_choices[drive][(size_t) index] == m_config.paths[drive])
    {
        return;
    }

    path          = m_choices[drive][(size_t) index];
    isAlternative = std::find (alternatives.begin(), alternatives.end(), path) != alternatives.end();

    if (!isAlternative || !m_config.SwitchToAlternative (drive, path))
    {
        m_config.paths[drive] = path;
    }

    m_dirty = true;

    FillDrive (drive);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::MoveUp
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::MoveUp (int drive)
{
    if (drive <= 0 || drive >= m_config.number)
    {
        return;
    }

    m_config.SwapDrives (drive - 1, drive);
    m_dirty = true;

    FillDrive (drive - 1);
    FillDrive (drive);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::ChooseNumber
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::ChooseNumber (int index)
{
    m_config.number = (index + 1) * 2;
    m_dirty         = true;

    Fill();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::ChooseDelay
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::ChooseDelay (int index)
{
    m_config.bootDelay = index;
    m_dirty            = true;
}
