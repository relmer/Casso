#pragma once

#include "Pch.h"
#include "DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText
//
//  A layout's text form: a version, then the tree as nested lists, then a
//  line for each floating and each auto-hidden pane. Reading is all or
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPaneLayoutText
{
public:
    using Node       = DxuiPaneLayoutNode;
    using Floating   = DxuiPaneLayout::Floating;
    using AutoHidden = DxuiPaneLayout::AutoHidden;
    using Tokens     = std::vector<std::wstring>;

    //  A layout read from text, before the layout repairs its active tabs.
    struct Parts
    {
        std::unique_ptr<Node>    root;
        std::vector<Floating>    floating;
        std::vector<AutoHidden>  autoHidden;
    };

    static std::wstring  Write   (const DxuiPaneLayout & layout);
    static bool          TryRead (const std::wstring & text, Parts & out);

private:
    static std::wstring            Quote             (const std::wstring & text);
    static void                    WriteNode         (const Node * node, std::wstring & out);
    static void                    WriteFloating     (const DxuiPaneLayout & layout, const Floating & floating, std::wstring & out);
    static void                    WriteAutoHidden   (const AutoHidden & hidden, std::wstring & out);
    static Tokens                  Tokenize          (const std::wstring & text);
    static std::unique_ptr<Node>   ReadNode          (const Tokens & tokens, size_t & at);
    static bool                    TryReadFloating   (const Tokens & tokens, size_t & at, Floating & out);
    static bool                    TryReadAutoHidden (const Tokens & tokens, size_t & at, AutoHidden & out);
    static bool                    TryReadIndex      (const Tokens & tokens, size_t & at, int & index);
    static bool                    IsString          (const Tokens & tokens, size_t i);
    static bool                    HasPaneTwice      (const Parts & parts);
    static const wchar_t         * GetSideName       (DxuiDockSide side);
    static bool                    TryGetSide        (const std::wstring & name, DxuiDockSide & side);
};
