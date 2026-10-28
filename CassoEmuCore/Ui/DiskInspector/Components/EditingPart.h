#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/EditController.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"

class EditBar;
class IInspectorComponentHost;
class InspectorSession;
class SectorByteView;





////////////////////////////////////////////////////////////////////////////////
//
//  EditingPart
//
//  Sector editing: the edit bar over the Sector data tab, typing into the
//  selected sector, and applying, undoing, redoing and saving a copy of the
//  edits through the host (FR-096 to FR-116).
//
////////////////////////////////////////////////////////////////////////////////

class EditingPart : public IInspectorComponent
{
public:
    EditingPart (InspectorSession & session, IInspectorComponentHost & host, SectorByteView & byteView);

    void          Create  () override;
    void          Update  () override;
    bool          OnKey   (const DxuiKeyEvent & ev) override;
    std::wstring  GetTip  (POINT pointPx, RECT & outAnchorPx) const override;

    void  Layout              (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx         (int widthPx, const DxuiDpiScaler & scaler) const;
    void  SetShown            (bool isShown);
    void  ToggleEditing       ();
    void  FollowEdits         ();
    bool  TakeReply           (const InspectorReply & reply);
    void  OnDiskStarted       (bool isNewDisk, bool isReload);
    void  OnEjected           ();
    bool  ConfirmDiscardEdits (const std::wstring & action);

private:
    //  One apply's tracks before and after, the guest write counts it left,
    //  and those an undo of it left (FR-107).
    struct AppliedEditRecord
    {
        vector<std::shared_ptr<const TrackCopy>>  before;
        vector<std::shared_ptr<const TrackCopy>>  after;
        vector<RecordWriteCount>                  countsAfter;
        vector<RecordWriteCount>                  countsUndone;
        std::string                               savedTo;
    };

    bool          HandleEditKey     (const DxuiKeyEvent & ev);
    void          UpdateEditBar     ();
    void          PasteIntoEdit     ();
    std::wstring  GetApplyBlock     () const;
    bool          IsHistoryCurrent  (const vector<RecordWriteCount> & counts) const;
    void          ApplyEdits        ();
    void          PostApply         (const InspectorRequest & request);
    void          PauseApply        ();
    void          CancelApply       ();
    void          UndoApplied       ();
    void          RedoApplied       ();
    void          TakeApplyReply    (const InspectorReply & reply);
    void          SaveEditedCopy    ();
    void          TakeSaveCopyReply (const InspectorReply & reply);
    bool          GetBSectorBytes   (PendingEdit::Sector & outBytes) const;
    void          UseBBytes         ();
    int           GetSelectedEdit   () const;
    static bool   IsSameFile        (const std::filesystem::path & a, const std::filesystem::path & b);

    InspectorSession           & m_session;
    IInspectorComponentHost    & m_host;
    SectorByteView             & m_byteView;
    EditBar                    * m_editBar          = nullptr;
    int64_t                      m_editSig          = 0;
    vector<AppliedEditRecord>    m_history;
    int                          m_historyAt        = 0;
    InspectorRequest             m_lastApply;
    uint64_t                     m_applyRequest     = 0;
    uint64_t                     m_saveCopyRequest  = 0;
    bool                         m_isApplyWaiting   = false;
    bool                         m_isApplyPaused    = false;
    bool                         m_isFirstApplyDone = false;
};
