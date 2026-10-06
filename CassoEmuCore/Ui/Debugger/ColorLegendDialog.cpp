#include "Pch.h"

#include "Ui/Debugger/ColorLegendDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void ColorLegendPanel::Init (DxuiListView & list)
{
    m_list = &list;
    Adopt (list);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ColorLegendPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    if (m_list != nullptr)
    {
        m_list->SetDpi (scaler.GetDpi());
        m_list->Layout (boundsDip, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendPanel::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool ColorLegendPanel::OnMouse (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = {};



    if (m_list == nullptr)
    {
        return false;
    }

    bounds            = m_list->GetBounds();
    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };

    return m_list->OnMouse (local);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendDialog::MakeRows
//
//  A text color is shown on a sample of text, a row's fill behind one, the
//  PC's arrow as itself, and every other color as a shape. A pane's heading
//  is drawn as the watch pane draws its own: the heading color on a light
//  tint of the hover color.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> ColorLegendDialog::MakeRows (const ColorLegend::Palette & palette, const IDxuiTheme & theme)
{
    constexpr float                                kHeadingTint = 0.35f;
    constexpr const wchar_t                      * kSample      = L"Ab";
    std::vector<std::vector<DxuiListView::Cell>>   rows;
    const wchar_t                                * group        = nullptr;
    uint32_t                                       heading      = DxuiColor::Mix (theme.ContentBackground(), theme.HoverBackground(), kHeadingTint);



    for (const ColorLegend::Entry & entry : ColorLegend::GetEntries())
    {
        DxuiListView::Cell  swatch;
        DxuiListView::Cell  meaning = { ColorLegend::GetText (entry.meaning) };
        uint32_t            argb    = ColorLegend::GetArgb (entry.meaning, palette);

        if (group == nullptr || std::wstring (group) != entry.group)
        {
            DxuiListView::Cell  label = { entry.group };

            group            = entry.group;
            label.argb       = theme.HeadingForeground();
            label.face       = DxuiTheme::kBodyFace;
            label.background = heading;
            label.spansRow   = true;

            rows.push_back ({ label, DxuiListView::Cell {} });
        }

        switch (entry.swatch)
        {
        case ColorLegend::Swatch::Row:
            swatch.text       = kSample;
            swatch.argb       = theme.Foreground();
            swatch.background = argb;
            break;

        case ColorLegend::Swatch::Text:
            swatch.text = kSample;
            swatch.argb = argb;
            break;

        case ColorLegend::Swatch::Marker:
            swatch.text = s_kpszTriangleRight;
            swatch.argb = argb;
            break;

        default:
            swatch.icon = ColorLegend::MakeSwatchIcon (entry.swatch, argb);
            break;
        }

        meaning.face = DxuiTheme::kBodyFace;
        rows.push_back ({ swatch, meaning });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendDialog::Open
//
//  Made once, then shown again each time it is asked for. Closing it hides
//  it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ColorLegendDialog::Open (HWND owner, const IDxuiTheme * theme, const ColorLegend::Palette & palette)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    m_theme   = theme;
    m_palette = palette;

    if (!IsCreated())
    {
        params.title                    = L"Colors";
        params.hInstance                = GetModuleHandleW (nullptr);
        params.ownerHwnd                = owner;
        params.initialSizeDip           = { kWidthDip, kHeightDip };
        params.minSizeDip               = { kWidthDip / 2, kHeightDip / 2 };
        params.resizable                = true;
        params.insetContentBelowCaption = true;
        params.captionStyle             = DxuiCaptionStyle::CloseOnly;
        params.placement                = DxuiWindowPlacement::CenteredOnOwner;

        hr = Create (params);
        CHR (hr);

        SetOnDialogEnd ([this] (int) { m_isShown = false; });
    }

    SetColors (theme, palette);
    ShowModelessDialog (IDOK);
    m_isShown = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendDialog::SetColors
//
////////////////////////////////////////////////////////////////////////////////

void ColorLegendDialog::SetColors (const IDxuiTheme * theme, const ColorLegend::Palette & palette)
{
    m_theme   = theme;
    m_palette = palette;

    if (theme == nullptr)
    {
        return;
    }

    m_list.SetTheme (theme);
    m_list.SetRows  (MakeRows (palette, *theme));

    if (IsCreated())
    {
        SetTheme (theme);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorLegendDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void ColorLegendDialog::OnCreate()
{
    std::vector<DxuiListView::Column>  columns (2);



    columns[0].widthDip = kSwatchColumnDip;
    columns[1].stretch  = true;

    m_list.SetColumns    (std::move (columns));
    m_list.SetShowHeader (false);

    m_body = CreateDialogContent<ColorLegendPanel>();
    m_body->Init (m_list);

    AddDialogButton (L"Close", IDOK);
}
