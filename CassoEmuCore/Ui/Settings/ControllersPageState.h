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
//  entry and mode are the same choices the command bar's picker makes, not
//  edits to a profile. Those take effect when they are made, so these do
//  too: a pick here reaches the service at once and Cancel does not take it
//  back.
//
//  The profiles listed and edited are of the kind the controller in Editing
//  plays: its player's mode, or Joyport while the Joyport is in effect.
//
//  Edits go to the chosen profile of the selected controller's model, which
//  starts as the machine's active profile. A name the model has no profile
//  for edits its Default. A model with nothing saved starts from its built-in
//  default mapping and dead zone, which becomes its Default profile the moment
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
    using PlayerModeFn   = std::function<void (size_t player, PlayerMode mode)>;

    void                   SetPlayers            (const PlayerEntries & entries, const PlayerSlots & slots, size_t axisCount);
    void                   SetOnPlayerPicked     (PlayerPickedFn onPicked);
    void                   SetOnPlayerModeSet    (PlayerModeFn onModeSet);
    const PlayerEntries &  GetPlayerEntries      () const;
    const PlayerSlots &    GetPlayerSlots        () const;
    size_t                 GetAxisCount          () const;

    // Whether two people are playing, which decides what the rest of the
    // page edits, and the controller each player plays, if any.
    bool                              IsMultiplayerEnabled  () const;
    std::optional<ControllerUnitKey>  GetPlayerUnit         (size_t player) const;


    // One player's entry, or their mode; a Joyport jack the other player
    // holds is not taken.
    void                   PickPlayerEntry       (size_t player, PlayerEntry entry);
    void                   SetPlayerMode         (size_t player, PlayerMode mode);

    // What one player's entry and mode drop-downs list, Player 2's Disabled
    // and Automatic among them, and the notice under a player whose buttons
    // the Joyport has taken, empty otherwise.
    std::vector<InputModeRules::PlayerChoice>      GetEntryChoices     (size_t player) const;
    std::vector<InputModeRules::PlayerModeChoice>  GetModeChoices      (size_t player) const;
    std::wstring                                   GetButtonsCutNotice (size_t player) const;

    // The heading above the input picture: what the controller in Editing
    // drives, "Joystick 0", "Paddle 1", "Paddles 2 and 3", "Atari joystick:
    // left jack" and the like, or a sentence saying it drives nothing here.
    std::wstring                                   GetEditedHeading    () const;

    static constexpr const wchar_t *  kpszNotUsedHeading = L"Not used on this machine";

    // Whether the controller in Editing plays for a player in a Joyport
    // jack, and whether the Joyport has taken its player's buttons.
    bool                      IsEditedOnJoyport       () const;
    bool                      AreEditedButtonsCut     () const;

    // Whether the controller in Editing plays paddles, in Paddle or Two
    // paddles mode, which the page shows as a bar per paddle rather than a
    // stick.
    bool                      IsEditedOnPaddles       () const;

    // The Joyport jack the controller in Editing drives, for the page's
    // switch lights, and the heading the page shows above them.
    JoyportJack               GetJoyportJack          () const;
    static std::wstring       GetJoyportHeading       (JoyportJack jack);

    // What a row drives for a player in a Joyport jack: PDL0 the left and
    // right switches, PDL1 up and down, PB0 fire. PB1 and PB2 drive nothing,
    // so the page leaves them out; their bindings stay in the profile.
    static bool               IsJoyportTarget         (PaddleTarget target);
    static std::wstring       GetJoyportRowLabel      (PaddleTarget target);


    // Which player holds the controller being edited while two play, or none.
    std::optional<size_t>          FindEditedPlayer () const;

    // Whether a target of the controller being edited drives anything: what
    // its player's place on the game port is wired to, which the players'
    // modes decide. A controller in no slot drives nothing while two play,
    // and is edited whole otherwise.
    bool                           IsTargetInPlay   (PaddleTarget target) const;

    // What one of the edited controller's targets drives on THIS machine: the
    // paddle or line the guest reads it on, when that is not the target's
    // own.
    std::wstring                   GetTargetPlayLabel (PaddleTarget target) const;

    const ControlMapping &                GetMapping         () const;
    float                                 GetDeadZone        () const;
    void                                  SetDeadZone        (float deadZone);

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

    // Whether an axis's options hold a paddle speed, which only a Paddle
    // profile's do, and whether they offer Position beside it, which only a
    // knob's do: its first analog binding a DirectInput axis on a controller
    // that is not a gamepad.
    bool                                  IsPaddleSpeedOffered () const;
    bool                                  IsPositionOffered    (PaddleTarget target) const;

    // Whether the running machine has a Joyport, which puts its jacks among
    // the players' modes, set before Load or after it to swap the list and
    // the edited profile in place. The kind whose profiles the page lists and
    // whose active profiles it edits follows the edited player's mode. A
    // controller with no profile chosen of that kind plays its built-in one.
    // The Joyport is on while a player is in one of its jacks.
    void                                  SetJoyportAvailable      (bool hasJoyport);
    bool                                  HasJoyport               () const           { return m_hasJoyport; }
    bool                                  IsJoyportInEffect        () const;
    ProfileMode                           GetProfileMode           () const           { return m_profileMode; }

    // Every controller's active profile for one kind, by unit token, set
    // after Load; the page edits it while it is in that kind.
    void                                  SetActiveProfiles        (ProfileMode mode, const std::map<std::string, std::string> & activeProfiles);

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
    // mapping, the profiles created, renamed and deleted, and the dead zone --
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
    const std::map<std::string, std::string> &  GetActiveProfiles () const;
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

    // The targets one control is assigned to, in the page's row order.
    std::vector<PaddleTarget>             GetControlTargets  (const ControlId & control) const;

    // The target a shared control's warning goes under: the one it was last
    // assigned to on the page, while it is still there, and otherwise the
    // last of its targets in row order.
    PaddleTarget                          GetSharedWarningTarget (const ControlId & control) const;

    // Items joined into one run of English: "A", "A and B", "A, B and C".
    static std::wstring                   JoinWithAnd        (const std::vector<std::wstring> & items);

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
    // pending calibration, then the pending mapping and dead zone. A rate
    // binding's paddle moves for `elapsedSeconds`, which the evaluator caps;
    // a reading taken only for its buttons passes 0 and moves nothing.
    GamePortContribution                  ComputeLiveReading (const ControllerSample & sample, float elapsedSeconds = MappingEvaluator::kMaxRateStep);

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
    void                             SyncPlayers          ();
    void                             SyncProfileMode      ();
    ProfileMode                      GetEditedPlayerProfileMode () const;
    std::optional<size_t>            FindHoldingPlayer    () const;
    std::string                      GetCommittedName     (const std::string & token, const std::string & name) const;
    AxisResponse                     GetAllowedResponse   (const AxisBinding & binding) const;

    // Every control on every target of the edited mapping, a pair per row.
    std::vector<std::pair<ControlId, PaddleTarget>>  ListControlUses() const;

    // Records the target a binding was just put on, for each of its controls.
    void                                             NoteAssigned   (PaddleTarget target, const AxisBinding & binding);
    void                                             NoteAssigned   (PaddleTarget target, const ControlId & control);

    static constexpr size_t  kPaddlesPerJoystick = 2;

    JoyportJack                          GetPlayerJack      (size_t player) const;
    std::wstring                         GetPlayerHeading   (size_t player) const;
    static std::wstring                  GetJoyportJackNote (JoyportJack jack);

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

    using ProfilesByMode = std::array<std::map<std::string, std::string>, ControllerProfileStore::kProfileModeCount>;

    // The edited controller's active profile as chosen, empty for none, which
    // plays the built-in profile of m_profileMode. It is that
    // controller's entry in the kind's map, loaded when Editing moves to it
    // and written back whenever it changes.
    std::string                                     m_editedProfile;
    ProfileMode                                     m_profileMode       = ProfileMode::Joystick;
    bool                                            m_hasJoyport        = false;

    // Every controller's active profile, by unit token, for each kind, and as
    // it was when the page opened or last committed.
    ProfilesByMode                                  m_activeProfiles;
    ProfilesByMode                                  m_baselineActiveProfiles;

    void                                  LoadEditedProfile      ();
    void                                  StoreEditedProfile     ();
    void                                  RetargetActiveProfiles (const std::string & modelToken, const std::string & from, const std::string & to);
    std::map<std::string, std::string> &  GetModeProfiles        ();

    static size_t                         GetModeIndex           (ProfileMode mode);

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
    PlayerModeFn                                    m_onPlayerModeSet;

    ControlCapture                                  m_capture;
    PaddleTarget                                    m_captureTarget = PaddleTarget::Pdl0;
    std::optional<size_t>                           m_captureReplace;

    // The target each control was last assigned to on the page, cleared
    // whenever the edited mapping is loaded, switched or reset.
    std::vector<std::pair<ControlId, PaddleTarget>> m_assignedTargets;

    CalibrationStep                                 m_calibrationStep = CalibrationStep::None;
    ControllerSample                                m_calibrationLast;
    ControllerCalibration                           m_calibrationDraft;

    MappingEvaluator                                m_liveEvaluator;
    ControlMapping                                  m_emptyMapping;

    // What GetMapping hands back: the edited profile's mapping, or the
    // built-in one for a model with nothing saved, with each response as the
    // profile's kind allows. Rebuilt on each call, and held here so the
    // reference it returns outlives the call.
    mutable ControlMapping                          m_shownMapping;
};
