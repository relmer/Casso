#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindBar
//
//  Find, docked under the inspector's toolbar (FR-056): the pattern, the
//  kind of search, Whole disk and Any bit offset, the selected hit's place
//  among the hits, steps to the previous and next hit, a button for the
//  Find results tab, and a close button. An info tip after the kind of
//  search has the format of that kind of pattern, with an example; a
//  pattern that cannot be read gets its reason in red after the options.
//  Enter searches for a new query and steps to the next hit of
//  the same one; Shift+Enter steps back; Escape closes the bar.
//
////////////////////////////////////////////////////////////////////////////////

class FindBar : public DxuiPanel
{
public:
    using SearchFn = std::function<void (const SearchQuery & query)>;
    using StepFn   = std::function<void (int step)>;
    using ActionFn = std::function<void ()>;

    static constexpr int  kRowDip = 38;

    FindBar();

    void  Init        (HWND hwnd, IDxuiTextRenderer * text, DxuiHwndSource * popupHost, const IDxuiTheme * theme);
    void  SetOnSearch (SearchFn search) { m_search = std::move (search); }
    void  SetOnStep   (StepFn step)     { m_step   = std::move (step); }
    void  SetOnList   (ActionFn list)   { m_list   = std::move (list); }
    void  SetOnClose  (ActionFn close)  { m_close  = std::move (close); }

    //  The query in the controls, and the one the hits are for.
    void         SetQuery    (const SearchQuery & query);
    SearchQuery  GetQuery    () const;
    void         SetSearched (const SearchQuery & query) { m_searched = query; m_hasSearched = true; }

    //  The selected hit's place among the hits, as "3 of 16" or "No
    //  matches"; and why a pattern could not be read, or nothing.
    void  SetResult (int index, int count);
    void  SetError  (const std::wstring & message);

    void  SetInputFocused (bool isFocused);
    bool  IsInputFocused  () const;

    static int  GetHeightPx (const DxuiDpiScaler & scaler) { return scaler.ToPx (kRowDip); }

    //  The format of each kind of pattern, for the info tip.
    static std::wstring  GetHint (SearchKind kind);

    //  Whether two queries find the same hits.
    static bool  IsSame (const SearchQuery & a, const SearchQuery & b);

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    bool  OnKey   (const DxuiKeyEvent & ev) override;

private:
    void  Submit   (int step);
    void  ShowHint ();

    DxuiLabel      * m_label       = nullptr;
    DxuiTextInput  * m_input       = nullptr;
    DxuiComboBox   * m_kind        = nullptr;
    DxuiCheckbox   * m_wholeDisk   = nullptr;
    DxuiCheckbox   * m_anyBit      = nullptr;
    DxuiLabel      * m_count       = nullptr;
    DxuiButton     * m_prev        = nullptr;
    DxuiButton     * m_next        = nullptr;
    DxuiButton     * m_listButton  = nullptr;
    DxuiButton     * m_closeButton = nullptr;
    DxuiInfoTip    * m_hintTip     = nullptr;
    DxuiLabel      * m_error       = nullptr;
    SearchFn         m_search;
    StepFn           m_step;
    ActionFn         m_list;
    ActionFn         m_close;
    SearchQuery      m_searched;
    bool             m_hasSearched = false;
    bool             m_isError     = false;
};
