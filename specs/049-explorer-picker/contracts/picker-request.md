# Contract: opening a picker

Two entry points, one for each form. Types are in [../data-model.md](../data-model.md).

## Path picker, modal: `IHostDialogs::PickFileToOpen` and `PickFileToSave`

The interface's methods do not change. `FileDialogSpec` gains two fields:

```cpp
enum class FileDialogContent { Any, Images };

struct FileDialogSpec
{
    // ... existing fields ...
    FileDialogContent              content        = FileDialogContent::Any;
    std::optional<ReceivingDrive>  receivingDrive;
};
```

`CassoHostDialogs` is the `IHostDialogs` of the emulator and of every Casso Explorer window. It wraps `Win32HostDialogs`.

| Request | Goes to |
|---|---|
| `PickFileToOpen`, `content == Images` | the Casso Explorer picker, modal over `owner` |
| `PickFileToOpen`, `content == Any` | `Win32HostDialogs` |
| `PickFileToSave`, `content == Images` | the Casso Explorer picker in its save form, modal over `owner` |
| `PickFileToSave`, `content == Any` | `Win32HostDialogs` |
| `PickFolder` | `Win32HostDialogs` |

Results are the seam's existing three: success with `outPicked` true and a path, success with `outPicked` false for a cancel, or a failure. A picker that cannot be created is a failure, not a silent fall back to the Windows dialog.

Behavior the caller can rely on:
- `filters` empty is treated as one entry, "All files" with `*.*`.
- The first filter entry is active. With one entry there is no filter choice.
- `initialFolder` that is missing is ignored; the picker opens on its saved folder for the request, else the emulator's disk folder.
- With `receivingDrive`, a file is listed only when the masks match and the drive rule accepts it. An "All files" entry widens the masks only.
- From `PickFileToOpen`, the returned path is a file that existed and was listed when the user confirmed.
- From `PickFileToSave`, the returned path is in a folder that exists, has `defaultExtension` when the typed name had no extension, and may or may not exist. With `askToReplace` true the user has already confirmed a replacement; with it false the caller prompts.
- A save request never starts on the Casso node: `initialFolder`, else the request's saved folder, else the folder new disks are created in.

Image requests in Casso Explorer that set `content = Images`: "Write image to..." (open, with the connected drive as `receivingDrive`), "Create image from...", "Capture raw disk from..." and "Save CassoLink disk image" (save). In the emulator: the Hardware page, the disk compare dialog, "Save disk as" and the new tape request. Raw sector and block dumps (`*.bin`) and the cassette bootstrap (`*.wav`) are not image requests.

A test replaces `IHostDialogs` with `FakeHostDialogs`, as today; `CassoHostDialogs` is tested with a fake inner `IHostDialogs` and a fake picker runner.

## Drive picker, not modal: `ShellDrivePickers`

```cpp
class ShellDrivePickers
{
public:
    //  Opens the drive's picker, or brings the open one to the front.
    HRESULT  Open     (const DriveAddress & address, const RECT * anchorClientPx = nullptr);

    bool     IsOpen   (const DriveAddress & address) const;
    void     Close    (const DriveAddress & address);      // without inserting
    void     CloseAll ();                                  // machine switch, emulator closing
};
```

- One picker per `DriveAddress`. A second `Open` for the same address creates nothing.
- A picker's drive never changes after `Open`.
- Confirm: `ConfirmInspectorDiscard` for a Disk II drive, then the mount (`ShellDisks::Mount`, or `ShellTapeDeck::InsertTape` for the recorder). A failed mount is reported in the picker and the picker stays open. A successful mount adds the image to the recent list, then closes the picker.
- `Close` and `CloseAll` never mount.
- Callers replaced: `ShellDisks::BrowseForDisk`, `ShellDeskScene::BrowseFloppy35` and `BrowseExtraDrive`, the drive bar's click, `EmulatorShell::ShowDriveContextMenu`'s insert item, the Disk menu's insert commands and their shortcuts, and the tape deck's insert.
