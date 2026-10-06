#pragma once

#include "Pch.h"

#include "Update/UpdateDialogModel.h"
#include "Update/ReleaseNotesExtractor.h"


class UpdateDialogContent;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog
//
//  "Casso update": both versions, the release date, the release notes, and
//  the actions UpdateDialogModel picks for this copy. The primary button
//  keeps the dialog open: Update now starts the update and turns into
//  Cancel while the download runs; Open release page opens the page and
//  closes. Skip this version closes with kIdSkip, and closing the window
//  any other way returns IDCANCEL.
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

    struct Callbacks
    {
        std::function<void ()>                       onUpdateNow;
        std::function<void ()>                       onCancelUpdate;
        std::function<void (const std::wstring &)>   onOpenUrl;
        std::function<void ()>                       onTick;
    };

    UpdateDialog();
    ~UpdateDialog() override;

    void  Configure          (UpdateButtonSet         buttons,
                              const std::wstring    & header,
                              const std::wstring    & dateLine,
                              const std::wstring    & pageUrl,
                              Callbacks               callbacks);

    void  ShowNotes          (const ReleaseNotes & notes);
    void  ShowNotesMissing   ();
    void  ShowProgress       (std::uint64_t bytesDone, std::uint64_t bytesTotal);
    void  ShowInstalling     ();
    void  ShowRestarting     ();
    void  ShowFailure        (UpdateFailure failure);
    void  ShowCanceled       ();

    bool  IsBusy             () const { return m_isBusy; }
    int   GetDefaultCommandId() const;

    void  Layout             (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

protected:
    void  OnCreate           () override;
    void  OnDialogTick       () override;

private:
    void  OnPrimaryClick     ();
    void  SetBusy            (bool isBusy, bool canCancel);
    void  ApplyButtonLabels  ();
    void  Relayout           ();

    std::unique_ptr<UpdateDialogContent>  m_pendingContent;
    UpdateDialogContent                 * m_content      = nullptr;
    DxuiButton                          * m_primaryBtn   = nullptr;
    DxuiButton                          * m_skipBtn      = nullptr;
    UpdateButtonSet                       m_buttons      = UpdateButtonSet::ReleasePage;
    std::wstring                          m_pageUrl;
    Callbacks                             m_callbacks;
    RECT                                  m_lastBoundsPx = {};
    DxuiDpiScaler                         m_lastScaler;
    bool                                  m_hasLayout    = false;
    bool                                  m_isBusy       = false;
    bool                                  m_canCancel    = false;
};
