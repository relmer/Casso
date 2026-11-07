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

    bool     HandleCapturedMouse (const DxuiMouseEvent & ev);
    bool     HandleMouse         (const DxuiMouseEvent & ev);
    bool     HandleShortcutKey   (const DxuiKeyEvent & ev);
    bool     HandleKey           (const DxuiKeyEvent & ev) { return m_list->OnKey (ev); }
    void     HandleFocusChanged  (bool focused)            { m_list->OnFocusChanged (focused); }

private:
    //  A VAR file's variables under Name, Type, Length and Value, with a group
    //  for each array, collapsed when `reset` is set, as for a new file.
    void    FillVariables     (const PreviewContent & preview, bool reset);
    void    SortVariables     (int column);
    size_t  GetValueWrapChars ();

    //  The Value column, the fewest characters a value wraps at, and the text
    //  measured for the width of one character.
    static constexpr size_t           s_kValueColumn   = 3;
    static constexpr size_t           s_kMinWrapChars  = 8;
    static constexpr const wchar_t *  s_kpszWrapSample = L"0123456789";

    DxuiListView        * m_list        = nullptr;
    PreviewArchiveView    m_archive     { *this, m_context };
    bool                  m_isArchive   = false;   // the rows are an archive's files, as the file list
    bool                  m_isVariables = false;   // the rows are a VAR file's variables

    //  The variables' sort, the search text they are filtered by, and the
    //  rows they were last filled from, which differ for a new file.
    int                  m_varColumn        = 0;
    bool                 m_varDescending    = false;
    std::wstring         m_varFilter;
    const VariableRow  * m_varRows          = nullptr;
    size_t               m_varRowCount      = 0;
    int                  m_varLayoutWidthPx = 0;   // the list's width the values were last wrapped to
};
