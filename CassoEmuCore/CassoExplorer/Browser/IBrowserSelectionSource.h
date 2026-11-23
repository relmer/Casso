#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

struct VolumeListing;





////////////////////////////////////////////////////////////////////////////////
//
//  IBrowserSelectionSource
//
//  The location and listing BrowserSelection reads the selection against,
//  and the preview and status it updates after the selection changes.
//
////////////////////////////////////////////////////////////////////////////////

class IBrowserSelectionSource
{
public:
    virtual ~IBrowserSelectionSource() = default;

    virtual Location               GetLocation                  () const                                     = 0;
    virtual bool                   IsHomeLocation               () const                                     = 0;
    virtual bool                   IsImageLocation              () const                                     = 0;
    virtual bool                   IsVolumeLocation             () const                                     = 0;
    virtual const VolumeListing  & GetListing                   () const                                     = 0;
    virtual bool                   TryGetSelectedCassoLinkPaths (std::vector<std::wstring> & outPaths) const = 0;
    virtual void                   UpdatePreviewAndStatus       ()                                           = 0;
};
