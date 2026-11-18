#include "Pch.h"

#include "DxuiPaneLayoutText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::Write
//
//  `dxui-layout 1`, then `tree` and the tree as nested lists, then a `float`
//  line per floating pane and a `hide` line per auto-hidden one. Names are
//  quoted, with `\` before a quote or a backslash in them. A floating pane
//  that shares its window, or whose window is not keyed by its own name,
//  ends its line with the window's key, then `active` for the tab shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiPaneLayoutText::Write (const DxuiPaneLayout & layout)
{
    std::wstring  out = std::format (L"dxui-layout {}\ntree ", DxuiPaneLayout::kVersion);



    if (layout.GetRoot() == nullptr)
    {
        out += L"none";
    }
    else
    {
        WriteNode (layout.GetRoot(), out);
    }

    out += L"\n";

    for (const Floating & floating : layout.GetFloating())
    {
        WriteFloating (layout, floating, out);
    }

    for (const AutoHidden & hidden : layout.GetAutoHidden())
    {
        WriteAutoHidden (hidden, out);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::Quote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiPaneLayoutText::Quote (const std::wstring & text)
{
    std::wstring  q = L"\"";



    for (wchar_t c : text)
    {
        q += (c == L'"' || c == L'\\') ? std::wstring (L"\\") + c : std::wstring (1, c);
    }

    return q + L"\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::WriteNode
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayoutText::WriteNode (const Node * node, std::wstring & out)
{
    if (node->kind == Node::Kind::Split)
    {
        out += std::format (L"(split {} {:.4f} ", node->horizontal ? L"h" : L"v", node->ratio);
        WriteNode (node->first.get(), out);
        out += L" ";
        WriteNode (node->second.get(), out);
        out += L")";
        return;
    }

    out += std::format (L"(tabs {}", node->active);

    for (const std::wstring & pane : node->panes)
    {
        out += L" " + Quote (pane);
    }

    out += L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::WriteFloating
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayoutText::WriteFloating (
    const DxuiPaneLayout  & layout,
    const Floating        & floating,
    std::wstring          & out)
{
    const RECT  & rc = floating.rectDip;



    out += std::format (L"float {} {} {} {} {} {} {}", Quote (floating.pane), Quote (floating.monitorKey),
                        rc.left, rc.top, rc.right, rc.bottom, Quote (floating.homePane));

    //  A pane alone in a window keyed by its own name is written as
    //  before windows held groups.
    if (floating.window != floating.pane || layout.GetFloatingPanes (floating.window).size() > 1)
    {
        out += L" " + Quote (floating.window) + (floating.isActive ? L" active" : L"");
    }

    //  The index among its group's tabs, for a pane that shared a group.
    if (floating.homeIndex >= 0)
    {
        out += std::format (L" index {}", floating.homeIndex);
    }

    out += L"\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::WriteAutoHidden
//
//  A pane that was alone on its side of a split has that side, the split's
//  ratio and the panes on the other side, so DockBack splits there again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayoutText::WriteAutoHidden (const AutoHidden & hidden, std::wstring & out)
{
    out += std::format (L"hide {} {} {}", Quote (hidden.pane), GetSideName (hidden.edge), Quote (hidden.homePane));

    if (hidden.homeSplit)
    {
        out += std::format (L" split {} {:.4f} {}", GetSideName (hidden.homeSide), hidden.homeRatio, hidden.homeSiblings.size());

        for (const std::wstring & sibling : hidden.homeSiblings)
        {
            out += L" " + Quote (sibling);
        }
    }

    //  The index among its group's tabs, for a pane that shared a group.
    if (hidden.homeIndex >= 0)
    {
        out += std::format (L" index {}", hidden.homeIndex);
    }

    out += L"\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::GetSideName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiPaneLayoutText::GetSideName (DxuiDockSide side)
{
    static const wchar_t * const  kNames[] = { L"left", L"top", L"right", L"bottom" };



    return kNames[(int) side];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::TryGetSide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::TryGetSide (const std::wstring & name, DxuiDockSide & side)
{
    for (int i = 0; i < 4; i++)
    {
        if (name == GetSideName ((DxuiDockSide) i))
        {
            side = (DxuiDockSide) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::Tokenize
//
//  Parentheses are tokens of their own; a quoted string is one token, kept
//  with its opening quote so it cannot be mistaken for a keyword.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayoutText::Tokens DxuiPaneLayoutText::Tokenize (const std::wstring & text)
{
    std::vector<std::wstring>  tokens;
    size_t                     at     = 0;



    while (at < text.size())
    {
        wchar_t       c = text[at];
        std::wstring  token;

        if (iswspace (c))
        {
            at++;
            continue;
        }

        if (c == L'(' || c == L')')
        {
            tokens.push_back (std::wstring (1, c));
            at++;
            continue;
        }

        if (c == L'"')
        {
            token = L"\"";

            for (at++; at < text.size() && text[at] != L'"'; at++)
            {
                if (text[at] == L'\\' && at + 1 < text.size())
                {
                    at++;
                }

                token += text[at];
            }

            if (at >= text.size())
            {
                return {};
            }

            at++;
            tokens.push_back (token);
            continue;
        }

        while (at < text.size() && !iswspace (text[at]) && text[at] != L'(' && text[at] != L')' && text[at] != L'"')
        {
            token += text[at++];
        }

        tokens.push_back (token);
    }

    return tokens;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::ReadNode
//
//  Null on anything malformed; the caller refuses the whole text.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<DxuiPaneLayoutNode> DxuiPaneLayoutText::ReadNode (const Tokens & tokens, size_t & at)
{
    std::unique_ptr<Node>  node   = std::make_unique<Node>();
    wchar_t              * end    = nullptr;
    std::wstring           kind;
    std::wstring           axis;
    std::wstring           ratio;
    std::wstring           active;
    auto                   next   = [&] () -> std::wstring { return (at < tokens.size()) ? tokens[at++] : std::wstring(); };



    if (next() != L"(")
    {
        return nullptr;
    }

    kind = next();

    if (kind == L"split")
    {
        axis  = next();
        ratio = next();

        node->kind       = Node::Kind::Split;
        node->horizontal = (axis == L"h");
        node->ratio      = wcstof (ratio.c_str(), &end);

        if ((axis != L"h" && axis != L"v") || ratio.empty() || *end != L'\0' || node->ratio < 0.0f || node->ratio > 1.0f)
        {
            return nullptr;
        }

        node->first  = ReadNode (tokens, at);
        node->second = (node->first != nullptr) ? ReadNode (tokens, at) : nullptr;

        return (node->second != nullptr && next() == L")") ? std::move (node) : nullptr;
    }

    if (kind != L"tabs")
    {
        return nullptr;
    }

    active = next();

    node->kind   = Node::Kind::Tabs;
    node->active = (int) wcstol (active.c_str(), &end, 10);

    if (active.empty() || *end != L'\0')
    {
        return nullptr;
    }

    while (at < tokens.size() && !tokens[at].empty() && tokens[at][0] == L'"')
    {
        node->panes.push_back (tokens[at++].substr (1));
    }

    if (node->panes.empty() || next() != L")")
    {
        return nullptr;
    }

    node->active = std::clamp (node->active, 0, (int) node->panes.size() - 1);
    return node;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::IsString
//
//  A quoted string's token, which starts with its opening quote.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::IsString (const Tokens & tokens, size_t i)
{
    return i < tokens.size() && !tokens[i].empty() && tokens[i][0] == L'"';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::TryRead
//
//  All or nothing: `out` is replaced only for a text with the current
//  version, a well-formed tree, no unknown line and no pane listed twice.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::TryRead (const std::wstring & text, Parts & out)
{
    Tokens  tokens = Tokenize (text);
    Parts   parts;
    size_t  at     = 3;



    if (tokens.size() < 4 || tokens[0] != L"dxui-layout" || tokens[1] != std::to_wstring (DxuiPaneLayout::kVersion) || tokens[2] != L"tree")
    {
        return false;
    }

    if (tokens[at] == L"none")
    {
        at++;
    }
    else
    {
        parts.root = ReadNode (tokens, at);

        if (parts.root == nullptr)
        {
            return false;
        }
    }

    while (at < tokens.size())
    {
        if (tokens[at] == L"float" && IsString (tokens, at + 1) && IsString (tokens, at + 2) && IsString (tokens, at + 7))
        {
            Floating  floating;

            if (!TryReadFloating (tokens, at, floating))
            {
                return false;
            }

            parts.floating.push_back (floating);
            continue;
        }

        if (tokens[at] == L"hide" && IsString (tokens, at + 1) && at + 3 < tokens.size() && IsString (tokens, at + 3))
        {
            AutoHidden  hidden;

            if (!TryReadAutoHidden (tokens, at, hidden))
            {
                return false;
            }

            parts.autoHidden.push_back (hidden);
            continue;
        }

        return false;
    }

    if (HasPaneTwice (parts))
    {
        return false;
    }

    out = std::move (parts);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::TryReadFloating
//
//  A `float` line, from its keyword: the pane, the monitor, the rect and the
//  pane to dock back beside. A pane in a window with other panes then has
//  the window's key, and `active` when its tab is the selected one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::TryReadFloating (
    const Tokens  & tokens,
    size_t        & at,
    Floating      & out)
{
    long       values[4] = {};
    wchar_t  * end       = nullptr;



    for (int i = 0; i < 4; i++)
    {
        values[i] = wcstol (tokens[at + 3 + (size_t) i].c_str(), &end, 10);

        if (*end != L'\0' || tokens[at + 3 + (size_t) i].empty())
        {
            return false;
        }
    }

    out.pane       = tokens[at + 1].substr (1);
    out.monitorKey = tokens[at + 2].substr (1);
    out.rectDip    = { values[0], values[1], values[2], values[3] };
    out.homePane   = tokens[at + 7].substr (1);
    out.window     = out.pane;
    out.isActive   = true;
    at += 8;

    if (IsString (tokens, at))
    {
        out.window   = tokens[at].substr (1);
        out.isActive = at + 1 < tokens.size() && tokens[at + 1] == L"active";
        at += out.isActive ? 2 : 1;
    }

    return TryReadIndex (tokens, at, out.homeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::TryReadAutoHidden
//
//  A `hide` line, from its keyword: the pane, its edge and the pane it docks
//  back beside, then for a pane that was alone on its side of a split, that
//  side, the split's ratio and the panes on the other side.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::TryReadAutoHidden (
    const Tokens  & tokens,
    size_t        & at,
    AutoHidden    & out)
{
    wchar_t  * ratioEnd = nullptr;
    wchar_t  * countEnd = nullptr;
    long       count    = 0;



    if (!TryGetSide (tokens[at + 2], out.edge))
    {
        return false;
    }

    out.pane     = tokens[at + 1].substr (1);
    out.homePane = tokens[at + 3].substr (1);
    at += 4;

    if (at + 3 < tokens.size() && tokens[at] == L"split")
    {
        out.homeSplit = TryGetSide (tokens[at + 1], out.homeSide);
        out.homeRatio = wcstof (tokens[at + 2].c_str(), &ratioEnd);
        count         = wcstol (tokens[at + 3].c_str(), &countEnd, 10);

        if (!out.homeSplit || *ratioEnd != L'\0' || *countEnd != L'\0' || out.homeRatio < 0.0f || out.homeRatio > 1.0f || count < 0)
        {
            return false;
        }

        at += 4;

        for (long i = 0; i < count; i++, at++)
        {
            if (!IsString (tokens, at))
            {
                return false;
            }

            out.homeSiblings.push_back (tokens[at].substr (1));
        }
    }

    return TryReadIndex (tokens, at, out.homeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::TryReadIndex
//
//  A pane's index among its group's tabs, `index` and a number, for a pane
//  that was in a group with others. A layout saved by an older build has
//  none, and then `index` is not set.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::TryReadIndex (
    const Tokens  & tokens,
    size_t        & at,
    int           & index)
{
    wchar_t  * indexEnd = nullptr;



    if (at + 1 >= tokens.size() || tokens[at] != L"index")
    {
        return true;
    }

    index = (int) wcstol (tokens[at + 1].c_str(), &indexEnd, 10);

    if (tokens[at + 1].empty() || *indexEnd != L'\0' || index < 0)
    {
        return false;
    }

    at += 2;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutText::HasPaneTwice
//
//  Whether a pane appears more than once among the tree's groups, the
//  floating panes and the auto-hidden ones.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutText::HasPaneTwice (const Parts & parts)
{
    std::vector<std::wstring>           seen;
    std::function<void (const Node *)>  collect = [&] (const Node * node)
    {
        if (node == nullptr)
        {
            return;
        }

        seen.insert (seen.end(), node->panes.begin(), node->panes.end());
        collect (node->first.get());
        collect (node->second.get());
    };



    collect (parts.root.get());

    for (const Floating & floating : parts.floating)
    {
        seen.push_back (floating.pane);
    }

    for (const AutoHidden & hidden : parts.autoHidden)
    {
        seen.push_back (hidden.pane);
    }

    std::sort (seen.begin(), seen.end());

    return std::adjacent_find (seen.begin(), seen.end()) != seen.end();
}