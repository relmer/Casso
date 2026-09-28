#pragma once

#include "Pch.h"

#include "Controllers/ControlCapture.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/InputModeRules.h"
#include "Controllers/MappingEvaluator.h"
#include "Controllers/PlayerSlotPolicy.h"





enum class PaddleTarget
{
    Pdl0,
    Pdl1,
    Pb0,
    Pb1,
    Pb2,
};





// Which Joyport jack a controller drives: its multiplayer slot's, or both
// when it plays alone.
enum class JoyportJack
{
    None,
    Left,
    Right,
    Both,
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
//  THE PLAYERS ARE THE ONE EXCEPTION, and deliberately so. Each player's
//  entry, what it maps to, and the Multiplayer checkbox are the same choices
//  the command bar's picker makes, not edits to a profile. Those take effect
//  when they are made, so these do too: a pick here reaches the service at
//  once and Cancel does not take it back.
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
        ControllerFormFactor    formFactor  = ControllerFormFactor::Gamepad;
        bool                    isConnected = true;
    };

    // The page opens. `hasPb2` is false on a machine whose $C063 is not a
    // pushbutton, the //c, where the PB2 target is unavailable.
    // `activeProfiles` is every controller's active profile by unit token,
    // empty for Default. `selection` is the machine's selected controller,
    // which the page opens on; with none, or one not attached, it opens on the
    // first attached.
    void  Load (const std::vector<ControllerDeviceInfo>              & devices,
                const std::map<std::string, ControllerModelSettings> & models,
                const std::map<std::string, ControllerCalibration>   & calibrations,
                bool                                                   hasPb2,
                const std::map<std::string, std::string>             & activeProfiles,
                const std::optional<ControllerUnitKey>               & selection);

    // The same, with one name as the active profile of the controller the
    // page opens on and no other controller's recorded.
    void  Load (const std::vector<ControllerDeviceInfo>              & devices,
                const std::map<std::string, ControllerModelSettings> & models,
                const std::map<std::string, ControllerCalibration>   & calibrations,
                bool                                                   hasPb2,
                const std::string                                    & openedProfile = std::string(),
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
    std::optional<size_t>                 FindController     (const ControllerUnitKey & unit) const;
    void                                  SelectController   (size_t index);
    bool                                  IsCalibratable     () const;

    static constexpr size_t  kPlayerCount = PlayerSlotPolicy::kPlayerCount;
    static constexpr size_t  kPlayerTwo   = 1;

    // The players as the service holds them -- each one's entry and what each
    // slot plays -- and how many paddle axes the machine has. A pick made on
    // the page is applied at once rather than on OK; see the note above.
    using PlayerPickedFn = std::function<void (size_t player, const PlayerEntry & entry)>;

    void                   SetPlayers            (const PlayerEntries & entries, const PlayerSlots & slots, size_t axisCount);
    void                   SetOnPlayerPicked     (PlayerPickedFn onPicked);
    const PlayerEntries &  GetPlayerEntries      () const;
    const PlayerSlots &    GetPlayerSlots        () const;
    size_t                 GetAxisCount          () const;

    // Whether two people are playing, which decides what the rest of the
    // page edits, and the controller each player plays, if any.
    bool                              IsMultiplayerEnabled  () const;
    std::optional<ControllerUnitKey>  GetPlayerUnit         (size_t player) const;

    // The Multiplayer checkbox: Player 2's entry seen another way.
    bool                   IsMultiplayerChecked  () const;
    void                   SetMultiplayerChecked (bool isChecked);

    // One player's entry, or their target; no target follows the profile.
    void                   PickPlayerEntry       (size_t player, PlayerEntry entry);
    void                   SetPlayerTarget       (size_t player, std::optional<PlayerAxisTarget> target);

    // What one player's entry drop-down lists.
    std::vector<InputModeRules::PlayerChoice>  GetEntryChoices (size_t player) const;

    // The Joyport jack the controller in Editing drives, for the page's
    // switch lights, and the heading the page shows above them.
    JoyportJack               GetJoyportJack          () const;
    static std::wstring       GetJoyportHeading       (JoyportJack jack);

    // What a row drives while the Joyport is attached: PDL0 the left and
    // right switches, PDL1 up and down, PB0 fire. PB1 and PB2 drive nothing,
    // so the page leaves them out; their bindings stay in the profile.
    static bool               IsJoyportTarget         (PaddleTarget target);
    static std::wstring       GetJoyportRowLabel      (PaddleTarget target);

    // The label beside one position of the Joyport's Apple / Atari switch.
    static std::wstring       GetJoyportPositionLabel (bool isAtariMode);


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

    // What one of the edited controller's targets drives on THIS machine while
    // two people play: the paddle its slot lands on, or the player whose
    // button line it is. Empty outside the mode, where the controller's own
    // names are the answer.
    std::wstring                   GetTargetPlayLabel (PaddleTarget target) const;

    const ControlMapping &                GetMapping         () const;
    float                                 GetDeadzone        () const;
    void                                  SetDeadzone        (float deadzone);

    bool                                  IsTargetAvailable  (PaddleTarget target) const;
    std::wstring                          GetUnassignedLabel (PaddleTarget target) const;
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

    // The mode whose profiles the page lists and whose active profiles it
    // edits: with a Joyport attached or without. A controller with no
    // profile chosen in it plays that mode's built-in profile. Set before
    // Load, or after it to swap the list and the edited profile in place.
    void                                  SetProfileMode           (ProfileMode mode);
    ProfileMode                           GetProfileMode           () const           { return m_profileMode; }

    // Every controller's active profile in the mode the page is not in, by
    // unit token, set after Load; the page edits it once SetProfileMode
    // switches to that mode.
    void                                  SetOtherModeActiveProfiles (const std::map<std::string, std::string> & activeProfiles);

    // The selected model's profiles of the page's mode, its built-in profile
    // first; the ones of them a new profile can be a copy of; and the one
    // being edited.
    std::vector<std::string>              GetProfileNames          () const;
    std::vector<std::string>              GetCopySourceNames       () const;
    std::string                           GetEditedProfileName     () const;
    bool                                  IsEditingBuiltInProfile  () const;
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

    // Every controller's active profile as it stands on the page, by unit
    // token, for the page's mode or for either, which becomes the service's
    // on OK; whether any differs from when the page opened; and the edited
    // controller's, empty for the mode's built-in profile.
    const std::map<std::string, std::string> &  GetActiveProfiles () const { return m_activeProfiles; }
    const std::map<std::string, std::string> &  GetActiveProfiles (ProfileMode mode) const;
    bool                                  HasActiveProfileChanged  () const;
    bool                                  HasActiveProfileChanged  (ProfileMode mode) const;
    const std::string &                   GetActiveProfileName     () const;

    // The two lines shown for a refused name; false when the result is not a
    // name error.
    static bool                           TryDescribeNameError     (ProfileEditResult result, std::wstring & outLabel, std::wstring & outRule);

    // What a new profile can start from in a mode, in the order the New
    // profile dialog lists them, and each one's label there.
    static std::vector<ProfileSource>     GetStartingPoints        (ProfileMode mode, bool canCopy);
    static std::wstring                   GetStartingPointLabel    (ProfileSource source);

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
    ControllerProfileKind            GetEditedBuiltInKind () const;
    bool                             IsNameOfOtherMode    (const std::string & name) const;
    bool                             FindCommittedMapping (ControlMapping & mapping) const;
    MultiplayerSetup                 MakePlayView         () const;
    void                             ApplyPlayerEntry     (size_t player, const PlayerEntry & entry);
    std::string                      GetCommittedName     (const std::string & token, const std::string & name) const;

    static std::vector<AxisBinding> *    FindAxisList    (ControlMapping & mapping, PaddleTarget target);
    static std::vector<ButtonBinding> *  FindButtonList  (ControlMapping & mapping, PaddleTarget target);
    static bool                          IsAxisTarget    (PaddleTarget target);

    std::vector<ControllerEntry>                    m_controllers;
    std::vector<ControllerDeviceInfo>               m_devices;
    std::optional<size_t>                           m_selected;
    bool                                            m_hasPb2       = true;
    std::wstring                                    m_machineName;

    std::map<std::string, ControllerModelSettings>  m_models;
    std::map<std::string, ControllerCalibration>    m_calibrations;
    std::map<std::string, ControllerModelSettings>  m_baselineModels;
    std::map<std::string, ControllerCalibration>    m_baselineCalibrations;

    // The edited controller's active profile as chosen, empty for none, which
    // plays the built-in profile of m_profileMode. It is that
    // controller's entry in m_activeProfiles, loaded when Editing moves to it
    // and written back whenever it changes.
    std::string                                     m_editedProfile;
    ProfileMode                                     m_profileMode = ProfileMode::Normal;

    // Every controller's active profile, by unit token, and as it was when
    // the page opened or last committed: for the page's mode, and set aside
    // for the other mode until SetProfileMode swaps them.
    std::map<std::string, std::string>              m_activeProfiles;
    std::map<std::string, std::string>              m_baselineActiveProfiles;
    std::map<std::string, std::string>              m_otherActiveProfiles;
    std::map<std::string, std::string>              m_baselineOtherActiveProfiles;

    void  LoadEditedProfile      ();
    void  StoreEditedProfile     ();
    void  RetargetActiveProfiles (const std::string & modelToken, const std::string & from, const std::string & to);

    static bool  IsEachPlayedAlike (const std::map<std::string, std::string> & from, const std::map<std::string, std::string> & in);

    // Per model token, a profile's name as edited mapped to its name in the
    // committed settings. An empty committed name marks a profile created on
    // the page, which the committed settings do not have yet.
    std::map<std::string, std::map<std::string, std::string>>  m_committedNames;

    // The players, applied as they are picked rather than on OK, so they have
    // no baseline and take no part in IsDirty or Revert.
    //
    // THE SLOTS ARE AS PLAYED, not as chosen. The page shows the settings for
    // what the machine is actually playing, so it never disagrees with the
    // picker: a machine that fell back to one controller because a player's
    // is not attached shows the single-player settings.
    PlayerEntries                                   m_entries;
    PlayerSlots                                     m_slots;
    size_t                                          m_axisCount = GamePortContribution::kAxisCount;
    PlayerPickedFn                                  m_onPlayerPicked;

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
