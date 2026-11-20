#pragma once

#include "Pch.h"

class ComparisonPart;
class EditingPart;
class FilesView;
class FindPart;
class HostLink;
class InspectorDialogs;
class InspectorToolbar;
class PlatterCorners;
class SelectionPart;
class TrackColumn;





//  The window's calls for its commands: Close, the window's zoom, Default
//  size, and hiding the tip before the walkthrough opens.
class IInspectorFrame
{
public:
    virtual ~IInspectorFrame() = default;

    virtual void    CloseWindow        () = 0;
    virtual double  GetWindowZoom      () const = 0;
    virtual void    RestoreDefaultSize () = 0;
    virtual void    HideTooltip        () = 0;
};


//  The window's parts that its commands call. The parts are created after
//  the commands, so a command accesses each part when it runs.
class IInspectorParts
{
public:
    virtual ~IInspectorParts() = default;

    virtual InspectorDialogs  & GetDialogs     () = 0;
    virtual EditingPart       & GetEditing     () = 0;
    virtual SelectionPart     & GetSelection   () = 0;
    virtual FilesView         & GetFiles       () = 0;
    virtual FindPart          & GetFind        () = 0;
    virtual InspectorToolbar  & GetToolbar     () = 0;
    virtual PlatterCorners    & GetCorners     () = 0;
    virtual TrackColumn       & GetTrackColumn () = 0;
    virtual ComparisonPart    & GetComparison  () = 0;
    virtual HostLink          & GetHostLink    () = 0;
};
