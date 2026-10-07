#pragma once

#include "Pch.h"

#include "Update/UpdateDialogModel.h"
#include "Update/ReleaseNotesExtractor.h"
#include "Update/UpdateResult.h"


class UpdateDialogContent;
class SizeGrip;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog
//
//  "Casso update": the versions and release date, the release notes, and the
//  actions UpdateDialogModel selects for this copy. Skip this version sits
//  bottom-left and closes with kIdSkip. Bottom-right, an official copy with
//  a download gets Update when closed and Update now (the default): both
//  keep the dialog open, and Update now turns into Cancel while a download
//  runs. Update when closed leaves the update waiting for Casso to close and
//  says so; Update now from there applies it at once. Without a usable
//  download, Open release page opens the page and closes. A developer build
//  has no update buttons: a nudge to pull and rebuild sits in their place.
//  Closing the window any other way returns IDCANCEL.
//
//  The owner feeds progress and results in through the Show calls; the
//  dialog itself starts nothing.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateDialog : public DxuiDialogWindow
{
public:
    static constexpr int  kIdPrimary = 100;
    static constexpr int  kIdSkip    = 101;
    static constexpr int  kIdClosed  = 102;

    struct Callbacks
    {
        std::function<void ()>                       onUpdateNow;
        std::function<void ()>                       onUpdateWhenClosed;
        std::function<void ()>                       onCancelUpdate;
        std::function<void (const std::wstring &)>   onOpenUrl;
        std::function<void ()>                       onTick;
    };

    UpdateDialog();
    ~UpdateDialog() override;

    void  Configure          (UpdateButtonSet         buttons,
                              const std::wstring    & opener,
                              const std::wstring    & header,
                              const std::wstring    & developerNudge,
                              const std::wstring    & pageUrl,
                              bool                    isPending,
                              Callbacks               callbacks);

    void  ShowNotes          (const ReleaseNotes & notes);
    void  ShowNotesMissing   ();
    void  SetHeader          (const std::wstring & header);
    void  ShowImage          (const std::string & src, std::shared_ptr<const NotesImage> image);
    std::vector<std::string>  GetImageSources () const;
    void  ShowProgress       (std::uint64_t bytesDone, std::uint64_t bytesTotal);
    void  ShowInstalling     ();
    void  ShowRestarting     ();
    void  ShowFailure        (UpdateFailure failure);
    void  ShowCanceled       ();
    void  ShowPending        ();

    bool  IsBusy             () const { return m_isBusy; }
    int   GetDefaultCommandId() const;

    void  Layout             (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

protected:
    void  OnCreate           () override;
    void  OnDialogTick       () override;

private:
    void  OnPrimaryClick     ();
    void  OnClosedClick      ();
    void  SetBusy            (bool isBusy, bool canCancel);
    void  ApplyButtonLabels  ();
    void  Relayout           ();

    std::unique_ptr<UpdateDialogContent>  m_pendingContent;
    UpdateDialogContent                 * m_content      = nullptr;
    DxuiButton                          * m_primaryBtn   = nullptr;
    DxuiButton                          * m_skipBtn      = nullptr;
    DxuiButton                          * m_closedBtn    = nullptr;
    UpdateButtonSet                       m_buttons      = UpdateButtonSet::ReleasePage;
    std::wstring                          m_pageUrl;
    std::wstring                          m_nudge;
    DxuiLabel                           * m_nudgeLabel   = nullptr;
    SizeGrip                            * m_grip         = nullptr;
    Callbacks                             m_callbacks;
    RECT                                  m_lastBoundsPx = {};
    DxuiDpiScaler                         m_lastScaler;
    bool                                  m_hasLayout    = false;
    bool                                  m_isBusy       = false;
    bool                                  m_canCancel    = false;
    bool                                  m_isPending    = false;
};
