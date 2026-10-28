#pragma once

#include "Pch.h"

#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar
//
//  The address bar and the search box: the path's segments and their
//  menus, typed paths and what they could go on to be, the history and
//  roots menus, and a search over the folder on display. The window routes
//  pointer and key events over them here.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerAddressBar
{
public:
    explicit ExplorerAddressBar (ExplorerContext & context);

    static constexpr const wchar_t *  kExplorerTypedPathsKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths";
    static constexpr size_t           kMaxCompletions        = 200;

    //  Explorer's address box is 32 dip tall; the search box is 30% of the
    //  width right of the tree, between these, measured at 150%.
    static constexpr int    kAddressBoxDip        = 32;
    static constexpr int    kFindBoxMinDip        = 100;
    static constexpr int    kFindBoxMaxDip        = 720;
    static constexpr float  kFindBoxShare         = 0.3f;

    void         Create                  ();
    void         Configure               ();
    void         Layout                  (const RECT & free, const RECT & strip, int rightOfTreePx);
    static RECT  GetAddressRect          (const RECT & free, const RECT & strip, const DxuiDpiScaler & scaler);
    void         ApplyTheme              (const DxuiTheme * theme) { m_suggest.SetTheme (theme); }
    void         CloseSuggestionsOnPress (const DxuiMouseEvent & ev);
    void         CloseSuggestions        () { m_suggest.Close(); }
    LPCWSTR      GetCursorForPoint       (POINT clientPx) const;

    //  Keys, and focus. The address being edited has its keys first.
    bool  IsEditing    () const { return m_address->IsEditing(); }
    void  ShowFocusCue () { m_address->SetFocusCue (true); }
    bool  IsCaretOn    () const { return m_findBox->IsCaretOn(); }
    bool  WantsTick    () const { return m_address->WantsTick(); }
    bool  Tick         (int64_t nowMs);

    //  Editing the address from a command, and the typed address dropped.
    void                                       BeginEdit               () { m_address->BeginEdit(); }
    bool                                       EndEdit                 ();
    void                                       ShowHistoryDropDown     () { ShowAddressHistoryMenu (m_address->GetBounds()); }
    void                                       GoToLocation            (const Location & location);
    void                                       FillAddress             ();
    void                                       FillNavTips             ();
    std::wstring                               GetNavLabel             (const Location & location);
    std::vector<BrowserModel::AddressSegment>  GetAddressSegmentsFor   (const Location & location);
    void                                       SubmitAddress           (const std::wstring & text);
    void                                       AddTypedPath            (const std::wstring & text);
    void                                       ShowAddressMenu         (int index, const RECT & anchor);
    void                                       ShowAddressOverflowMenu (const RECT & anchor);
    void                                       ShowAddressRootsMenu    (const RECT & anchor);
    void                                       SearchLocation          (const std::wstring & query);
    void                                       LeaveSearch             ();
    std::wstring                               GetCassoSearchScope     ();
    void                                       ShowAddressHistoryMenu  (const RECT & anchor);
    void                                       ShowAddressSuggestions  (const std::wstring & typed);
    std::vector<std::wstring>                  GetAddressHistory       () const;
    static std::vector<std::wstring>           GetCompletions          (const std::wstring & typed);
    void                                       ShowAddressContextMenu  (int x, int y);
    static std::vector<std::wstring>           ReadExplorerTypedPaths  ();
    static void                                WriteExplorerTypedPath  (const std::wstring & path);
    void                                       ShowHistoryMenu         (bool forward, const RECT & anchor);
    static std::wstring                        EscapeMnemonics         (const std::wstring & text);
    static BrowserModel::AddressRoot           GetProfileRoot          ();

    void  HandleLocationChanged (const Location & location);
    bool  HandleMouse           (const DxuiMouseEvent & ev);
    bool  HandleKey             (const DxuiKeyEvent & ev) { return m_address->OnKey (ev); }
    void  HandleFocusChanged    (ExplorerPane focus);
    bool  HandleFindBoxKey      (const DxuiKeyEvent & ev);
    bool  HandleAddressKey      (WPARAM vk);

private:
    ExplorerContext  & m_context;
    DxuiAddressBar   * m_address = nullptr;

    //  Explorer's search box, at the address bar's right: a search over the
    //  folder shown and below it.
    DxuiTextInput                              * m_findBox = nullptr;
    DxuiSuggestionList                           m_suggest;
    std::vector<BrowserModel::AddressSegment>    m_addressSegments;
    BrowserModel::AddressRoot                    m_addressRoot;
    std::wstring                                 m_addressTyped;
};
