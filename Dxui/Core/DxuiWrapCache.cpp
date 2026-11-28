#include "Pch.h"

#include "DxuiWrapCache.h"

#include "Core/DxuiTextElide.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWrapCache::WrapToLines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & DxuiWrapCache::WrapToLines (IDxuiTextRenderer  & text,
                                                              const std::wstring & value,
                                                              float                fontDip,
                                                              const wchar_t      * fontFamily,
                                                              float                maxWidthDip,
                                                              int                  maxLines,
                                                              bool                 ellipsis,
                                                              bool                 gdiWidths)
{
    Key   key { &text, value, fontDip, fontFamily != nullptr ? fontFamily : L"", maxWidthDip, maxLines, ellipsis, gdiWidths };
    auto  found = m_entries.find (key);



    if (found != m_entries.end())
    {
        return found->second;
    }

    if (m_entries.size() >= s_kMaxEntries)
    {
        m_entries.clear();
    }

    m_wraps++;

    return m_entries.emplace (std::move (key), DxuiTextElide::WrapToLines (text, value, fontDip, fontFamily, maxWidthDip, maxLines, ellipsis, gdiWidths)).first->second;
}
