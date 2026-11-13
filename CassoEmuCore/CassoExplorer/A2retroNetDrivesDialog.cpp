#include "Pch.h"

#include "CassoExplorer/A2retroNetDrivesDialog.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Seams/IShellIcons.h"





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDriveRow::Init
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDriveRow::Init (int number, std::shared_ptr<const DxuiIconImage> icon, DxuiHwndSource * popupHost)
{
    m_icon = std::move (icon);

    m_number.SetText      (std::to_wstring (number));
    m_number.SetTextRole  (DxuiTextRole::Muted);
    m_number.SetTextAlign (DxuiTextHAlign::Right, DxuiTextVAlign::Center);

    m_image.SetPopupHost (popupHost);

    m_status.SetTextRole  (DxuiTextRole::Muted);
    m_status.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    Adopt (m_number);
    Adopt (m_image);
    Adopt (m_status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDriveRow::Layout
//
//  The number, the icon and the dropdown on a line, and the status below
//  the dropdown.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDriveRow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  field   = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  gap     = scaler.ToPx (kGapDip);
    int  icon    = scaler.ToPx (kIconDip);
    int  number  = boundsDip.left + scaler.ToPx (kNumberDip);
    int  boxLeft = number + gap + icon + gap;



    SetBounds (boundsDip);

    m_iconRect = RECT { number + gap, boundsDip.top + (field - icon) / 2, number + gap + icon, boundsDip.top + (field + icon) / 2 };

    m_number.Layout (RECT { boundsDip.left, boundsDip.top, number, boundsDip.top + field }, scaler);
    m_image.Layout  (RECT { boxLeft, boundsDip.top, boundsDip.right, boundsDip.top + field }, scaler);
    m_status.Layout (RECT { boxLeft, boundsDip.top + field, boundsDip.right, boundsDip.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDriveRow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDriveRow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr = S_OK;



    if (m_icon && !m_icon->bgraPremul.empty())
    {
        hr = text.DrawIconBitmap (m_icon->bgraPremul.data(), m_icon->width, m_icon->height,
                                  (float) m_iconRect.left, (float) m_iconRect.top, (float) (m_iconRect.right - m_iconRect.left), (float) (m_iconRect.bottom - m_iconRect.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesPanel::Init (const Parts & parts)
{
    m_parts = parts;

    Adopt (*parts.numberLabel);
    Adopt (*parts.number);
    Adopt (*parts.delayLabel);
    Adopt (*parts.delay);
    Adopt (*parts.drives);
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
//  The settings' dropdowns first, then the drives, whose stack takes a
//  drag anywhere once a gripper is pressed.
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetDrivesPanel::OnMouse (const DxuiMouseEvent & ev)
{
    bool  taken = false;



    taken = (m_parts.drives->GetDraggedRow() < 0 && (m_parts.number->OnMouse (ev) || m_parts.delay->OnMouse (ev))) || m_parts.drives->OnMouse (ev);

    if (taken && m_handled)
    {
        m_handled();
    }

    return taken;
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
//  Returns the height of the stack: the two settings on a line, the drives,
//  and two lines for the notice.
//
////////////////////////////////////////////////////////////////////////////////

int A2retroNetDrivesPanel::Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place)
{
    int  field  = scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  line   = scaler.ToPx (ExplorerMetrics::kStackLabelHeightDip);
    int  gap    = scaler.ToPx (kSectionGapDip);
    int  labelW = scaler.ToPx (kLabelWidthDip);
    int  half   = (bounds.left + bounds.right) / 2;
    int  drives = m_parts.drives->MeasureHeightPx (scaler);
    int  top    = bounds.top;



    if (place)
    {
        m_parts.numberLabel->Layout (RECT { bounds.left, top, bounds.left + labelW, top + field }, scaler);
        m_parts.number->Layout      (RECT { bounds.left + labelW, top, half - gap, top + field }, scaler);
        m_parts.delayLabel->Layout  (RECT { half, top, half + labelW, top + field }, scaler);
        m_parts.delay->Layout       (RECT { half + labelW, top, bounds.right, top + field }, scaler);
        m_parts.drives->Layout      (RECT { bounds.left, top + field + gap, bounds.right, top + field + gap + drives }, scaler);
        m_parts.notice->Layout      (RECT { bounds.left, top + field + gap + drives + gap, bounds.right, top + field + gap + drives + gap + line * 2 }, scaler);
    }

    top += field + gap + drives + gap + line * 2;

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
                                   IShellIcons               * icons,
                                   EditAsTextFn                editAsText,
                                   ActionsTypes::Outcome     & outOutcome)
{
    HRESULT                   hr = S_OK;
    A2retroNetDrivesDialog    dialog (actions, root, icons, std::move (editAsText));
    DxuiWindow::CreateParams  params;



    params.title                    = L"A2retroNET drives";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 520, 720 };
    params.minSizeDip               = { 440, 600 };
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
    A2retroNetDrivesPanel::Parts           parts;
    std::vector<IDxuiControl *>            rows;
    std::vector<std::wstring>              numbers;
    std::vector<std::wstring>              delays;
    std::shared_ptr<const DxuiIconImage>   icon;
    DxuiButton                           * save   = nullptr;
    DxuiButton                           * cancel = nullptr;
    DxuiButton                           * text   = nullptr;



    if (m_icons != nullptr)
    {
        icon = m_icons->GetForKind (IShellIcons::Kind::DiskImage);
    }

    for (int drive = 0; drive < A2retroNetConfig::kDrivesMax; drive++)
    {
        m_rows[drive].Init (drive + 1, icon, GetPopupHost());
        m_rows[drive].GetImageBox().SetSelect ([this, drive] (int index) { ChooseImage (drive, index); });

        rows.push_back (&m_rows[drive]);
    }

    m_drives.SetRowHeightDip (A2retroNetDriveRow::kHeightDip);
    m_drives.SetRows         (rows);
    m_drives.SetOnReorder    ([this] (size_t from, size_t to) { MoveDrive (from, to); });

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

    parts = { &m_numberLabel, &m_numberBox, &m_delayLabel, &m_delayBox, &m_drives, &m_notice };

    Load();

    m_body = CreateDialogContent<A2retroNetDrivesPanel>();
    m_body->Init (parts);
    m_body->SetOnMouseHandled ([this]() { UpdateTick(); Invalidate(); });

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
//  While a drive moves, each tick repaints. Otherwise, a save in another
//  program, as Edit as text opens, changes the file's size or time, and the
//  file is read again, unless the dialog has unsaved changes, which Save
//  would write over the other program's.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::OnDialogTick()
{
    FileStamp  stamp;
    HRESULT    hr      = S_OK;
    bool       changed = false;



    UpdateTick();

    if (m_sliding)
    {
        Invalidate();
        return;
    }

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
//  A2retroNetDrivesDialog::UpdateTick
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::UpdateTick()
{
    bool  sliding = m_drives.IsAnimating();



    if (sliding != m_sliding)
    {
        m_sliding = sliding;
        SetDialogTickIntervalMs (sliding ? kSlideTickMs : kIdleTickMs);
    }
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

    m_drives.SetMovableCount ((size_t) m_config.number);
    m_numberBox.SetSelected  (m_config.number / 2 - 1);
    m_delayBox.SetSelected   (m_config.bootDelay);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::FillDrive
//
//  A drive in use has no image first, then each image on the card, and the
//  drive's image and its commented-out images when they aren't among them,
//  as a path on the other device is not. A drive past the number in use is
//  not configured, and is disabled.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::FillDrive (int drive)
{
    std::vector<std::string>  & choices = m_choices[drive];
    std::vector<std::wstring>   labels;
    ActionsA2retroNet::Drive    status;
    DxuiComboBox              & box     = m_rows[drive].GetImageBox();
    DxuiLabel                 & under   = m_rows[drive].GetStatus();
    const std::string         & current = m_config.paths[drive];
    int                         index   = 0;



    if (drive >= m_config.number)
    {
        choices.clear();

        box.SetItems    ({ kpszNotConfigured });
        box.SetSelected (0);
        box.SetEnabled  (false);
        under.SetText   (std::wstring());
        return;
    }

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
        labels.push_back (path.empty() ? std::wstring (kpszNoImage) : TextEncoding::NarrowToWide (path));
    }

    index  = (int) (std::find (choices.begin(), choices.end(), current) - choices.begin());
    status = m_actions.CheckDrive (m_root, drive + 1, current);

    box.SetItems    (labels);
    box.SetSelected (index);
    box.SetEnabled  (true);

    if (!status.problem.empty())
    {
        under.SetText     (status.problem);
        under.SetTextRole (DxuiTextRole::Error);
    }
    else
    {
        under.SetText     (current.empty() ? std::wstring() : std::format (L"{}{}", TextEncoding::NarrowToWide (PartitionAccess::FormatSize (status.bytes)), status.isWriteProtected ? L", write-protected" : L""));
        under.SetTextRole (DxuiTextRole::Muted);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::ChooseImage
//
//  An image already on another drive is taken from that drive, so every
//  drive is filled again.
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::ChooseImage (int drive, int index)
{
    if (index < 0 || (size_t) index >= m_choices[drive].size() || m_choices[drive][(size_t) index] == m_config.paths[drive])
    {
        return;
    }

    m_config.SetDrive (drive, m_choices[drive][(size_t) index]);
    m_dirty = true;

    Fill();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog::MoveDrive
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetDrivesDialog::MoveDrive (size_t from, size_t to)
{
    m_config.MoveDrive ((int) from, (int) to);
    m_dirty = true;

    for (int drive = 0; drive < A2retroNetConfig::kDrivesMax; drive++)
    {
        FillDrive (drive);
    }
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
