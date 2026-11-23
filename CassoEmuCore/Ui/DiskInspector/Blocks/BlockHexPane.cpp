#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"
#include "Ui/DiskInspector/Blocks/BlockHexPane.h"
#include "Ui/DiskInspector/Blocks/BlockHexView.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::Create
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::Create (DxuiPanel & parent)
{
    m_hex    = parent.CreateChild<BlockHexView>();
    m_fields = parent.CreateChild<BlockFieldsView>();

    m_hex->SetBytesPerRow  (kBytesPerRow);
    m_hex->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hex->SetMarkColor    ([this] (uint8_t mark, uint32_t & outArgb)
    {
        outArgb = GetMarkColor (mark);

        return mark != BlockHexSource::kMarkNone;
    });
    m_hex->SetByteStyle    ([this] (uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle)
    {
        bool  isCaret = m_isEditing && m_hasFocus && offset == m_hex->GetCaret();



        return PickByteStyle (m_colors, m_isEditing, isCaret, mark, isSelected, outStyle);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::GetMarkColor
//
//  An edited byte is drawn in the theme's accent, once the pane has it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockHexPane::GetMarkColor (uint8_t mark) const
{
    return mark == BlockHexSource::kMarkChanged ? (m_colors.accent != 0 ? m_colors.accent : s_kChangedColor)
         : mark == BlockHexSource::kMarkDiffers ? s_kDiffersColor
         : mark == BlockHexSource::kMarkField   ? s_kFieldColor
         :                                        s_kLinkColor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::ShowBytes
//
//  The block's bytes, or the header's, with their marks and fields.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexPane::ShowBytes (BlockInspectorModel & model)
{
    BlockInspectorModel::Block  bytes  = {};
    BlockInspectorModel::Block  other  = {};
    vector<BlockField>          fields;
    uint32_t                    block  = model.GetBlock();
    int64_t                     drawn  = (int64_t) block;
    bool                        isNew  = drawn != m_drawn;
    HRESULT                     hr     = S_OK;
    size_t                      i      = 0;



    hr = model.ReadBlock (block, bytes);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_source.SetBytes (bytes);
    model.GetFields (block, bytes, fields);
    m_fields->SetHeading   (L"Fields in block " + NumberFormat::FormatInteger (block));
    m_fields->SetEmptyText (L"There are no fields in this block.");

    for (i = 0; i < bytes.size(); i++)
    {
        if (model.GetEdits().IsChanged (block, (int) i))
        {
            m_source.SetMark (i, BlockHexSource::kMarkChanged);
        }
    }

    if (model.IsDifferent (block))
    {
        hr = model.ReadOtherBlock (block, other);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (i = 0; i < bytes.size(); i++)
        {
            if (bytes[i] != other[i])
            {
                m_source.SetMark (i, BlockHexSource::kMarkDiffers);
            }
        }
    }

    for (const BlockField & field : fields)
    {
        if (field.link == BlockField::kNoLink)
        {
            continue;
        }

        for (i = field.offset; i < (size_t) field.offset + field.length; i++)
        {
            m_source.SetMark (i, BlockHexSource::kMarkLink);
        }

        if (field.pairOffset >= 0)
        {
            m_source.SetMark ((uint64_t) field.pairOffset, BlockHexSource::kMarkLink);
        }
    }

    if (isNew)
    {
        m_fields->SetFields (std::move (fields));
        m_fieldTip.Hide();

        m_drawn = drawn;
        m_hex->SetSource (&m_source);
        m_hex->ResetNibble();
    }
    else
    {
        m_fields->UpdateFields (std::move (fields));
    }

    return isNew;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::MarkField
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::MarkField (BlockInspectorModel & model, const BlockField * field)
{
    uint32_t  i = 0;



    (void) ShowBytes (model);

    if (field == nullptr)
    {
        return;
    }

    for (i = field->offset; i < field->offset + field->length; i++)
    {
        m_source.SetMark (i, BlockHexSource::kMarkField);
    }

    if (field->pairOffset >= 0)
    {
        m_source.SetMark ((uint64_t) field->pairOffset, BlockHexSource::kMarkField);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::Layout (const RECT & hexPx, const RECT & fieldsPx, const DxuiDpiScaler & scaler)
{
    int   frame = std::max (1, scaler.ToPx ((int) kFrameDip));
    RECT  inner = { hexPx.left + frame, hexPx.top + frame, std::max (hexPx.left + frame, hexPx.right - frame), std::max (hexPx.top + frame, hexPx.bottom - frame) };



    m_frameRect = hexPx;
    m_framePx   = frame;

    m_hex->SetVisible    (true);
    m_fields->SetVisible (true);

    m_hex->Layout    (inner, scaler);
    m_fields->Layout (fieldsPx, scaler);
    FitRows ((int) (inner.right - inner.left), scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::SetEditLook
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::SetEditLook (bool isEditing, bool hasFocus, const EditColors & colors)
{
    m_isEditing = isEditing;
    m_hasFocus  = hasFocus;
    m_colors    = colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::PaintFrame
//
//  The frame's room is there whether or not it is drawn, so the hex view
//  doesn't move when editing starts or stops.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::PaintFrame (IDxuiPainter & painter) const
{
    float  left   = (float) m_frameRect.left;
    float  top    = (float) m_frameRect.top;
    float  width  = (float) (m_frameRect.right - m_frameRect.left);
    float  height = (float) (m_frameRect.bottom - m_frameRect.top);
    float  edge   = (float) m_framePx;



    if (!m_isEditing || width <= 2 * edge || height <= 2 * edge)
    {
        return;
    }

    painter.FillRect (left, top, width, edge, m_colors.accent);
    painter.FillRect (left, top + height - edge, width, edge, m_colors.accent);
    painter.FillRect (left, top + edge, edge, height - 2 * edge, m_colors.accent);
    painter.FillRect (left + width - edge, top + edge, edge, height - 2 * edge, m_colors.accent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::PickByteStyle
//
//  As the memory window draws the byte the keys act on: the accent behind
//  the caret byte while editing, in both columns. An edited byte is drawn
//  in the memory window's changed-byte red, made readable on whatever is
//  behind it, the caret's accent included. While editing, every other byte
//  has a light accent tint behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexPane::PickByteStyle (const EditColors & colors, bool isEditing, bool isCaret, uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle)
{
    bool      isEdited = mark == BlockHexSource::kMarkChanged;
    uint32_t  tint     = DxuiColor::Mix (colors.background, colors.accent, kTintShare);
    uint32_t  changed  = GetChangedInk (colors.background);



    outStyle = DxuiHexViewTypes::ByteStyle();

    if (colors.accent == 0 || (!isEditing && !isEdited))
    {
        return false;
    }

    if (isCaret)
    {
        outStyle.fill = colors.accent;
        outStyle.ink  = isEdited ? GetReadableInk (changed, colors.accent) : DxuiColor::GetContrastingInk (colors.accent);
        return true;
    }

    if (isSelected && !isEdited)
    {
        return false;
    }

    outStyle.fill = isSelected ? DxuiColor::Mix (colors.background, colors.accent, 2 * kTintShare) : isEditing ? tint : 0;
    outStyle.ink  = isEdited ? GetReadableInk (changed, outStyle.fill != 0 ? outStyle.fill : colors.background) : 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::GetChangedInk
//
//  The debugger's memory window color for a changed byte: coral on a dark
//  page and red on a light one, made readable on the page, or on a light
//  page's row shade.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockHexPane::GetChangedInk (uint32_t background)
{
    constexpr uint32_t  kDarkChanged  = 0xFFFF6B68;
    constexpr uint32_t  kLightChanged = 0xFFD00000;
    constexpr uint32_t  kBlack        = 0xFF000000;
    constexpr float     kRowShade     = 0.15f;
    constexpr float     kDarkLimit    = 0.5f;



    bool  isDark = DxuiColor::ComputeRelativeLuminance (background) < kDarkLimit;



    return GetReadableInk (isDark ? kDarkChanged : kLightChanged, isDark ? background : DxuiColor::Mix (background | kBlack, kBlack, kRowShade));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::GetReadableInk
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockHexPane::GetReadableInk (uint32_t ink, uint32_t fill)
{
    constexpr int    kSteps = 10;
    constexpr float  kStep  = 0.1f;



    uint32_t  toward = DxuiColor::GetContrastingInk (fill);
    uint32_t  result = ink;



    for (int step = 1; step <= kSteps && DxuiColor::ComputeContrastRatio (result, fill) < kMinContrast; step++)
    {
        result = DxuiColor::Mix (ink, toward, step * kStep);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::RefitRows
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexPane::RefitRows (const DxuiDpiScaler & scaler)
{
    if (m_hex->GetContentWidthPx() == m_contentPx)
    {
        return false;
    }

    FitRows ((int) (m_hex->GetBounds().right - m_hex->GetBounds().left), scaler);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::FitRows
//
//  The most bytes in a row, of 16, 8 and 4, whose columns fit the hex
//  view's width, so no scroll bar across is drawn. Before the view has
//  measured its text, the rows stay as they are.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::FitRows (int widthPx, const DxuiDpiScaler & scaler)
{
    int  room  = widthPx - scaler.ToPx ((int) kHexScrollDip);
    int  bytes = m_hex->GetBytesPerRow();



    for (int count : { 16, 8, 4 })
    {
        m_hex->SetBytesPerRow (count);
        bytes = count;

        if (m_hex->GetContentWidthPx() == 0 || m_hex->GetContentWidthPx() <= room)
        {
            break;
        }
    }

    m_hex->SetBytesPerRow (bytes);
    m_contentPx = m_hex->GetContentWidthPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::MeasureMinHexPx
//
////////////////////////////////////////////////////////////////////////////////

int BlockHexPane::MeasureMinHexPx (const DxuiDpiScaler & scaler)
{
    int  bytes  = m_hex->GetBytesPerRow();
    int  width  = 0;



    m_hex->SetBytesPerRow (kMinHexBytes);
    width = m_hex->GetContentWidthPx() > 0 ? m_hex->GetContentWidthPx() + scaler.ToPx ((int) kHexScrollDip) : 0;
    m_hex->SetBytesPerRow (bytes);

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::GetTooltip
//
//  A split pointer's high byte lies outside its field, at pairOffset.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockHexPane::GetTooltip (POINT pointPx, const BlockInspectorModel & model, std::wstring & outText, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    DxuiHexView::HitResult  hit      = m_hex->IsVisible() ? m_hex->HitTestPoint (pointPx) : DxuiHexView::HitResult();
    uint8_t                 mark     = BlockHexSource::kMarkNone;
    const BlockField      * field    = nullptr;
    int                     noteLine = -1;



    if (!hit.hit || hit.offset >= m_source.GetByteCount())
    {
        return false;
    }

    m_source.ReadMarks (hit.offset, std::span<uint8_t> (&mark, 1));

    for (const BlockField & candidate : m_fields->GetFields())
    {
        if (field == nullptr && (candidate.Contains ((uint32_t) hit.offset) || candidate.pairOffset == (int) hit.offset))
        {
            field = &candidate;
        }
    }

    outText = BlockHeaderText::BuildByteTip (hit.offset, mark, field, BlockHeaderText::GetOtherName (model), noteLine);

    if (noteLine >= 0)
    {
        outSwatches = { { noteLine, GetMarkColor (mark) } };
    }
    else
    {
        outSwatches.clear();
    }

    outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::UpdateFieldTip
//
//  As Casso Explorer's tree draws a name its pane cuts off: the field row
//  under the pointer whole, laid over the row and on over the map, at
//  once, with the pointer passing through to the row. A press, the wheel
//  or leaving the row hides it.
//
////////////////////////////////////////////////////////////////////////////////

void BlockHexPane::UpdateFieldTip (const DxuiMouseEvent & ev, IDxuiTextRenderer * text, const IDxuiTheme * theme)
{
    int   row = -1;
    RECT  box = {};



    if (ev.kind == DxuiMouseEventKind::Move && text != nullptr && theme != nullptr)
    {
        row = m_fields->FindRowAt (ev.positionDip);
    }

    if (row < 0 || !m_fields->GetClippedRowRect (row, *text, box))
    {
        m_fieldTip.Hide();
        return;
    }

    m_fieldTip.Show (box, theme->BackgroundElevated() | 0xFF000000u, [this, row, theme] (IDxuiPainter & painter, IDxuiTextRenderer & rowText)
    {
        m_fields->PaintRowTip (row, painter, rowText, *theme);
    });
}
