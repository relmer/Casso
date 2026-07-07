#include "Pch.h"

#include "Core/DxuiFormLayout.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFormLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiFormLayout::DxuiFormLayout (float labelColumnDip,
                                float rowHeightDip,
                                float rowGapDip,
                                float sectionGapDip,
                                float subRowIndentDip)
    : m_labelColumnDip   (labelColumnDip),
      m_rowHeightDip     (rowHeightDip),
      m_rowGapDip        (rowGapDip),
      m_sectionGapDip    (sectionGapDip),
      m_subRowIndentDip  (subRowIndentDip)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFormLayout::AddRow (IDxuiControl * label, IDxuiControl * field)
{
    m_entries.push_back (Entry{ RowKind::Row, label, field });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddSubRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFormLayout::AddSubRow (IDxuiControl * label, IDxuiControl * field)
{
    m_entries.push_back (Entry{ RowKind::SubRow, label, field });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddSectionGap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFormLayout::AddSectionGap()
{
    m_entries.push_back (Entry{ RowKind::SectionGap, nullptr, nullptr });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFormLayout::Reset()
{
    m_entries.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Arrange
//
//  Walks the row entries in order, advancing a y cursor by row-height
//  + row-gap for each Row / SubRow and by section-gap for each
//  SectionGap. The label column is fixed-width; the field column
//  fills the remaining horizontal space inside the parent bounds.
//
//  The children span passed to Arrange is the visible-child slice from
//  DxuiPanel and is unused -- per-entry pointers in m_entries are the
//  source of truth.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFormLayout::Arrange (
    const RECT                          & boundsDip,
    const DxuiDpiScaler                 & /*scaler*/,
    std::span<IDxuiControl * const>       /*children*/)
{
    LONG  labelCol = (LONG) m_labelColumnDip;
    LONG  rowH     = (LONG) m_rowHeightDip;
    LONG  rowGap   = (LONG) m_rowGapDip;
    LONG  secGap   = (LONG) m_sectionGapDip;
    LONG  indent   = (LONG) m_subRowIndentDip;
    LONG  cursorY  = boundsDip.top;


    for (const Entry & entry : m_entries)
    {
        LONG  rowLeft     = boundsDip.left + (entry.kind == RowKind::SubRow ? indent : 0);
        LONG  rowRight    = boundsDip.right;
        LONG  labelLeft   = rowLeft;
        LONG  labelRight  = rowLeft + labelCol;
        LONG  fieldLeft   = labelRight + rowGap;
        RECT  labelBounds = {};
        RECT  fieldBounds = {};

        if (entry.kind == RowKind::SectionGap)
        {
            cursorY += secGap;
            continue;
        }

        if (fieldLeft > rowRight)
        {
            fieldLeft = rowRight;
        }

        labelBounds.left   = labelLeft;
        labelBounds.top    = cursorY;
        labelBounds.right  = labelRight;
        labelBounds.bottom = cursorY + rowH;

        fieldBounds.left   = fieldLeft;
        fieldBounds.top    = cursorY;
        fieldBounds.right  = rowRight;
        fieldBounds.bottom = cursorY + rowH;

        if (entry.label != nullptr) { entry.label->SetBounds (labelBounds); }
        if (entry.field != nullptr) { entry.field->SetBounds (fieldBounds); }

        cursorY += rowH + rowGap;
    }
}
