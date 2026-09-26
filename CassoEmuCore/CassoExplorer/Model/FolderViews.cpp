#include "Pch.h"

#include "CassoExplorer/Model/FolderViews.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::GetDefaultView
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::View FolderViews::GetDefaultView (FolderType type)
{
    switch (type)
    {
        case FolderType::Drives:   return DxuiListView::View::Tiles;
        case FolderType::Pictures: return DxuiListView::View::LargeIcons;
        case FolderType::Videos:   return DxuiListView::View::LargeIcons;
        default:                   return DxuiListView::View::Details;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::GetView
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::View FolderViews::GetView (const std::wstring & key, FolderType type) const
{
    for (const FolderViewEntry & entry : m_entries)
    {
        if (IsSameFolder (entry.key, key))
        {
            return entry.view;
        }
    }

    return GetDefaultView (type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::Remember
//
////////////////////////////////////////////////////////////////////////////////

void FolderViews::Remember (const std::wstring & key, DxuiListView::View view)
{
    std::erase_if (m_entries, [&key] (const FolderViewEntry & entry) { return IsSameFolder (entry.key, key); });

    m_entries.insert (m_entries.begin(), FolderViewEntry { key, view });

    if (m_entries.size() > kMaxEntries)
    {
        m_entries.resize (kMaxEntries);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::SetEntries
//
////////////////////////////////////////////////////////////////////////////////

void FolderViews::SetEntries (std::vector<FolderViewEntry> entries)
{
    m_entries = std::move (entries);

    if (m_entries.size() > kMaxEntries)
    {
        m_entries.resize (kMaxEntries);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::IsSameFolder
//
//  Host paths compare as Windows compares them, whatever their case and a
//  trailing backslash.
//
////////////////////////////////////////////////////////////////////////////////

bool FolderViews::IsSameFolder (const std::wstring & a, const std::wstring & b)
{
    std::wstring_view  left  = a;
    std::wstring_view  right = b;



    while (left.size() > 3 && left.back() == L'\\')
    {
        left.remove_suffix (1);
    }

    while (right.size() > 3 && right.back() == L'\\')
    {
        right.remove_suffix (1);
    }

    return left.size() == right.size() && _wcsnicmp (left.data(), right.data(), left.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderViews::ReadFolderType
//
////////////////////////////////////////////////////////////////////////////////

FolderViews::FolderType FolderViews::ReadFolderType (const std::wstring & path)
{
    struct KnownType
    {
        const KNOWNFOLDERID  & id;
        FolderType             type;
    };

    struct NamedType
    {
        const wchar_t  * name;
        FolderType       type;
    };

    static const KnownType  kKnown[] =
    {
        { FOLDERID_Documents, FolderType::Documents },
        { FOLDERID_Pictures,  FolderType::Pictures  },
        { FOLDERID_Music,     FolderType::Music     },
        { FOLDERID_Videos,    FolderType::Videos    },
    };

    static const NamedType  kNamed[] =
    {
        { L"Documents", FolderType::Documents },
        { L"Pictures",  FolderType::Pictures  },
        { L"Photos",    FolderType::Pictures  },
        { L"Music",     FolderType::Music     },
        { L"Videos",    FolderType::Videos    },
    };

    wchar_t       named[64] = {};
    std::wstring  ini       = path;



    for (const KnownType & known : kKnown)
    {
        PWSTR    found = nullptr;
        HRESULT  hr    = SHGetKnownFolderPath (known.id, KF_FLAG_DEFAULT, nullptr, &found);
        bool     same  = SUCCEEDED (hr) && IsSameFolder (found, path);

        CoTaskMemFree (found);

        if (same)
        {
            return known.type;
        }
    }

    if (!ini.empty() && ini.back() != L'\\')
    {
        ini += L'\\';
    }

    ini += L"desktop.ini";

    GetPrivateProfileStringW (L"ViewState", L"FolderType", L"", named, (DWORD) std::size (named), ini.c_str());

    for (const NamedType & entry : kNamed)
    {
        if (_wcsicmp (named, entry.name) == 0)
        {
            return entry.type;
        }
    }

    return FolderType::Generic;
}
