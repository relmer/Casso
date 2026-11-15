#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewArchiveView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"

struct PreviewContent;
struct VariableRow;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewListingView
//
//  A catalog's or an archive's rows under columns, in the preview's list.
//  An archive's rows are handled as the file list's are, by
//  PreviewArchiveView.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewListingView : public PreviewView
{
public:
    PreviewListingView (ExplorerPreviewManager & manager, ExplorerContext & context);

    static constexpr int  kRowHeightDip = 18;   // one line of fixed-width text, not a file listing's roomier row

    void                  Create               () override;
    void                  Configure            () override;
    void                  Layout               (const RECT & content) override;
    void                  Hide                 () override;
    bool                  UpdateScrollbarHover (POINT at) override;
    bool                  TickScrollbars       (int64_t nowMs) override;
    void                  Fill                 (const PreviewContent & preview, bool columns);
    void                  SetVisible           (bool visible);
    LPCWSTR               GetCursorForPoint    (POINT clientPx) const;
    bool                  TryGetScrollbarMenu  (const DxuiMouseEvent & ev, std::vector<DxuiPopupMenuItem> & outItems) const;
    DxuiListView        * GetList              () const { return m_list; }
    PreviewArchiveView  & GetArchive           ()       { return m_archive; }
    bool                  IsArchive            () const { return m_isArchive; }
    bool                  IsVariables          () const { return m_isVariables; }
    void                  SetVariablesFilter   (const std::wstring & filter);
    void                  UpdateValueTipAt     (const DxuiMouseEvent & ev);

    //  A VAR file's row's name: a variable's, an element's, or an array's with
    //  its subscripts; empty past the last row.
    std::wstring          GetVariableNameAt    (int row) const { return (row >= 0 && row < (int) m_varLines.size()) ? m_varLines[(size_t) row].name : std::wstring(); }

    bool     HandleShortcutKey   (const DxuiKeyEvent & ev);
    bool     HandleKey           (const DxuiKeyEvent & ev) { return (m_isVariables && HandleVariablesKey (ev)) || m_list->OnKey (ev); }
    void     HandleFocusChanged  (bool focused)            { m_list->OnFocusChanged (focused); }

private:
    //  Which part of a row CopyVariables copies.
    enum class VarPart { Name, Value, Row };

    //  One row of a VAR file's list: a simple variable, an array, or one of
    //  its elements, with the text each column has, without the indent or
    //  the expand mark.
    struct VarLine
    {
        int           array  = -1;      // the array the row is or is in; -1 for a simple variable
        bool          header = false;   // the array's own row
        std::wstring  name;
        std::wstring  type;
        std::wstring  length;
        std::wstring  value;
    };

    //  A VAR file's variables under Name, Type, Length and Value, each array
    //  a row its elements open under, all closed when `reset` is set, as for
    //  a new file.
    void  FillVariables        (const PreviewContent & preview, bool reset);
    void  SortVariables        (int column);
    bool  ToggleArray          (int row, bool open, bool toParent);
    bool  IsArrayOpen          (int row) const;
    bool  HandleVariablesMouse (const DxuiMouseEvent & ev);
    bool  HandleListMouse      (const DxuiMouseEvent & ev);
    bool  HandleVariablesKey   (const DxuiKeyEvent & ev);
    void  ShowVariablesMenu    (int x, int y);
    void  CopyVariables        (VarPart part);
    void  UpdateValueTip       (int row);
    int   GetCharWidthPx       () const;

    //  The Value column; the indents before a simple variable's name and an
    //  element's; the room around the text of a whole value over its row; and the
    //  text measured for the width of one character, with the width used
    //  when it cannot be measured.
    static constexpr size_t           s_kValueColumn      = 3;
    static constexpr const wchar_t *  s_kpszVarIndent     = L"  ";
    static constexpr const wchar_t *  s_kpszElementIndent = L"    ";
    static constexpr int              s_kValueTipPadDip   = 4;
    static constexpr const wchar_t *  s_kpszWidthSample   = L"0123456789";
    static constexpr int              s_kFallbackCharDip  = 8;

    DxuiListView        * m_list        = nullptr;
    PreviewArchiveView    m_archive     { *this, m_context };
    bool                  m_isArchive   = false;   // the rows are an archive's files, as the file list
    bool                  m_isVariables = false;   // the rows are a VAR file's variables

    //  The variables' sort, the search text they are filtered by, the rows
    //  they were last filled from, which differ for a new file, the arrays
    //  open, and what each row of the list is.
    int                     m_varColumn     = 0;
    bool                    m_varDescending = false;
    std::wstring            m_varFilter;
    const VariableRow     * m_varRows       = nullptr;
    size_t                  m_varRowCount   = 0;
    std::set<std::wstring>  m_varExpanded;
    std::vector<VarLine>    m_varLines;

    //  A value cut off at the pane's edge, drawn whole over its row.
    DxuiInPlaceTip          m_valueTip;
};
