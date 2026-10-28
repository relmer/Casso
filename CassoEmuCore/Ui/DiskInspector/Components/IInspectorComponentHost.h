#pragma once

#include "Pch.h"

class DiskCopy;
struct InspectorReply;





////////////////////////////////////////////////////////////////////////////////
//
//  IInspectorComponentHost
//
//  The window's services that a component of the disk inspector calls. A
//  component calls the window through this and its InspectorSession and
//  nothing else, so a test can host a component with a fake (FR-162).
//
////////////////////////////////////////////////////////////////////////////////

class IInspectorComponentHost
{
public:
    virtual ~IInspectorComponentHost() = default;

    virtual DxuiPanel          & GetControlParent    () = 0;
    virtual HWND                 GetOwnerHwnd        () const = 0;
    virtual DxuiHwndSource     * GetPopupHost        () const = 0;
    virtual IDxuiTextRenderer  * GetTextRenderer     () const = 0;
    virtual void                 Relayout            () = 0;
    virtual void                 OnSelection         () = 0;
    virtual void                 ShowTrackTab        (int tab) = 0;
    virtual void                 ShowDiskTab         (int tab) = 0;
    virtual void                 SetStatusNote       (const std::wstring & note) = 0;
    virtual bool                 Confirm             (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action) = 0;
    virtual bool                 ConfirmDiscardEdits (const std::wstring & action) = 0;
    virtual void                 RequestCopy         () = 0;
    virtual void                 StartDisk           (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason) = 0;
    virtual HRESULT              ShowDrive           (int drive) = 0;
    virtual void                 TakeTracks          (const InspectorReply & reply) = 0;
};
