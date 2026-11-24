#pragma once

#include "Pch.h"

#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Model/Location.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IBrowserLoaderHost
//
//  The browser as BrowserLoader calls it: to list the location again, to
//  queue an image's volume facts, and to go to another location.
//
////////////////////////////////////////////////////////////////////////////////

class IBrowserLoaderHost
{
public:
    virtual ~IBrowserLoaderHost() = default;

    virtual HRESULT  Refresh            ()                             = 0;
    virtual void     QueueVolumeFacts   (const std::wstring & folder)  = 0;
    virtual void     NavigateToLocation (const Location & location)    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IBrowserPreviewHost
//
//  The browser as BrowserPreviewer calls it.
//
////////////////////////////////////////////////////////////////////////////////

class IBrowserPreviewHost
{
public:
    virtual ~IBrowserPreviewHost() = default;

    virtual bool  IsHomeLocation () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IBrowserCellsHost
//
//  The browser as BrowserCells calls it, for the status bar.
//
////////////////////////////////////////////////////////////////////////////////

class IBrowserCellsHost
{
public:
    virtual ~IBrowserCellsHost() = default;

    virtual bool                      IsHomeLocation () const = 0;
    virtual BrowserRowPaths::Listing  GetRowListing  ()       = 0;
};
