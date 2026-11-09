#pragma once

#include "Pch.h"

struct DialogDefinition;
struct GlobalUserPrefs;
class IDxuiControl;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IUpdaterHost
//
//  What ShellUpdater uses from the app it runs in. Casso and Casso Explorer
//  each supply one. All of it is called on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class IUpdaterHost
{
public:
    virtual ~IUpdaterHost() = default;

    //  Update results are posted to the result window, as
    //  AppMessage::ShellUpdateResult. Dialogs open over the owner window.
    virtual HWND               GetResultWindow () = 0;
    virtual HWND               GetOwnerWindow  () = 0;
    virtual HINSTANCE          GetInstance     () = 0;
    virtual const IDxuiTheme * GetDialogTheme  () = 0;

    //  The prefs in memory, the update fields of them saved over the shared
    //  file, and the file's global section as it is now.
    virtual GlobalUserPrefs  & GetPrefs        () = 0;
    virtual void               SavePrefs       () = 0;
    virtual HRESULT            ReadStoredPrefs (GlobalUserPrefs & outPrefs) = 0;

    //  The title bar the indicator sits in.
    virtual bool  HasCaption                 () = 0;
    virtual bool  TryGetClientWidthPx        (int & outWidthPx) = 0;
    virtual UINT  GetDpi                     () = 0;
    virtual void  SetCaptionAccessory        (IDxuiControl * accessory) = 0;
    virtual void  SetCaptionAccessoryWidth   (int widthDip) = 0;
    virtual int   GetCaptionReservedWidthDip () = 0;
    virtual void  RequestRepaint             () = 0;
    virtual void  ShowIndicatorTip           (const RECT & anchorPx, const std::wstring & text, int64_t nowMs) = 0;
    virtual void  HideIndicatorTip           () = 0;

    //  What the user reads.
    virtual void  ShowNotice          (const std::wstring & text) = 0;
    virtual void  ShowNotification    (const std::wstring & text) = 0;
    virtual int   ShowModalDialog     (const DialogDefinition & def) = 0;
    virtual void  RefreshUpdateStatus () = 0;

    //  Polls the prefs file while another app's check is running. Each tick
    //  calls ShellUpdater::PollSharedCheckRecord.
    virtual HRESULT  StartSharedCheckTimer (UINT periodMs) = 0;
    virtual void     StopSharedCheckTimer  () = 0;

    //  Installing. PrepareForDeploy flushes unsaved state before a deploy,
    //  since the deploy ends the process. RequestClose starts an ordinary exit.
    //  GetRelaunchArguments returns the options a relaunch repeats.
    virtual void                       PrepareForDeploy     () = 0;
    virtual void                       RequestClose         () = 0;
    virtual std::vector<std::wstring>  GetRelaunchArguments () = 0;
};
