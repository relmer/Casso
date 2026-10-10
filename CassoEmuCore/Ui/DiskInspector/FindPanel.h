#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Ui/DiskInspector/InspectorTableView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindPanel
//
//  "Find" (FR-056): what to look for, a nibble pattern, hex bytes or text in
//  sector data, on the selected track or the whole disk, with "Any bit
//  offset" for nibble patterns. Find lists every hit with its place, or says
//  "No matches"; "Go to" closes on the chosen hit. The hits outlast the
//  panel, so F3 and Shift+F3 step through them from the window, and a hit
//  whose track has been analyzed again since is marked out of date.
//
////////////////////////////////////////////////////////////////////////////////

class FindPanel : public DxuiDialogWindow
{
public:
    void  Configure (const IDxuiTheme * theme, InspectorViewContext & context, int quarterTrack, const SearchQuery & query, const vector<SearchHit> & hits);

    const SearchQuery &        GetQuery  () const { return m_query; }
    const vector<SearchHit> &  GetHits   () const { return m_hits; }
    int                        GetChosen () const { return m_chosen; }

    static constexpr SIZE  kSizeDip = { 560, 520 };

protected:
    void  OnCreate () override;

private:
    void  OnFind      ();
    void  OnGoTo      ();
    void  ShowHits    ();
    void  ReadQuery   ();
    std::wstring  FormatOwner (const SearchHit & hit) const;

    const IDxuiTheme *       m_theme        = nullptr;   // non-owning
    InspectorViewContext *   m_context      = nullptr;   // non-owning
    int                      m_quarterTrack = 0;
    SearchQuery              m_query;
    vector<SearchHit>        m_hits;
    int                      m_chosen       = -1;
    int                      m_highlighted  = -1;

    DxuiRadioGroup                       m_kind;
    DxuiLabel                            m_label;
    DxuiTextInput                        m_input;
    DxuiCheckbox                         m_wholeDisk { L"Whole disk" };
    DxuiCheckbox                         m_anyBit    { L"Any bit offset" };
    DxuiLabel                            m_status;
    std::unique_ptr<InspectorTableView>  m_results;
};
