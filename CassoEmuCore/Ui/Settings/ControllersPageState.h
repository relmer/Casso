#pragma once

#include "Pch.h"

#include "Controllers/ControlCapture.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/MappingEvaluator.h"





enum class PaddleTarget
{
    Pdl0,
    Pdl1,
    Pb0,
    Pb1,
    Pb2,
};





enum class CalibrationStep
{
    None,
    Center,   // the stick left at rest; the reading becomes its center
    Travel,   // the stick moved through its full travel; the limits widen
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPageState
//
//  What the Controllers page edits, with no window and no device behind it.
//  The page shows it and forwards clicks to it; the Settings sheet's apply
//  pipeline reads the result on OK and discards it on Cancel.
//
//  EVERY EDIT IS PENDING. The model and calibration maps are copies taken
//  when the page opened, and nothing here reaches the controller service:
//  what the user tries on this page drives the game port only once they
//  press OK, and Cancel puts every copy back as it was, calibrations
//  included.
//
//  THE MULTIPLAYER SETUP IS THE ONE EXCEPTION, and deliberately so. Which
//  controller each player holds and what it maps to is a MACHINE INPUT
//  setting, the same kind of choice the toolbar's paddle picker makes, not an
//  edit to a profile. Those take effect when they are made and are saved for
//  the machine, so these do too: a change here reaches the service at once
//  and Cancel does not take it back.
//
//  Edits go to the chosen profile of the selected controller's model, which
//  starts as the machine's active profile. A name the model has no profile
//  for edits its Default. A model with nothing saved starts from its built-in
//  default mapping and deadzone, which becomes its Default profile the moment
//  it is edited. Creating, renaming and deleting profiles is pending too, and
//  the chosen profile becomes the machine's active profile on OK.
//
////////////////////////////////////////////////////////////////////////////////

class ControllersPageState
{
public:

    struct ControllerEntry
    {
        ControllerUnitKey       unit;
        std::wstring            description;
        std::vector<ControlId>  controls;
        bool                    isConnected = true;
    };

    // The page opens. `hasPb2` is false on a machine whose $C063 is not a
    // pushbutton, the //c, where the PB2 target is unavailable.
    // `activeProfile` is the machine's active profile, empty for Default.
    // `selection` is the machine's selected controller, which the page opens
    // on; with none, or one not attached, it opens on the first attached.
    void  Load (const std::vector<ControllerDeviceInfo>              & devices,
                const std::map<std::string, ControllerModelSettings> & models,
                const std::map<std::string, ControllerCalibration>   & calibrations,
                bool                                                   hasPb2,
                const std::string                                    & activeProfile = std::string(),
                const std::optional<ControllerUnitKey>               & selection     = std::nullopt);

    // The machine's display name, for saying which machine a target is not
    // supported on.
    void                  SetMachineName (const std::wstring & name) { m_machineName = name; }
    const std::wstring &  GetMachineName () const                    { return m_machineName; }

    // Controllers came or went while the page is open. One that left keeps
    // its row and its edits, shown as not connected, so unplugging a cable by
    // accident does not throw away the user's work.
    void  UpdateDevices (const std::vector<ControllerDeviceInfo> & devices);

    const std::vector<ControllerEntry> &  GetControllers     () const;
    std::optional<size_t>                 GetSelectedIndex   () const;
    void                                  SelectController   (size_t index);
    bool                                  IsCalibratable     () const;

    // The machine's two-player setup and how many paddle axes it has. Applied
    // at once rather than on OK; see the note above.
    using MultiplayerChangedFn = std::function<void (const MultiplayerSetup &)>;

    void                      SetMultiplayer          (const MultiplayerSetup & setup, size_t axisCount);
    void                      SetOnMultiplayerChanged (MultiplayerChangedFn onChanged);
    const MultiplayerSetup &  GetMultiplayer          () const;
    size_t                    GetAxisCount            () const;
    bool                      IsMultiplayerEnabled    () const;

    // One player's controller, or their target. Both normalize, so a slot that
    // cannot be played beside the other one is emptied rather than kept.
    void  SetMultiplayerUnit   (size_t player, const std::optional<ControllerUnitKey> & unit);
    void  SetMultiplayerTarget (size_t player, PlayerAxisTarget target);

    // What one slot may map to on this machine, less what the other holds.
    std::vector<PlayerAxisTarget>  GetTargetChoices (size_t player) const;

    // Which player holds the controller being edited, or none.
    std::optional<size_t>          FindEditedPlayer () const;

    // Whether a target of the controller being edited drives anything. With
    // multiplayer off every target is in play, which is what a machine has
    // always done. With it on, a player drives the paddles their slot maps to
    // -- PDL0 and PDL1 for a joystick, PDL0 alone for a single paddle -- and
    // one button line, which is their PB0 bindings whichever line it lands on
    // (FR-039). A controller in neither slot drives nothing.
    bool                           IsTargetInPlay   (PaddleTarget target) const;

    static std::wstring            GetTargetLabel   (PlayerAxisTarget target);

    const ControlMapping &                GetMapping         () const;
    float                                 GetDeadzone        () const;
    void                                  SetDeadzone        (float deadzone);

    bool                                  IsTargetAvailable  (PaddleTarget target) const;
    bool                                  AddAxisBinding     (PaddleTarget target, const AxisBinding & binding);
    bool                                  AddButtonBinding   (PaddleTarget target, const ButtonBinding & binding);
    bool                                  RemoveBinding      (PaddleTarget target, size_t index);

    // The control on one row of a target, in place: the rest of the target's
    // controls keep their order.
    bool                                  ReplaceAxisBinding   (PaddleTarget target, size_t index, const AxisBinding & binding);
    bool                                  ReplaceButtonBinding (PaddleTarget target, size_t index, const ButtonBinding & binding);
    bool                                  SetInverted        (PaddleTarget target, size_t index, bool inverted);
    bool                                  SetResponse        (PaddleTarget target, size_t index, AxisResponse response, float maxSpeed);
    bool                                  SetThreshold       (PaddleTarget target, size_t index, float threshold);

    // The selected model's profiles, Default first, and the one being edited.
    std::vector<std::string>              GetProfileNames          () const;
    std::string                           GetEditedProfileName     () const;
    bool                                  IsEditingDefaultProfile  () const;
    void                                  SelectProfile            (const std::string & name);
    ProfileEditResult                     CheckProfileName         (const std::string & name, bool isRename) const;
    ProfileEditResult                     CreateProfile            (const std::string & name, ProfileSource source, const std::string & sourceName);
    ProfileEditResult                     RenameProfile            (const std::string & newName);
    ProfileEditResult                     DeleteProfile            ();
    void                                  ResetProfile             ();

    // Whether the edited profile's mapping differs from when it was opened or
    // last switched to, and a way to put it back.
    bool                                  HasUnappliedProfileEdits () const;
    void                                  DiscardProfileEdits      ();

    // Commits the selected model's settings as edited -- every profile's
    // mapping, the profiles created, renamed and deleted, and the deadzone --
    // ahead of OK. `commit` receives every model and calibration as they would
    // stand committed: the other models' pending edits and all pending
    // calibrations left out. The committed model joins the baseline only when
    // `commit` succeeds, so Cancel no longer reverts it. The machine's active
    // profile does not change.
    using CommitFn = std::function<HRESULT (const std::map<std::string, ControllerModelSettings> & models,
                                            const std::map<std::string, ControllerCalibration>   & calibrations)>;
    HRESULT                               SaveProfileEdits         (const CommitFn & commit);

    // The profile that becomes the machine's active one on OK, empty for
    // Default, and whether it differs from the one the page opened on.
    bool                                  HasActiveProfileChanged  () const;
    const std::string &                   GetActiveProfileName     () const;

    // The two lines shown for a refused name; false when the result is not a
    // name error.
    static bool                           TryDescribeNameError     (ProfileEditResult result, std::wstring & outLabel, std::wstring & outRule);

    // Every control assigned to more than one target (FR-025).
    std::vector<ControlId>                GetSharedControls  () const;

    // Press-to-assign on one target. Feeding a reading that activates a
    // control ends the wait and puts the control on the row `replaceIndex`
    // names, or adds it as a new row when that is absent.
    void                                  BeginCapture       (PaddleTarget target, const ControllerSample & baseline, std::optional<size_t> replaceIndex = std::nullopt);
    bool                                  FeedCapture        (const ControllerSample & sample);
    void                                  CancelCapture      ();
    bool                                  IsCapturing        () const;

    // The Calibrate action: Center, then Travel, then a pending user
    // calibration for the selected unit.
    void                                  BeginCalibration   ();
    void                                  FeedCalibration    (const ControllerSample & sample);
    void                                  AdvanceCalibration ();
    void                                  CancelCalibration  ();
    CalibrationStep                       GetCalibrationStep () const;
    void                                  UseAutomaticCalibration ();

    // What the game port would read from this reading under the edits: the
    // pending calibration, then the pending mapping and deadzone.
    GamePortContribution                  ComputeLiveReading (const ControllerSample & sample);

    bool                                  IsDirty            () const;
    void                                  Revert             ();
    void                                  MarkCommitted      ();

    const std::map<std::string, ControllerModelSettings> &  GetModels       () const;
    const std::map<std::string, ControllerCalibration> &    GetCalibrations () const;

private:

    const ControllerEntry *          GetSelected          () const;
    ControllerModelSettings *        EnsureSelectedModel  ();
    const ControllerModelSettings *  FindSelectedModel    () const;
    const ControllerProfile *        FindEditedProfile    () const;
    ControllerProfile *              EnsureEditedProfile  ();
    bool                             FindCommittedMapping (ControlMapping & mapping) const;
    std::string                      GetCommittedName     (const std::string & token, const std::string & name) const;

    static std::vector<AxisBinding> *    FindAxisList    (ControlMapping & mapping, PaddleTarget target);
    static std::vector<ButtonBinding> *  FindButtonList  (ControlMapping & mapping, PaddleTarget target);
    static bool                          IsAxisTarget    (PaddleTarget target);

    std::vector<ControllerEntry>                    m_controllers;
    std::optional<size_t>                           m_selected;
    bool                                            m_hasPb2       = true;
    std::wstring                                    m_machineName;

    std::map<std::string, ControllerModelSettings>  m_models;
    std::map<std::string, ControllerCalibration>    m_calibrations;
    std::map<std::string, ControllerModelSettings>  m_baselineModels;
    std::map<std::string, ControllerCalibration>    m_baselineCalibrations;

    // The edited profile's name as chosen, empty for Default, and as it was
    // when the page opened or last committed.
    std::string                                     m_editedProfile;
    std::string                                     m_baselineProfile;

    // Per model token, a profile's name as edited mapped to its name in the
    // committed settings. An empty committed name marks a profile created on
    // the page, which the committed settings do not have yet.
    std::map<std::string, std::map<std::string, std::string>>  m_committedNames;

    // The machine's mode, applied as it is edited rather than on OK, so it
    // has no baseline and takes no part in IsDirty or Revert.
    MultiplayerSetup                                m_multiplayer;
    size_t                                          m_axisCount = GamePortContribution::kAxisCount;
    MultiplayerChangedFn                            m_onMultiplayerChanged;

    ControlCapture                                  m_capture;
    PaddleTarget                                    m_captureTarget = PaddleTarget::Pdl0;
    std::optional<size_t>                           m_captureReplace;

    CalibrationStep                                 m_calibrationStep = CalibrationStep::None;
    ControllerSample                                m_calibrationLast;
    ControllerCalibration                           m_calibrationDraft;

    MappingEvaluator                                m_liveEvaluator;
    ControlMapping                                  m_emptyMapping;

    // What GetMapping hands back for a model with no saved Default profile.
    // Rebuilt on each call, and held here so the reference it returns
    // outlives the call.
    mutable ControlMapping                          m_builtInMapping;
};
