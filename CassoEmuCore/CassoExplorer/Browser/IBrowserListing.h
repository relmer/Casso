#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IBrowserListing
//
//  The list the browser loads for the active tab, as BrowserNavigator calls
//  it after it moves a tab or switches to one.
//
////////////////////////////////////////////////////////////////////////////////

class IBrowserListing
{
public:
    virtual ~IBrowserListing() = default;

    virtual void      ReloadAfterNavigation ()                                          = 0;
    virtual void      ReturnToTab           (const std::vector<std::wstring> & rowKeys) = 0;
    virtual void      SelectRowsByKeys      (const std::vector<std::wstring> & rowKeys) = 0;
    virtual bool      IsOnMissingDrive      (const Location & location) const           = 0;
    virtual bool      IsHomeLocation        () const                                    = 0;
    virtual void      RequestHomeRead       ()                                          = 0;
    virtual Location  GetShellParent        () const                                    = 0;
};
