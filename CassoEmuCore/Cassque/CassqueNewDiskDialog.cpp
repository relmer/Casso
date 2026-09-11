#include "Pch.h"

#include "Cassque/CassqueNewDiskDialog.h"
#include "Core/TextEncoding.h"





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
//  CassqueNewDiskPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskPanel::Init (const Children & children, bool showNameRows)
{
    m_kids         = children;
    m_showNameRows = showNameRows;

    Adopt (*children.nameLabel);
    Adopt (*children.name);
    Adopt (*children.containerLabel);
    Adopt (*children.container);
    Adopt (*children.formatLabel);
    Adopt (*children.format);
    Adopt (*children.volumeLabel);
    Adopt (*children.volume);
    Adopt (*children.bootable);

    children.nameLabel->SetVisible (showNameRows);
    children.name->SetVisible (showNameRows);
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
    int  y      = boundsDip.top;
    int  fieldX = boundsDip.left + label;



    SetBounds (boundsDip);

    if (m_showNameRows)
    {
        m_kids.nameLabel->Layout      (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
        m_kids.name->Layout           (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
        y += row + gap;

        m_kids.containerLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
        m_kids.container->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
        y += row + gap;
    }

    m_kids.formatLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
    m_kids.format->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
    y += row + gap;

    m_kids.volumeLabel->Layout (RECT { boundsDip.left, y, fieldX, y + row }, scaler);
    m_kids.volume->Layout      (RECT { fieldX, y, boundsDip.right, y + row }, scaler);
    y += row + gap;

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
//  CassqueNewDiskDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassqueNewDiskDialog::OnCreate()
{
    CassqueNewDiskPanel::Children  kids;
    DxuiButton *                   ok = nullptr;



    for (DxuiLabel * label : { &m_nameLabel, &m_containerLabel, &m_formatLabel, &m_volumeLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
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
        input->SetMaxLength    (64);
    }

    m_name.SetText (L"New Disk");
    m_volume.SetPlaceholder (L"254, or a ProDOS volume name");

    m_container.SetPopupHost (GetPopupHost());
    m_container.SetItems     (CassqueNewDiskChoices::GetContainerLabels());
    m_container.SetSelected  (0);

    m_format.SetPopupHost (GetPopupHost());
    m_format.SetItems     (CassqueNewDiskChoices::GetFormatLabels());
    m_format.SetSelected  (0);

    m_bootable.SetSingleLineLabel (true);

    kids.nameLabel      = &m_nameLabel;
    kids.name           = &m_name;
    kids.containerLabel = &m_containerLabel;
    kids.container      = &m_container;
    kids.formatLabel    = &m_formatLabel;
    kids.format         = &m_format;
    kids.volumeLabel    = &m_volumeLabel;
    kids.volume         = &m_volume;
    kids.bootable       = &m_bootable;

    m_body = CreateDialogContent<CassqueNewDiskPanel>();
    m_body->Init (kids, !m_formatMode);
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    ok = AddDialogButton (m_formatMode ? L"Format" : L"Create", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    ok->SetOnClick ([this]()
    {
        m_outcome.confirmed = true;
        m_outcome.fileName  = CassqueNewDiskChoices::ApplyExtension (m_name.GetText(), m_container.GetSelectedIndex());
        m_outcome.request   = CassqueNewDiskChoices::MakeRequest (m_format.GetSelectedIndex(), m_container.GetSelectedIndex(),
                                                                  m_volume.GetText(), m_bootable.IsChecked());
        EndDialog (IDOK);
    });

    SetInitialFocus (m_formatMode ? (IDxuiControl *) &m_format : (IDxuiControl *) &m_name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueNewDiskDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

CassqueNewDiskDialog::Outcome CassqueNewDiskDialog::Ask (HWND owner, const IDxuiTheme * theme, bool formatMode)
{
    HRESULT                   hr     = S_OK;
    CassqueNewDiskDialog      dialog;
    DxuiWindow::CreateParams  params;
    int                       rows   = formatMode ? 3 : 5;



    dialog.m_theme      = theme;
    dialog.m_formatMode = formatMode;

    params.title                    = formatMode ? L"Format Disk Image" : L"New Disk Image";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 460, 130 + rows * (CassqueNewDiskPanel::kRowHeightDip + CassqueNewDiskPanel::kRowGapDip) };
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

    return dialog.m_outcome;
}
