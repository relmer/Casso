#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Model/BrowserModel.h"

class BrowserSelection;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserArchives
//
//  The archives the shell lists: which paths are inside one, why one the shell
//  lists nothing in is empty, and the archive Extract all applies to.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserArchives
{
public:
    BrowserArchives (const BrowserParts & parts, const BrowserSelection & selection);

    //  The largest archive read whole for the reason it lists nothing.
    static constexpr uintmax_t  kArchiveProbeMaxBytes = 64ull * 1024 * 1024;

    bool          IsLocalArchivePath   (const std::wstring & id) const;

    //  Why an archive the shell lists nothing in is empty, by its name;
    //  empty when it has nothing in it.
    std::wstring  DescribeEmptyArchive (const std::wstring & path) const;

    //  The archive Extract all applies to, selected or open; empty for none.
    std::wstring  GetExtractTarget     () const;

private:
    bool      TryGetFileSize (const std::wstring & path, uintmax_t & outSize) const;
    Location  GetLocation    () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }

    BrowserParts              m_parts;
    const BrowserSelection  & m_selection;
};
