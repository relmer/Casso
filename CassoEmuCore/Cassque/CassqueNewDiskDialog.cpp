#include "Pch.h"

#include "Cassque/CassqueNewDiskDialog.h"
#include "Core/TextEncoding.h"
#include "Ui/FileBrowseModel.h"



//  What each field takes, shown when the pointer rests on it.
static constexpr const wchar_t *  s_kpszNameTip         = L"The new image's file name. The image type's extension is added when you do not type one.";
static constexpr const wchar_t *  s_kpszDos33VolumeTip  = L"DOS 3.3: a volume number from 1 to 254. Leave it blank for 254.";
static constexpr const wchar_t *  s_kpszProDosVolumeTip = L"ProDOS: up to 15 letters, digits and periods, starting with a letter. Leave it blank for NEWDISK.";





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::GetFormatLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassqueNewDiskChoices::GetFormatLabels()
{
    std::vector<std::wstring>  labels;



    for (const Choice & choice : kFormats)
    {
        labels.push_back (choice.label);
    }

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::GetContainerLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassqueNewDiskChoices::GetContainerLabels()
{
    std::vector<std::wstring>  labels;



    for (const Choice & choice : kContainers)
    {
        labels.push_back (choice.label);
    }

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::MakeRequest
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::NewDiskRequest CassqueNewDiskChoices::MakeRequest (int formatIndex, int containerIndex, const std::wstring & volume, bool bootable)
{
    DiskOperations::NewDiskRequest  request;
    int                             formats    = (int) ARRAYSIZE (kFormats);
    int                             containers = (int) ARRAYSIZE (kContainers);



    request.formatName    = kFormats[(formatIndex >= 0 && formatIndex < formats) ? formatIndex : 0].value;
    request.containerType = kContainers[(containerIndex >= 0 && containerIndex < containers) ? containerIndex : 0].value;
    request.volumeName    = TextEncoding::WideToNarrow (volume);
    request.bootable      = bootable && request.formatName != "none";

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::ApplyExtension
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueNewDiskChoices::ApplyExtension (const std::wstring & name, int containerIndex)
{
    int           containers = (int) ARRAYSIZE (kContainers);
    const char  * extension  = kContainers[(containerIndex >= 0 && containerIndex < containers) ? containerIndex : 0].value;



    if (name.rfind (L'.') != std::wstring::npos)
    {
        return name;
    }

    return name + L"." + TextEncoding::NarrowToWide (extension);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::ValidateFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueNewDiskChoices::ValidateFileName (const std::wstring & name, int containerIndex, const std::function<bool (const std::wstring &)> & exists)
{
    std::wstring  error;



    if (name.empty())
    {
        error = L"Enter a file name.";
    }
    else if (!FileBrowseModel::IsValidFileName (name))
    {
        error = L"A file name cannot contain \\ / : * ? \" < > |, end in a period or a space, or be a name Windows reserves.";
    }
    else if (exists && exists (ApplyExtension (name, containerIndex)))
    {
        error = L"A file with this name is already in this folder.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::ValidateVolume
//
//  Blank takes the runner's default for the format. A DOS 3.3 volume is a
//  number from 1 to 254; a ProDOS volume is a name of up to 15 letters, digits
//  and periods, starting with a letter, matched without regard to case. An
//  unformatted image has no volume.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueNewDiskChoices::ValidateVolume (int formatIndex, const std::wstring & volume)
{
    std::wstring  error;
    bool          digits = true;
    bool          name   = true;
    size_t        i      = 0;
    long          number = 0;



    if (volume.empty() || formatIndex == kFormatNone)
    {
        return error;
    }

    for (i = 0; i < volume.size(); i++)
    {
        wchar_t  c        = volume[i];
        bool     isLetter = (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
        bool     isDigit  = (c >= L'0' && c <= L'9');

        digits = digits && isDigit;
        name   = name   && (isLetter || (i > 0 && (isDigit || c == L'.')));
    }

    if (formatIndex == kFormatDos33)
    {
        number = (digits && volume.size() <= kMaxDos33VolumeLength) ? wcstol (volume.c_str(), nullptr, 10) : 0;

        if (number < 1 || number > 254)
        {
            error = L"Enter a volume number from 1 to 254, or leave it blank for 254.";
        }
    }
    else if (!name || volume.size() > kMaxProDosVolumeLength)
    {
        error = L"Use up to 15 letters, digits and periods, starting with a letter, or leave it blank for NEWDISK.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskChoices::GetVolumeMaxLength
//
////////////////////////////////////////////////////////////////////////////////

size_t CassqueNewDiskChoices::GetVolumeMaxLength (int formatIndex)
{
    return (formatIndex == kFormatProDos) ? kMaxProDosVolumeLength : kMaxDos33VolumeLength;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::Init (const Children & children, bool showNameRows)
{
    m_kids         = children;
    m_showNameRows = showNameRows;

    Adopt (*children.nameLabel);
    Adopt (*children.name);
    Adopt (*children.nameError);
    Adopt (*children.containerLabel);
    Adopt (*children.container);
    Adopt (*children.formatLabel);
    Adopt (*children.format);
    Adopt (*children.volumeLabel);
    Adopt (*children.volume);
    Adopt (*children.volumeError);
    Adopt (*children.bootable);

    children.nameLabel->SetVisible (showNameRows);
    children.name->SetVisible (showNameRows);
    children.nameError->SetVisible (showNameRows);
    children.containerLabel->SetVisible (showNameRows);
    children.container->SetVisible (showNameRows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  row    = scaler.ToPx (kRowHeightDip);
    int  gap    = scaler.ToPx (kRowGapDip);
    int  label  = scaler.ToPx (kLabelWidthDip);
    int  err    = scaler.ToPx (kErrorLineDip);
    int  indent = scaler.ToPx (kErrorMarkDip + kErrorGapDip);
    int  y      = boundsDip.top;
    int  fieldX = boundsDip.left + label;



    SetBounds (boundsDip);
    m_scaler = scaler;

    //  A field that can be wrong keeps a line under it for its error, empty or
    //  not, so the form does not jump as errors come and go.
    if (m_showNameRows)
    {
        m_kids.nameLabel->Layout      (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
        m_kids.name->Layout           (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
        m_nameErrorLine = RECT { fieldX, y + row, boundsDip.right, y + row + err };
        m_kids.nameError->Layout      (RECT { fieldX + indent, y + row, boundsDip.right, y + row + err }, scaler);
        y += row + err + gap;

        m_kids.containerLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
        m_kids.container->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
        y += row + gap;
    }

    m_kids.formatLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
    m_kids.format->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
    y += row + gap;

    m_kids.volumeLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
    m_kids.volume->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
    m_volumeErrorLine = RECT { fieldX, y + row, boundsDip.right, y + row + err };
    m_kids.volumeError->Layout (RECT { fieldX + indent, y + row, boundsDip.right, y + row + err }, scaler);
    y += row + err + gap;

    m_kids.bootable->Layout (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::OnMouse
//
//  The open dropdowns first, since their lists reach over the rows below;
//  a press that lands takes focus to the field it hit.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueNewDiskPanel::OnMouse (const DxuiMouseEvent & ev)
{
    IDxuiControl *  fields[] = { m_kids.container, m_kids.format, m_kids.name, m_kids.volume, m_kids.bootable };
    bool            handled  = false;



    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (ev.positionDip.x, ev.positionDip.y);
    }

    for (IDxuiControl * field : fields)
    {
        if (!field->IsVisible())
        {
            continue;
        }

        handled = field->OnMouse (ev);

        if (handled)
        {
            if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
            {
                m_onPressed (field);
            }

            break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::UpdateTooltip
//
//  The field under the pointer shows what it takes; anywhere else, no tip.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::UpdateTooltip (int x, int y)
{
    IDxuiControl *  fields[] = { m_kids.name, m_kids.container, m_kids.format, m_kids.volume, m_kids.bootable };
    int64_t         now      = (int64_t) GetTickCount64();
    POINT           pt       = { x, y };
    std::wstring    tip;



    if (m_tooltip == nullptr || !m_tipFor)
    {
        return;
    }

    for (IDxuiControl * field : fields)
    {
        RECT  bounds = field->GetBounds();

        if (field->IsVisible() && PtInRect (&bounds, pt))
        {
            tip = m_tipFor (field);

            if (!tip.empty())
            {
                m_tooltip->RequestShow (bounds, tip, now);
                return;
            }
        }
    }

    m_tooltip->RequestHide (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiPanel::Paint (painter, text, theme);

    if (m_showNameRows && m_nameBad)
    {
        PaintErrorMark (painter, m_nameErrorLine, theme);
    }

    if (m_volumeBad)
    {
        PaintErrorMark (painter, m_volumeErrorLine, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskPanel::PaintErrorMark
//
//  A red disc with a white cross at the start of the error line, as Windows
//  marks a field in error. Drawn rather than taken from a font, so it needs
//  none installed.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::PaintErrorMark (IDxuiPainter & painter, const RECT & line, const IDxuiTheme & theme) const
{
    float  radius = m_scaler.ToPxf ((float) kErrorMarkDip) * 0.5f;
    float  cx     = (float) line.left + radius;
    float  cy     = (float) (line.top + line.bottom) * 0.5f;
    float  arm    = radius * 0.4f;
    float  thick  = (std::max) (1.0f, m_scaler.ToPxf (1.5f));



    painter.FillCircleApprox (cx, cy, radius, theme.ErrorForeground());
    painter.DrawLineApprox   (cx - arm, cy - arm, cx + arm, cy + arm, thick, 0xFFFFFFFFu);
    painter.DrawLineApprox   (cx - arm, cy + arm, cx + arm, cy - arm, thick, 0xFFFFFFFFu);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskDialog::OnCreate()
{
    CassqueNewDiskPanel::Children  kids;



    for (DxuiLabel * label : { &m_nameLabel, &m_containerLabel, &m_formatLabel, &m_volumeLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    for (DxuiLabel * label : { &m_nameError, &m_volumeError })
    {
        label->SetTextRole  (DxuiTextRole::Error);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_nameLabel.SetText      (L"File name:");
    m_containerLabel.SetText (L"Image type:");
    m_formatLabel.SetText    (L"Format:");
    m_volumeLabel.SetText    (L"Volume:");

    for (DxuiTextInput * input : { &m_name, &m_volume })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetOnChange     ([this] (const std::wstring &) { Revalidate(); });
    }

    //  Typing stops at each field's limit rather than being refused afterward.
    m_name.SetMaxLength   (CassqueNewDiskChoices::kMaxFileNameLength);
    m_volume.SetMaxLength (CassqueNewDiskChoices::GetVolumeMaxLength (CassqueNewDiskChoices::kFormatDos33));

    //  The default name starts selected, so typing replaces it.
    m_name.SetText   (L"New Disk");
    m_name.SelectAll();

    m_container.SetPopupHost (GetPopupHost());
    m_container.SetItems     (CassqueNewDiskChoices::GetContainerLabels());
    m_container.SetSelected  (0);
    m_container.SetSelect    ([this] (int) { Revalidate(); });

    m_format.SetPopupHost (GetPopupHost());
    m_format.SetItems     (CassqueNewDiskChoices::GetFormatLabels());
    m_format.SetSelected  (0);
    m_format.SetSelect    ([this] (int index)
    {
        size_t  limit = CassqueNewDiskChoices::GetVolumeMaxLength (index);

        m_volume.SetMaxLength (limit);

        if (m_volume.GetText().size() > limit)
        {
            m_volume.SetText (m_volume.GetText().substr (0, limit));
        }

        Revalidate();
    });

    m_bootable.SetSingleLineLabel (true);

    kids.nameLabel      = &m_nameLabel;
    kids.name           = &m_name;
    kids.nameError      = &m_nameError;
    kids.containerLabel = &m_containerLabel;
    kids.container      = &m_container;
    kids.formatLabel    = &m_formatLabel;
    kids.format         = &m_format;
    kids.volumeLabel    = &m_volumeLabel;
    kids.volume         = &m_volume;
    kids.volumeError    = &m_volumeError;
    kids.bootable       = &m_bootable;

    m_body = CreateDialogContent<CassqueNewDiskPanel>();
    m_body->Init (kids, !m_formatMode);
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    m_tooltip.SetPopupHost (GetPopupHost());

    if (m_theme != nullptr)
    {
        m_tooltip.SetTheme (*m_theme);
    }

    m_body->SetTooltip (&m_tooltip, [this] (IDxuiControl * field) -> std::wstring
    {
        std::wstring  tip;

        if (field == &m_name)
        {
            tip = s_kpszNameTip;
        }
        else if (field == &m_volume)
        {
            tip = (m_format.GetSelectedIndex() == CassqueNewDiskChoices::kFormatProDos) ? s_kpszProDosVolumeTip : s_kpszDos33VolumeTip;
        }

        return tip;
    });

    m_ok = AddDialogButton (m_formatMode ? L"Format" : L"Create", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_ok->SetOnClick ([this]()
    {
        m_outcome.confirmed = true;
        m_outcome.fileName  = CassqueNewDiskChoices::ApplyExtension (m_name.GetText(), m_container.GetSelectedIndex());
        m_outcome.request   = CassqueNewDiskChoices::MakeRequest (m_format.GetSelectedIndex(), m_container.GetSelectedIndex(),
                                                                  m_volume.GetText(), m_bootable.IsChecked());
        EndDialog (IDOK);
    });

    //  The tooltip opens and closes on a timer the dialog keeps.
    SetDialogTickIntervalMs (50);

    Revalidate();

    SetInitialFocus (m_formatMode ? (IDxuiControl *) &m_format : (IDxuiControl *) &m_name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

CassqueNewDiskDialog::Outcome CassqueNewDiskDialog::Ask (HWND owner, const IDxuiTheme * theme, bool formatMode, const ExistsFn & exists)
{
    HRESULT                   hr         = S_OK;
    CassqueNewDiskDialog      dialog;
    DxuiWindow::CreateParams  params;
    int                       rows       = formatMode ? 3 : 5;
    int                       errorLines = formatMode ? 1 : 2;



    dialog.m_theme      = theme;
    dialog.m_formatMode = formatMode;
    dialog.m_exists     = exists;

    params.title                    = formatMode ? L"Format Disk Image" : L"New Disk Image";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 460, 130 + rows * (CassqueNewDiskPanel::kRowHeightDip + CassqueNewDiskPanel::kRowGapDip)
                                                 + errorLines * CassqueNewDiskPanel::kErrorLineDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return Outcome();
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    //  Its popup goes back to this dialog's pool before the dialog closes it.
    dialog.m_tooltip.HideImmediate();

    return dialog.m_outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog::Revalidate
//
//  Run on every change to any field. A format with no volume takes the volume
//  field out of use rather than checking it.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskDialog::Revalidate()
{
    int           format      = m_format.GetSelectedIndex();
    std::wstring  nameError   = m_formatMode ? std::wstring() : CassqueNewDiskChoices::ValidateFileName (m_name.GetText(), m_container.GetSelectedIndex(), m_exists);
    std::wstring  volumeError = CassqueNewDiskChoices::ValidateVolume (format, m_volume.GetText());



    m_nameError.SetText   (nameError);
    m_volumeError.SetText (volumeError);

    m_volume.SetEnabled     (format != CassqueNewDiskChoices::kFormatNone);
    m_volume.SetPlaceholder ((format == CassqueNewDiskChoices::kFormatProDos) ? L"NEWDISK"
                           : (format == CassqueNewDiskChoices::kFormatDos33)  ? L"254"
                                                                               : L"");

    if (m_body != nullptr)
    {
        m_body->SetErrorMarks (!nameError.empty(), !volumeError.empty());
    }

    if (m_ok != nullptr)
    {
        m_ok->SetEnabled (nameError.empty() && volumeError.empty());
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskDialog::OnDialogTick()
{
    m_tooltip.Tick ((int64_t) GetTickCount64());
}
