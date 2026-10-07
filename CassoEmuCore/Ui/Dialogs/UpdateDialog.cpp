#include "Pch.h"

#include "Ui/Dialogs/UpdateDialog.h"
#include "Ui/Dialogs/UpdateDialogContent.h"
#include "Ui/Dialogs/SizeGrip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::UpdateDialog
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialog::UpdateDialog()
{
    m_pendingContent = std::make_unique<UpdateDialogContent>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::~UpdateDialog
//
//  The content's callbacks point back into this object, so the window and
//  its tree go before the members they reach.
//
////////////////////////////////////////////////////////////////////////////////

UpdateDialog::~UpdateDialog()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::Configure
//
//  Call before Create.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::Configure (
    UpdateButtonSet         buttons,
    const std::wstring    & opener,
    const std::wstring    & header,
    const std::wstring    & updateLabel,
    const std::wstring    & developerNudge,
    const std::wstring    & pageUrl,
    Callbacks               callbacks)
{
    m_buttons     = buttons;
    m_pageUrl     = pageUrl;
    m_updateLabel = updateLabel;
    m_nudge       = developerNudge;
    m_callbacks   = std::move (callbacks);

    m_pendingContent->SetOpener       (opener);
    m_pendingContent->SetHeader       (header);
    m_pendingContent->SetPageUrl      (pageUrl);
    m_pendingContent->SetNotesMessage (UpdateDialogModel::kpszNotesLoading);
    m_pendingContent->SetOnOpenUrl    ([this] (const std::wstring & url)
    {
        if (m_callbacks.onOpenUrl)
        {
            m_callbacks.onOpenUrl (url);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::OnCreate
//
//  Skip this version goes bottom-left, the primary bottom-right. A developer
//  build gets no primary; its pull-and-rebuild nudge takes that place.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::OnCreate()
{
    constexpr UINT  kTickMs = 100;



    m_content = m_pendingContent.get();
    SetDialogContentOwned (std::move (m_pendingContent));

    m_skipBtn    = AddDialogButton (UpdateDialogModel::kpszSkip, kIdSkip, DxuiButtonRow::Anchor::Left);
    if (m_buttons == UpdateButtonSet::Developer)
    {
        m_nudgeLabel = CreateChild<DxuiLabel>();
        m_nudgeLabel->SetText      (m_nudge);
        m_nudgeLabel->SetTextRole  (DxuiTextRole::Muted);
        m_nudgeLabel->SetTextAlign (DxuiTextHAlign::Right, DxuiTextVAlign::Center);
    }
    else
    {
        m_primaryBtn = AddDialogButton (m_updateLabel, kIdPrimary);
        m_primaryBtn->SetOnClick ([this] () { OnPrimaryClick(); });
    }

    m_grip = CreateChild<SizeGrip>();

    ApplyButtonLabels();
    SetDialogTickIntervalMs (kTickMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::GetDefaultCommandId
//
//  A developer build has no default: Enter must not skip a release.
//
////////////////////////////////////////////////////////////////////////////////

int UpdateDialog::GetDefaultCommandId() const
{
    return (m_buttons == UpdateButtonSet::Developer) ? 0 : kIdPrimary;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::Layout
//
//  Remembers what it was laid out against, so a relabeled button can be
//  measured again without waiting for a resize.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_lastBoundsPx = boundsPx;
    m_lastScaler   = scaler;
    m_hasLayout    = true;

    DxuiDialogWindow::Layout (boundsPx, scaler);

    // The gripper marks the bottom-right corner as a resize handle, and is
    // gone while maximized, when there is nothing to resize.
    if (m_grip != nullptr)
    {
        m_grip->Layout     (SizeGrip::GetGripRect (boundsPx, scaler.ToPx (SizeGrip::kSizeDip)), scaler);
        m_grip->SetVisible (GetHwnd() == nullptr || IsZoomed (GetHwnd()) == FALSE);
    }

    // A developer build's nudge takes the primary button's place: the
    // bottom-right of the button row.
    if (m_nudgeLabel != nullptr)
    {
        int  pad   = scaler.ToPx (DxuiButtonRow::kEdgePadDip);
        int  rowH  = scaler.ToPx (DxuiButtonRow::kRowHeightDip);
        int  skipW = scaler.ToPx (DxuiButtonRow::GetWidthForLabel (UpdateDialogModel::kpszSkip) + DxuiButtonRow::kGapDip);

        // Everything right of Skip, so the nudge stays on one line.
        m_nudgeLabel->Layout (RECT { boundsPx.left + pad + skipW, boundsPx.bottom - rowH,
                                     boundsPx.right - pad, boundsPx.bottom - pad }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::Relayout
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::Relayout()
{
    if (m_hasLayout)
    {
        DxuiDialogWindow::Layout (m_lastBoundsPx, m_lastScaler);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ApplyButtonLabels
//
//  The primary button says what it will do now: start the update, cancel
//  the download in flight, or open the release page.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ApplyButtonLabels()
{
    LPCWSTR  label = UpdateDialogModel::kpszOpenPage;



    if (m_primaryBtn == nullptr)
    {
        return;
    }

    if (m_isBusy)
    {
        label = UpdateDialogModel::kpszCancel;
    }
    else if (m_buttons != UpdateButtonSet::ReleasePage)
    {
        label = m_updateLabel.c_str();
    }

    m_primaryBtn->SetLabel   (label);
    m_primaryBtn->SetEnabled (m_isBusy ? m_canCancel : m_buttons != UpdateButtonSet::Developer);

    if (m_skipBtn != nullptr)
    {
        m_skipBtn->SetEnabled (!m_isBusy);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::SetBusy
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::SetBusy (bool isBusy, bool canCancel)
{
    m_isBusy    = isBusy;
    m_canCancel = canCancel;

    ApplyButtonLabels();
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::OnPrimaryClick
//
//  The release page closes the dialog once it is open; the update and
//  Cancel keep it open, since the result arrives later.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::OnPrimaryClick()
{
    if (m_isBusy)
    {
        if (m_canCancel && m_callbacks.onCancelUpdate)
        {
            m_callbacks.onCancelUpdate();
        }

        return;
    }

    if (m_buttons == UpdateButtonSet::UpdateNow)
    {
        m_content->SetStatus (UpdateDialogModel::MakeProgressText (0, 0), false);
        SetBusy (true, true);

        if (m_callbacks.onUpdateNow)
        {
            m_callbacks.onUpdateNow();
        }

        return;
    }

    if (m_callbacks.onOpenUrl)
    {
        m_callbacks.onOpenUrl (m_pageUrl);
    }

    EndDialog (kIdPrimary);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::OnDialogTick
//
//  Lets the owner push progress, and resizes the notes area once painting
//  has measured them.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::OnDialogTick()
{
    if (m_callbacks.onTick)
    {
        m_callbacks.onTick();
    }

    if (m_content != nullptr && m_content->SyncNotesHeight())
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowNotes
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowNotes (const ReleaseNotes & notes)
{
    m_content->SetNotes (notes);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::SetHeader
//
//  The header gains its closing remark once the notes have dated the
//  running build, so it is replaced after the dialog is up.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::SetHeader (const std::wstring & header)
{
    m_content->SetHeader (header);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowNotesMissing
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowNotesMissing()
{
    m_content->SetNotesMessage (UpdateDialogModel::kpszNotesMissing);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowProgress
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowProgress (std::uint64_t bytesDone, std::uint64_t bytesTotal)
{
    if (m_isBusy && m_canCancel)
    {
        m_content->SetStatus (UpdateDialogModel::MakeProgressText (bytesDone, bytesTotal), false);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowInstalling
//
//  Past the download there is nothing safe to cancel, so Cancel is
//  disabled from here on.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowInstalling()
{
    m_content->SetStatus (UpdateDialogModel::kpszInstalling, false);
    SetBusy (true, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowRestarting
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowRestarting()
{
    m_content->SetStatus (UpdateDialogModel::kpszRestarting, false);
    SetBusy (true, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowFailure
//
//  Reports the failure under its label and puts the buttons back, swapped
//  for the release page when retrying cannot help.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowFailure (UpdateFailure failure)
{
    m_buttons = UpdateDialogModel::SelectAfterFailure (m_buttons, failure);

    m_content->SetStatus (UpdateDialogModel::MakeUpdateFailedText (failure), true);
    SetBusy (false, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowCanceled
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowCanceled()
{
    m_content->SetStatus (L"The update was canceled. Nothing was changed.", false);
    SetBusy (false, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::ShowImage
//
//  A release-notes image arrived (or, null, could not be shown). The next
//  paint flows the notes around it, and the tick resizes the scroll area.
//
////////////////////////////////////////////////////////////////////////////////

void UpdateDialog::ShowImage (const std::string & src, std::shared_ptr<const NotesImage> image)
{
    m_content->SetNotesImage (src, std::move (image));
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialog::GetImageSources
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> UpdateDialog::GetImageSources() const
{
    return (m_content != nullptr) ? m_content->GetImageSources() : std::vector<std::string>();
}