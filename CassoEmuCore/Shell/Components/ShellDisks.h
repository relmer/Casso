#pragma once

#include "Pch.h"

#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/IDriveCommandSink.h"



class DiskManager;
class EmulatorShell;
class IFileSystem;
class SalvageDialogContent;
class UserConfigStore;
struct DialogDefinition;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDisks
//
//  The Disk II drives as the emulator presents them: the DiskManager that
//  mounts and ejects through the CPU thread, the two drive widgets and the
//  state they share with it, the recent-disks list, salvage, the write-
//  protect preference, the second drive's connection, and the notice raised
//  when an image changes outside Casso. The drives themselves -- the
//  controller and the image store -- are the machine's.
//
//  It is the drive widgets' command sink: a drop, a click to browse or the
//  eject affordance arrives here and is queued to the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDisks : public IDriveCommandSink
{
public:
    explicit ShellDisks (EmulatorShell & shell);
    ~ShellDisks();

    // Startup, once the asset base directory and the config store exist.
    // `imageWatchDisabled` is --no-image-watch, which installs a watcher that
    // refuses every watch so the check before each write can be measured.
    void  Initialize (UserConfigStore & configStore, IFileSystem & fileSystem, bool imageWatchDisabled);

    // Every mount reports its outcome through here, not just the startup
    // ones: the recent-disks entry, the damage check, and the failure report
    // all hang off it.
    void  InstallMountReporting  ();

    // Installs the two sinks the image store reports through: the
    // non-blocking banner, and the question. Both bounce to the UI thread.
    void  InstallChangeReporting ();

    // Null before Initialize.
    DiskManager *                      GetManager          () const { return m_diskManager.get(); }
    std::array<DriveWidget, 2>       & GetDriveChrome      ()       { return m_driveChrome; }
    std::array<DriveWidgetState, 2>  & GetDriveWidgetState ()       { return m_driveWidgetState; }
    DriveWidgetController            & GetDriveWidgets     ()       { return m_driveWidgets; }

    // Live per-drive user write-protect preference (Settings > Disk
    // checkbox / write-protect menu). Seeded from $cassoUiPrefs at
    // startup and re-applied to each freshly mounted image so the guest
    // sees the disk as protected and dirty writes never flush. Distinct
    // from the image's own embedded flag and from the backing file's
    // read-only state; all three are surfaced independently in the UI.
    std::array<bool, 2>  & GetUserWriteProtect () { return m_userWriteProtect; }

    // Records the user's per-drive write-protect preference and applies it to
    // the currently mounted image (if any) so the change takes effect
    // immediately. CPU thread.
    void  SetDriveUserWriteProtect (int drive, bool wp);

    // IDriveCommandSink
    // UI-thread entry points the drive widgets call into when the user
    // drops a file, clicks-to-browse, or clicks the eject affordance.
    // Both forms route through the existing IDM_DISK_* command queue so
    // the actual mount/eject runs on the CPU thread same as the menu
    // path. `Mount` accepts only slot 6 (the integrated Disk II);
    // unknown slots are E_INVALIDARG and the mount is dropped.
    HRESULT  Mount (int slot, int drive, const std::wstring & path) override;
    void     Eject (int slot, int drive) override;

    // UI helper: open the drive door for visual feedback, show the
    // file-open dialog, then close the door again. Mount-on-success
    // is handled by the existing PromptForDiskImage path; this
    // method just owns the door visual. `anchorClientPx` is the clicked
    // drive in client pixels, which the picker opens below; null (the menu
    // and accelerator path) centers the picker on the window.
    void  BrowseForDisk (int drive, const RECT * anchorClientPx = nullptr);

    // Push a freshly mounted disk image onto the recent-disks MRU
    // and persist user prefs. Best-effort; never propagates failures
    // back into the mount path. Takes the mount's own HRESULT and hands
    // it to DiskMru, which drops anything that did not actually mount.
    void  RecordRecentDisk (const std::wstring & path, HRESULT mountResult);

    // Whether the Disk menu offers the write-protect toggle for a drive, and
    // salvage: only a damaged image with ordinary 16-sector structure can be
    // rebuilt from its sectors.
    bool  IsWriteProtectToggleOffered (int drive);
    bool  IsSalvageOffered            (int drive);

    // The whole salvage interaction: assess, show the figures, write the copy
    // on confirmation, then offer to insert it.
    void  RunSalvageFlow (int drive);

    // Reports a freshly mounted image that failed its stored checksum, with
    // salvage offered inline. Raised here rather than by the loader because a
    // dialog with an action on it is the shell's business, and EhmNotifyUser
    // carries a string and nothing else.
    void  ReportDamagedMount (int drive);

    // Whether the second drive is shown, and whether this machine has anywhere
    // to plug one. The //c's external drive is an optional add-on, shown only
    // while connected; everywhere else the second drive is whatever is on the
    // Disk ][ card's second connector.
    bool  ShouldShowExternalDrive  () const;
    bool  IsSecondDriveOffered     () const;
    bool  IsExternalDriveConnected () const         { return m_externalDriveConnected; }
    void  SetExternalDriveConnected (bool connected) { m_externalDriveConnected = connected; }

    // Whether the machine's drive is soldered in rather than plugged into a
    // card, which decides the drive the desk scene draws.
    bool  MachineHasBuiltInDrive () const;

    // The info icon's tooltip for a drive: what its WOZ image declares about
    // the machine, and what conflicts with the one running. Empty when nothing
    // does.
    std::wstring  ComposeDriveInfoTooltip (int drive) const;

    // For the Settings > Theme preview: each drive's mounted image, its
    // write-protect breakdown, and its head position and activity, read from
    // the live widget state. Index 0 is drive 1.
    const std::wstring &  GetMountedImagePath  (int driveIndex) const;
    WriteProtectInfo      GetDriveWriteProtect (int driveIndex) const;
    void                  SampleDriveActivity  (int driveIndex, DriveWidgetState & outState) const;

    // One attempted mount's outcome, carried from the thread that ran the
    // mount to the UI thread that reacts to it. Plain data, and used only as
    // a parameter, so it rides along in this header.
    //
    // The path stays in the narrow form the store and the DiskManager use.
    // Widening it here and narrowing it again for the message would be a
    // round-trip through the platform encoding for no gain, and that is the
    // trip that mangles a non-ASCII filename.
    struct MountCompletion
    {
        std::string     path;
        MountDiagnosis  diagnosis;
        HRESULT         result = S_OK;
        int             drive  = 0;
    };

    // The UI-thread half: a successful mount enters the recent-disks list and
    // is checked for damage, a failed one is reported to the user. Posting to
    // get here is also what keeps a failed --disk1 from raising a modal inside
    // Initialize, before the message loop that would service it is running.
    void  HandleMountCompletion (const MountCompletion & completion);

    // What one bay's external change wants said, carried from the thread that
    // owns disk writes to the one that owns the screen.
    struct ChangeNotice
    {
        int           slot  = 0;
        int           drive = 0;
        ChangePrompt  prompt;
    };

    // Raises the non-modal banner over the running machine for a bay, and
    // puts the store's question to the user, routing the answer back to the
    // thread that owns disk writes.
    void  ShowChangeBanner (const ChangeNotice & notice);
    void  AskAboutChange   (const ChangeNotice & notice);

    // Non-modal notice over the running machine: a disk changed outside Casso.
    //
    // IT DOES NOT CLEAR ITSELF, and that is the design rather than an
    // oversight: the action it carries is the restart, which is what the user
    // reaches for once the program starts misbehaving, and a notice that faded
    // would take that action with it.
    DxuiActionBanner &  GetChangeBanner () { return m_changeBanner; }

    // How tall the notice's band is right now: zero when nothing is being
    // reported, and the height its wrapped text needs when something is.
    int   GetChangeBandThicknessPx (int clientWidthPx) const;

    // Lays the notice into the band the dock gave it.
    void  LayoutChangeBanner (const RECT & bandBounds);

    //  Closes it once its time is up, unless the pointer is resting on it.
    void  ExpireChangeBannerIfDue ();

    // Offers a mouse event to the message bar, if one is up.
    //
    // THE SHELL HIT-TESTS ITS CHROME BY NAME rather than walking the panel
    // tree -- the toolbar, the joystick selector and the //c switch strip are
    // each asked in turn -- so a control that is not on that list is painted
    // and never clicked. Measured: the bar drew correctly and its button could
    // not be pressed.
    bool  OfferMouseToChangeBanner (DxuiMouseEventKind kind, int x, int y);

private:
    // The DiskManager mount-completion hook. Runs on whichever thread ran the
    // mount -- the CPU thread for anything the user started, the UI thread for
    // the command-line disks -- and does nothing but get the outcome onto the
    // UI thread, where the MRU and the dialogs live.
    void  OnMountCompleted (int drive, const std::string & path, HRESULT mountResult,
                            const MountDiagnosis & diagnosis);

    // Shows a dialog whose body is a caller-built panel rather than text runs,
    // for content a string cannot carry (here: an aligned figures table and a
    // warning banner).
    int   ShowSalvageDialog (const DialogDefinition & def,
                             std::unique_ptr<SalvageDialogContent> content);

    // Asks where to save the contents of a disk whose file has gone.
    //
    // THE SAVE DIALOG, NOT THE DISK PICKER. The user is saving a disk here,
    // not choosing one to mount, and the two look similar enough that reaching
    // for the wrong one would be easy and baffling.
    //
    // Returns false where the user cancelled, which is the same outcome as
    // declining: the drive is emptied either way and nothing is written.
    bool  AskWhereToSaveLostDisk (const std::string & imagePath, std::wstring & outPath);

    //  Closes the change band and gives its height back to the picture.
    void  HideChangeBanner ();

    EmulatorShell                    & m_shell;

    std::array<DriveWidget, 2>         m_driveChrome;

    // Live per-drive user write-protect preference; see GetUserWriteProtect.
    std::array<bool, 2>                m_userWriteProtect { { false, false } };

    // Drive widget state pump. The controller channel publishes
    // per-drive door/spin sync events the chrome painter consumes. Per-drive
    // UI/CPU bridge state lives in m_driveWidgetState; the CPU thread's
    // motor + nibble counters are sampled once per UI frame and pushed
    // through the controller.
    DriveWidgetController              m_driveWidgets;
    std::array<DriveWidgetState, 2>    m_driveWidgetState;

    // //c only: whether the optional external drive is "connected". Mirrors
    // the per-machine $cassoUiPrefs.externalDriveConnected pref; seeded at
    // machine build and flipped live by IDM_DRIVE_EXTERNAL_CONNECT/DISCONNECT.
    // Gates the second drive-mount widget (m_driveChrome[1]) via
    // ShouldShowExternalDrive(). No effect on machines whose second drive is
    // fixed hardware (they have no banked ROM, so the gate is always open).
    bool                               m_externalDriveConnected = false;

    DxuiActionBanner                   m_changeBanner;

    //  When the change band closes itself, and the frame that last looked.
    //  Zero means it stands until dismissed. Hovering does not extend the
    //  wait, it suspends it: the deadline moves with the clock while the
    //  pointer is over the band, so what is left when the pointer leaves is
    //  what was left when it arrived.
    int64_t                            m_changeBannerHideAtMs = 0;
    int64_t                            m_changeBannerTickMs   = 0;

    std::unique_ptr<DiskManager>       m_diskManager;
};
