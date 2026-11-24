#include "Pch.h"

#include "Seams/Win32ExplorerCommandEntries.h"
#include "Seams/ShellItemId.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ExplorerCommandEntries::LoadIndirectString
//
//  A "@file,-id" resource string read from its file; any other text is
//  returned as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ExplorerCommandEntries::LoadIndirectString (const std::wstring & text)
{
    wchar_t  loaded[512] = {};
    bool     indirect    = !text.empty() && text[0] == L'@';
    HRESULT  hr          = indirect ? SHLoadIndirectString (text.c_str(), loaded, (UINT) std::size (loaded), nullptr) : E_INVALIDARG;



    return SUCCEEDED (hr) ? std::wstring (loaded) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ExplorerCommandEntries::MakeItems
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ExplorerCommandEntries::MakeItems (const std::vector<std::wstring> & paths, IShellItemArray ** outItems)
{
    HRESULT                        hr  = S_OK;
    std::vector<PIDLIST_ABSOLUTE>  ids;



    *outItems = nullptr;

    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  id = nullptr;

        hr = ShellItemId::ToPidl (path, &id);
        CHR (hr);

        ids.push_back (id);
    }

    hr = SHCreateShellItemArrayFromIDLists ((UINT) ids.size(), (PCIDLIST_ABSOLUTE_ARRAY) ids.data(), outItems);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE id : ids)
    {
        CoTaskMemFree (id);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ExplorerCommandEntries::AddEntry
//
//  Skip a command hidden for these items and gray a disabled one. After
//  E_PENDING from a quick GetState, call it again with okToBeSlow set, as
//  File Explorer does, and list the command whatever the slow answer hides,
//  as File Explorer lists Beyond Compare's. A command with no title or icon of its
//  own takes the fallbacks, and one with subcommands becomes a submenu.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ExplorerCommandEntries::AddEntry (IExplorerCommand * command, IShellItemArray * items, int depth, const std::wstring & fallbackTitle,
                                            const std::wstring & fallbackIcon, std::vector<Entry> & outEntries)
{
    static constexpr int  kMaxDepth = 2;



    EXPCMDSTATE                                   state    = ECS_ENABLED;
    EXPCMDFLAGS                                   flags    = ECF_DEFAULT;
    PWSTR                                         title    = nullptr;
    PWSTR                                         icon     = nullptr;
    Entry                                         entry;
    Microsoft::WRL::ComPtr<IEnumExplorerCommand>  children;
    HRESULT                                       hr       = S_OK;
    bool                                          pending  = false;



    hr      = command->GetState (items, FALSE, &state);
    pending = hr == E_PENDING;

    if (pending)
    {
        hr = command->GetState (items, TRUE, &state);
    }

    state = SUCCEEDED (hr) ? state : ECS_ENABLED;
    state = pending ? (state & ~ECS_HIDDEN) : state;

    hr = command->GetFlags (&flags);
    flags = SUCCEEDED (hr) ? flags : ECF_DEFAULT;

    if ((state & ECS_HIDDEN) != 0 || (flags & ECF_ISSEPARATOR) != 0)
    {
        return;
    }

    hr = command->GetTitle (items, &title);

    entry.title = (SUCCEEDED (hr) && title != nullptr) ? std::wstring (title) : fallbackTitle;
    CoTaskMemFree (title);

    if (entry.title.empty())
    {
        return;
    }

    entry.enabled = (state & ECS_DISABLED) == 0;
    entry.command = command;

    hr = command->GetIcon (items, &icon);

    entry.icon = (SUCCEEDED (hr) && icon != nullptr) ? std::wstring (icon) : fallbackIcon;
    CoTaskMemFree (icon);

    if ((flags & ECF_HASSUBCOMMANDS) != 0 && depth < kMaxDepth)
    {
        hr = command->EnumSubCommands (&children);

        while (SUCCEEDED (hr) && children)
        {
            Microsoft::WRL::ComPtr<IExplorerCommand>  child;
            ULONG                                     fetched = 0;

            hr = children->Next (1, &child, &fetched);

            if (hr != S_OK || fetched == 0)
            {
                break;
            }

            AddEntry (child.Get(), items, depth + 1, L"", L"", entry.children);
        }
    }

    outEntries.push_back (std::move (entry));
}





