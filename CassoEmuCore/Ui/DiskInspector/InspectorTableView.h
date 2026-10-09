#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorTables.h"
#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView
//
//  A table of the inspector window: a list with a header row, an optional
//  line of text above it, and its rows from InspectorTables. Selecting a row
//  calls the view's select function with it; a header click calls the sort
//  function with the column, for a table that sorts.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorTableView : public InspectorView
{
public:
    using SelectFn = std::function<void (const TableRow & row)>;
    using SortFn   = std::function<void (int column)>;

    static constexpr int  kCaptionDip = 22;

    explicit InspectorTableView (InspectorViewContext & context);

    void  SetColumns  (const vector<std::wstring> & titles);
    void  SetRows     (vector<TableRow> rows);
    void  SetCaption  (const std::wstring & caption) { m_caption = caption; }
    void  SetOnSelect (SelectFn select) { m_select = std::move (select); }
    void  SetOnSort   (SortFn sort);
    void  SetSortIndicator (int column, bool isDescending) { m_list.SetSortIndicator (column, isDescending); }
    void  SelectRowWhere   (const std::function<bool (const TableRow & row)> & isMatch);
    void  Tick        (int64_t nowMs) override { m_list.Tick (nowMs); }

    const vector<TableRow> &  GetRows () const { return m_rows; }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    bool  OnKey   (const DxuiKeyEvent & ev) override;

protected:
    //  The room above the list, for a caption or a subclass's own controls.
    virtual int   GetTopHeightPx () const;
    virtual void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top);

    DxuiListView      m_list;
    vector<TableRow>  m_rows;
    std::wstring      m_caption;
    SelectFn          m_select;
    bool              m_isSelecting = false;
};
