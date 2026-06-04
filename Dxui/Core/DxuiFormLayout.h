#pragma once

#include "Pch.h"
#include "Core/IDxuiLayout.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFormLayout
//
//  Two-column form layout: label : field rows with a fixed label
//  column width and uniform row height. Rows may be marked as
//  sub-rows (indented under their parent) or separated by section
//  gaps.
//
//  Rows are registered via AddRow / AddSubRow / AddSectionGap. The
//  order of registration matches the order children are passed to
//  Arrange(). Mismatched counts are tolerated by truncating to the
//  shorter sequence (registered row count vs. supplied children).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiFormLayout : public IDxuiLayout
{
public:
    DxuiFormLayout (float labelColumnDip,
                    float rowHeightDip,
                    float rowGapDip,
                    float sectionGapDip,
                    float subRowIndentDip);

    void  AddRow         (IDxuiControl * label, IDxuiControl * field);
    void  AddSubRow      (IDxuiControl * label, IDxuiControl * field);
    void  AddSectionGap  ();
    void  Reset          ();

    void  Arrange        (const RECT                          & boundsDip,
                          const DxuiDpiScaler                 & scaler,
                          std::span<IDxuiControl * const>       children) override;

private:
    enum class RowKind
    {
        Row,
        SubRow,
        SectionGap,
    };

    struct Entry
    {
        RowKind         kind   = RowKind::Row;
        IDxuiControl *  label  = nullptr;
        IDxuiControl *  field  = nullptr;
    };

    float                m_labelColumnDip;
    float                m_rowHeightDip;
    float                m_rowGapDip;
    float                m_sectionGapDip;
    float                m_subRowIndentDip;
    std::vector<Entry>   m_entries;
};
