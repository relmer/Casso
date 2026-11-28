#pragma once

#include "Pch.h"

class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWrapCache
//
//  The lines DxuiTextElide::WrapToLines returns, stored by the text, the
//  font, the width and the other arguments, so a paint wraps only the labels
//  that changed. Clear it when the DPI changes, since the lines are measured
//  in pixels. Past s_kMaxEntries, every entry is dropped.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWrapCache
{
public:
    const std::vector<std::wstring> &  WrapToLines (IDxuiTextRenderer  & text,
                                                    const std::wstring & value,
                                                    float                fontDip,
                                                    const wchar_t      * fontFamily,
                                                    float                maxWidthDip,
                                                    int                  maxLines,
                                                    bool                 ellipsis,
                                                    bool                 gdiWidths);

    void    Clear         () { m_entries.clear(); }
    size_t  GetWrapCount  () const { return m_wraps; }

    static constexpr size_t  s_kMaxEntries = 8192;

private:
    struct Key
    {
        const IDxuiTextRenderer  * text        = nullptr;
        std::wstring               value;
        float                      fontDip     = 0.0f;
        std::wstring               fontFamily;
        float                      maxWidthDip = 0.0f;
        int                        maxLines    = 0;
        bool                       ellipsis    = false;
        bool                       gdiWidths   = false;

        auto operator<=> (const Key &) const = default;
    };

    std::map<Key, std::vector<std::wstring>>  m_entries;
    size_t                                    m_wraps = 0;
};
