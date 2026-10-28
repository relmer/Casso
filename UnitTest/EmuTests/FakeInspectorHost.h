#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeInspectorHost
//
//  A fake disk inspector window, so a test can create one of its components
//  on a bare panel and check the component's calls to the window (FR-162).
//  Nothing is drawn: there is no HWND, popup host or text renderer.
//
////////////////////////////////////////////////////////////////////////////////

class FakeInspectorHost : public IInspectorComponentHost
{
public:
    //  The panel the component creates its controls in, and the component's
    //  calls to the window, newest last.
    DxuiPanel     panel;
    vector<int>   trackTabs;
    vector<int>   diskTabs;
    int           selections = 0;
    int           relayouts  = 0;
    std::wstring  statusNote;

    DxuiPanel          & GetControlParent    () override { return panel; }
    HWND                 GetOwnerHwnd        () const override { return nullptr; }
    DxuiHwndSource     * GetPopupHost        () const override { return nullptr; }
    IDxuiTextRenderer  * GetTextRenderer     () const override { return nullptr; }
    void                 Relayout            () override { relayouts++; }
    void                 OnSelection         () override { selections++; }
    void                 ShowTrackTab        (int tab) override { trackTabs.push_back (tab); }
    void                 ShowDiskTab         (int tab) override { diskTabs.push_back (tab); }
    void                 SetStatusNote       (const std::wstring & note) override { statusNote = note; }
    bool                 Confirm             (const std::wstring &, const vector<std::wstring> &, const std::wstring &) override { return true; }
    bool                 ConfirmDiscardEdits (const std::wstring &) override { return true; }
    void                 RequestCopy         () override {}
    void                 StartDisk           (std::shared_ptr<const DiskCopy>, const std::wstring &) override {}
    HRESULT              ShowDrive           (int) override { return S_OK; }
    void                 TakeTracks          (const InspectorReply &) override {}
    void                 SetWindowZoom       (double) override {}
    void                 OpenWalkthrough     () override {}

    void                 NotifyDiskStarting    (const DiskChange &) override {}
    void                 NotifyDiskStarted     (const DiskChange &) override {}
    void                 NotifyAnalysisChanged () override {}
    bool                 OfferReply            (const InspectorReply &) override { return false; }
};
