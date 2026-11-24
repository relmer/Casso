#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserParts.h"
#include "CassoExplorer/Browser/Preview/PreviewParts.h"
#include "CassoExplorer/Model/BrowserModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPreviewer
//
//  The preview of what CassoLink lists: a linked Apple's drive as its catalog,
//  a file on that drive read over the link, and the items CassoLink adds
//  under a connection.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkPreviewer
{
public:
    CassoLinkPreviewer (const BrowserParts & parts, const PreviewParts & previewParts);

    bool  PreviewAppleDrive    (const CatalogRow & row);
    void  PreviewCassoLinkItem (const CatalogRow & row);
    void  PreviewAppleEntry    (const std::string & entryPath, const FileEntry & entry);

private:
    Location  GetLocation () const { return m_parts.model.HasTabs() ? m_parts.model.GetActiveTab().location : Location(); }

    BrowserParts  m_parts;
    PreviewParts  m_previewParts;
};
