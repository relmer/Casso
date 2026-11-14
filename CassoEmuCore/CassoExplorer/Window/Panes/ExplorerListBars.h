#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"

struct ExplorerContext;
class  DxuiInfoBanner;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars
//
//  The infobars above the file list: a search's regular expression that
//  isn't valid, and an A2Pico card's A2retroNET drives at the card's root,
//  with Repair for a settings file with CR LF line ends. Each is put away for
//  the location it was closed at.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerListBars
{
public:
    using RelayoutFn = std::function<void ()>;

    explicit ExplorerListBars (ExplorerContext & context) : m_context (context) {}

    //  The bars, as children of the window. `relayout` is called to lay the
    //  pane out again when a click in a bar makes a bar visible or hidden.
    void  Create      (RelayoutFn relayout);

    //  The bars for the location on display. True when a bar became visible
    //  or hidden.
    bool  Fill        ();

    //  The visible bars at the top of the rect, and the rect below them.
    RECT  Layout      (const RECT & rect);

    //  The event offered to the visible bars' buttons and close boxes, in
    //  window coordinates. True when a bar took it.
    bool  HandleMouse (const DxuiMouseEvent & ev);

    DxuiInfoBanner & GetCardBar () { return *m_cardBar; }

private:
    enum class CardAction { Drives, Repair, WritePo };

    static std::wstring  DescribeFiles (const std::vector<std::wstring> & paths);

    bool  FillSearchBar ();
    bool  FillCardBar   ();
    void  RunCardAction (size_t index);

    ExplorerContext            & m_context;
    RelayoutFn                   m_relayout;
    DxuiInfoBanner             * m_searchBar = nullptr;
    DxuiInfoBanner             * m_cardBar   = nullptr;
    std::wstring                 m_searchBarClosed;   // the search whose infobar was closed
    std::wstring                 m_cardBarClosed;     // the folder whose card infobar was closed
    ActionsA2retroNet::Card      m_card;
    std::vector<CardAction>      m_cardActions;       // the card infobar's buttons, in order
    std::vector<std::wstring>    m_cardLabels;        // the labels the buttons were made with
    std::vector<std::wstring>    m_convertible;       // the images Write as .po writes
};
