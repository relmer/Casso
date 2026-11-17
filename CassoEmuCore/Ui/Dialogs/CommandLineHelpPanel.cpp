#include "Pch.h"

#include "Ui/Dialogs/CommandLineHelpPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::Init
//
//  Builds one label per piece of text. The refusal label exists only when
//  there is a refusal, and each option row has a switch label in the
//  monospace face and a description label in the body face.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::Init (const Content & content)
{
    RowLabels  labels;



    m_content = content;

    m_title    = AddLabel (m_content.title,    kTitleFontDip,    DxuiFontWeight::Bold,   DxuiTextRole::Heading);
    m_subtitle = AddLabel (m_content.subtitle, kSubtitleFontDip, DxuiFontWeight::Normal, DxuiTextRole::Body);
    m_version  = AddLabel (m_content.version,  kVersionFontDip,  DxuiFontWeight::Normal, DxuiTextRole::Muted);

    if (!m_content.refusal.empty())
    {
        m_refusal = AddLabel (m_content.refusal, kBodyFontDip, DxuiFontWeight::SemiBold, DxuiTextRole::Error);
    }

    m_usageHeading = AddLabel (L"Usage", kHeadingFontDip, DxuiFontWeight::SemiBold, DxuiTextRole::Heading);
    m_usage        = AddLabel (m_content.usage, kBodyFontDip, DxuiFontWeight::Normal, DxuiTextRole::Body);
    m_usage->SetFontFace (DxuiTheme::kMonoFace);

    m_optionsHeading = AddLabel (L"Options", kHeadingFontDip, DxuiFontWeight::SemiBold, DxuiTextRole::Heading);

    for (const Row & row : m_content.rows)
    {
        labels.option      = AddLabel (row.option,      kBodyFontDip, DxuiFontWeight::Normal, DxuiTextRole::Link);
        labels.description = AddLabel (row.description, kBodyFontDip, DxuiFontWeight::Normal, DxuiTextRole::Body);

        labels.option->SetFontFace (DxuiTheme::kMonoFace);

        m_rows.push_back (labels);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::AddLabel
//
////////////////////////////////////////////////////////////////////////////////

DxuiLabel * CommandLineHelpPanel::AddLabel (const std::wstring & text, float fontDip, DxuiFontWeight weight, DxuiTextRole role)
{
    DxuiLabel  * label = CreateChild<DxuiLabel>();



    label->SetText        (text);
    label->SetFontSizeDip (fontDip);
    label->SetFontWeight  (weight);
    label->SetTextRole    (role);
    label->SetTextAlign   (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::Layout
//
//  Places the labels at the measured sizes when the table has been measured
//  at this width, and at estimated sizes until then. Paint measures with the
//  window's renderer when the estimate is all there is. An estimate replaces
//  the measured table, so the next paint measures again.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  widthPx = boundsPx.right - boundsPx.left;



    SetBounds (boundsPx);

    m_scaler   = scaler;
    m_boundsPx = boundsPx;

    //  A minimized window lays its content out with no width.
    if (widthPx <= 0)
    {
        return;
    }

    if (!IsMeasuredAt (widthPx))
    {
        EstimateTable (widthPx);
    }

    Arrange (boundsPx, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::TryMeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineHelpPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    HRESULT  hr = S_OK;



    UNREFERENCED_PARAMETER (theme);

    hr = MeasureTable (text, widthPx);
    CHR (hr);

    heightPx = Arrange (RECT { 0, 0, widthPx, 0 }, false);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::IsMeasuredAt
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineHelpPanel::IsMeasuredAt (int widthPx) const
{
    return m_measured && m_tableWidthPx == GetTableWidthPx (widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::GetTableWidthPx
//
//  The table is inset on both sides, so the shading of a row extends past its
//  text.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLineHelpPanel::GetTableWidthPx (int widthPx) const
{
    return std::max (1, widthPx - m_scaler.ToPx (kInsetDip) * 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::MeasureTable
//
//  Measures each switch's width and each cell's wrapped height with the
//  renderer that draws them, and the refusal's wrapped height, at `widthPx`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineHelpPanel::MeasureTable (IDxuiTextRenderer & text, int widthPx)
{
    HRESULT                       hr      = S_OK;
    float                         fontPx  = m_scaler.ToPxf (kBodyFontDip);
    float                         w       = 0.0f;
    float                         h       = 0.0f;
    CommandLineHelpLayout::Input  input;



    input.widthPx      = GetTableWidthPx (widthPx);
    input.columnGapPx  = m_scaler.ToPx (kColumnGapDip);
    input.rowPadPx     = m_scaler.ToPx (kRowPadDip);
    input.lineHeightPx = m_scaler.ToPx (kLineDip);

    for (const Row & row : m_content.rows)
    {
        hr = text.MeasureString (row.option.c_str(), fontPx, DxuiTheme::kMonoFace, w, h);
        CHR (hr);

        input.optionWidthsPx.push_back ((int) std::ceil (w) + 1);
    }

    input.measureOption = [&] (size_t row, int cellPx)
    {
        float  cellW = 0.0f;
        float  cellH = 0.0f;



        hr = text.MeasureStringWrapped (m_content.rows[row].option.c_str(), fontPx, DxuiTheme::kMonoFace, (float) cellPx, cellW, cellH);

        return SUCCEEDED (hr) ? (int) std::ceil (cellH) : 0;
    };

    input.measureDescription = [&] (size_t row, int cellPx)
    {
        float  cellW = 0.0f;
        float  cellH = 0.0f;



        hr = text.MeasureStringWrapped (m_content.rows[row].description.c_str(), fontPx, DxuiTheme::kBodyFace, (float) cellPx, cellW, cellH);

        return SUCCEEDED (hr) ? (int) std::ceil (cellH) : 0;
    };

    m_table = CommandLineHelpLayout::Compute (input);
    CHR (hr);

    m_refusalPx = 0;

    if (m_refusal != nullptr)
    {
        hr = text.MeasureStringWrapped (m_content.refusal.c_str(), fontPx, DxuiTheme::kBodyFace, (float) widthPx, w, h);
        CHR (hr);

        m_refusalPx = std::max (m_scaler.ToPx (kLineDip), (int) std::ceil (h));
    }

    m_tableWidthPx = input.widthPx;
    m_measured     = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::EstimateTable
//
//  The table from an average character width, for a layout that runs before
//  there is a renderer to measure with.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::EstimateTable (int widthPx)
{
    int                           charPx = m_scaler.ToPx (kCharEstimateDip);
    int                           linePx = m_scaler.ToPx (kLineDip);
    CommandLineHelpLayout::Input  input;



    input.widthPx      = GetTableWidthPx (widthPx);
    input.columnGapPx  = m_scaler.ToPx (kColumnGapDip);
    input.rowPadPx     = m_scaler.ToPx (kRowPadDip);
    input.lineHeightPx = linePx;

    for (const Row & row : m_content.rows)
    {
        input.optionWidthsPx.push_back ((int) row.option.size() * charPx);
    }

    input.measureDescription = [&] (size_t row, int cellPx)
    {
        int  perLine = std::max (1, cellPx / charPx);
        int  chars   = (int) m_content.rows[row].description.size();



        return ((chars + perLine - 1) / perLine) * linePx;
    };

    m_table     = CommandLineHelpLayout::Compute (input);
    m_refusalPx = (m_refusal != nullptr) ? linePx : 0;
    m_measured  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::Arrange
//
//  The header, a divider, the refusal, the usage and the option table, top to
//  bottom. Returns the height used. Places the labels only when `place` is
//  set.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLineHelpPanel::Arrange (const RECT & bounds, bool place)
{
    int  gap      = m_scaler.ToPx (kSectionGapDip);
    int  headingH = m_scaler.ToPx (kHeadingLineDip);
    int  headGap  = m_scaler.ToPx (kHeadingGapDip);
    int  lineH    = m_scaler.ToPx (kLineDip);
    int  inset    = m_scaler.ToPx (kInsetDip);
    int  y        = bounds.top;



    y += ArrangeHeader (bounds, place);
    y += gap / 2;

    if (place)
    {
        m_dividerYPx = y;
    }

    y += gap / 2;

    if (m_refusal != nullptr)
    {
        if (place)
        {
            m_refusal->Layout (RECT { bounds.left, y, bounds.right, y + m_refusalPx }, m_scaler);
        }

        y += m_refusalPx + gap;
    }

    if (place)
    {
        m_usageHeading->Layout (RECT { bounds.left, y, bounds.right, y + headingH }, m_scaler);
        m_usage->Layout        (RECT { bounds.left + inset, y + headingH + headGap, bounds.right, y + headingH + headGap + lineH }, m_scaler);
    }

    y += headingH + headGap + lineH + gap;

    if (place)
    {
        m_optionsHeading->Layout (RECT { bounds.left, y, bounds.right, y + headingH }, m_scaler);
    }

    y += headingH + headGap;

    if (place)
    {
        PlaceRows (bounds.left, bounds.right, y);
    }

    y += m_table.heightPx;

    return y - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::ArrangeHeader
//
//  The logo on the left and the name, subtitle and version stacked beside it,
//  centered on each other. Returns the header's height.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLineHelpPanel::ArrangeHeader (const RECT & bounds, bool place)
{
    int  logoPx    = m_scaler.ToPx (kLogoDip);
    int  titleH    = m_scaler.ToPx (kTitleLineDip);
    int  subtitleH = m_scaler.ToPx (kSubtitleLineDip);
    int  versionH  = m_scaler.ToPx (kVersionLineDip);
    int  stackH    = titleH + subtitleH + versionH;
    int  headerH   = std::max (logoPx, stackH);
    int  textLeft  = bounds.left + logoPx + m_scaler.ToPx (kLogoGapDip);
    int  logoTop   = bounds.top + (headerH - logoPx) / 2;
    int  stackTop  = bounds.top + (headerH - stackH) / 2;



    if (place)
    {
        m_logoRectPx = RECT { bounds.left, logoTop, bounds.left + logoPx, logoTop + logoPx };

        m_title->Layout    (RECT { textLeft, stackTop,                      bounds.right, stackTop + titleH }, m_scaler);
        m_subtitle->Layout (RECT { textLeft, stackTop + titleH,             bounds.right, stackTop + titleH + subtitleH }, m_scaler);
        m_version->Layout  (RECT { textLeft, stackTop + titleH + subtitleH, bounds.right, stackTop + stackH }, m_scaler);
    }

    return headerH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::PlaceRows
//
//  Each row's switch and description in their columns, inset from the band
//  that the row's shading fills, and every second band added to the shading
//  list.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::PlaceRows (int left, int right, int top)
{
    int     inset = m_scaler.ToPx (kInsetDip);
    int     pad   = m_scaler.ToPx (kRowPadDip);
    int     rowY  = 0;
    int     rowH  = 0;
    size_t  i     = 0;



    m_stripesPx.clear();

    for (i = 0; i < m_rows.size() && i < m_table.rows.size(); ++i)
    {
        rowY = top + m_table.rows[i].topPx;
        rowH = m_table.rows[i].heightPx;

        m_rows[i].option->Layout      (RECT { left + inset, rowY + pad, left + inset + m_table.optionWidthPx, rowY + rowH - pad }, m_scaler);
        m_rows[i].description->Layout (RECT { left + inset + m_table.descriptionLeftPx, rowY + pad, right - inset, rowY + rowH - pad }, m_scaler);

        if (i % 2 == 0)
        {
            m_stripesPx.push_back (RECT { left, rowY, right, rowY + rowH });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::Paint
//
//  Measures with this renderer when the layout so far is an estimate, then
//  draws the row shading, the divider and the logo under the labels.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr      = S_OK;
    int      widthPx = m_boundsPx.right - m_boundsPx.left;



    if (!IsMeasuredAt (widthPx))
    {
        hr = MeasureTable (text, widthPx);

        if (SUCCEEDED (hr))
        {
            Arrange (m_boundsPx, true);
        }
    }

    PaintDecorations (text, theme);

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanel::PaintDecorations
//
//  The shaded bands under every second option row, the divider under the
//  header, and the logo.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineHelpPanel::PaintDecorations (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr uint32_t  kAlphaMask  = 0xFF000000;
    constexpr uint32_t  kColorMask  = 0x00FFFFFF;
    constexpr int       kAlphaShift = 24;
    constexpr uint32_t  kStripeDiv  = 2;
    HRESULT             hr          = S_OK;
    uint32_t            hover       = theme.HoverBackground();
    uint32_t            alpha       = ((hover & kAlphaMask) >> kAlphaShift) / kStripeDiv;
    uint32_t            stripe      = (hover & kColorMask) | (alpha << kAlphaShift);
    float               dividerH    = std::max (1.0f, m_scaler.ToPxf (1.0f));



    for (const RECT & band : m_stripesPx)
    {
        hr = text.FillRect ((float) band.left, (float) band.top, (float) (band.right - band.left), (float) (band.bottom - band.top), stripe);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.FillRect ((float) m_boundsPx.left, (float) m_dividerYPx, (float) (m_boundsPx.right - m_boundsPx.left), dividerH, theme.Divider());
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!m_content.logoPixels.empty() && m_logoRectPx.right > m_logoRectPx.left)
    {
        hr = text.DrawIconBitmap (m_content.logoPixels.data(),
                                  m_content.logoWidth,
                                  m_content.logoHeight,
                                  (float) m_logoRectPx.left,
                                  (float) m_logoRectPx.top,
                                  (float) (m_logoRectPx.right  - m_logoRectPx.left),
                                  (float) (m_logoRectPx.bottom - m_logoRectPx.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





