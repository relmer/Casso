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
    m_hex->GetHostEdit().SetIsChanged ([] (uint64_t offset, uint8_t mark)
    {
        (void) offset;

        return mark == BlockHexSource::kMarkChanged;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHexPane::GetMarkColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockHexPane::GetMarkColor (uint8_t mark) const
{
    return mark == BlockHexSource::kMarkChanged ? s_kChangedColor
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
        m_hex->GetHostEdit().ResetNibble();
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
    m_hex->SetVisible    (true);
    m_fields->SetVisible (true);

    m_hex->Layout    (hexPx, scaler);
    m_fields->Layout (fieldsPx, scaler);
    FitRows ((int) (m_hex->GetBounds().right - m_hex->GetBounds().left), scaler);
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
//  BlockHexPane::FitHexWidthPx
//
//  The frame is counted whether or not the block is edited, so entering
//  and leaving edit mode doesn't move the views.
//
////////////////////////////////////////////////////////////////////////////////

int BlockHexPane::FitHexWidthPx (int widthPx, const DxuiDpiScaler & scaler)
{
    int  frame = std::max (1, scaler.ToPx (DxuiHexHostEdit::kEditFrameDip));



    FitRows (widthPx - 2 * frame, scaler);

    if (m_contentPx == 0)
    {
        return widthPx;
    }

    return std::min (widthPx, m_contentPx + scaler.ToPx ((int) kHexScrollDip) + 2 * frame);
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
