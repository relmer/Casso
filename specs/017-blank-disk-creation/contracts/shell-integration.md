# Contract: Shell integration (picker row, dialog, mount, write-protect)

## 1. Pinned picker row (R-009, FR-001)

`DiskMruPickerSession` (Casso/AssetBootstrap.cpp) gains a sentinel row:

- `ModelRow` with a reserved result code `s_kCreateNewResult` and display name
  `<Create new disk...>` (Location/Last-loaded cells empty).
- `RebuildView`: `rowPasses` always admits the sentinel (immune to the search
  filter); `sortLess` orders it first for every sort column and direction.
- `AssetBootstrap::PromptInsertDiskMru` decodes the code into a new out-signal
  (alongside the existing browse/cancel signals). The caller
  (`WindowCommandManager::PromptInsertDiskMru`) then runs the create flow for
  the same drive, inside the same `BeginModalKeepAlive` span the door
  choreography already holds (the drive door stays open through the dialog,
  exactly like a browse).

## 2. Create flow orchestration (WindowCommandManager)

```
CreateDiskFlow (drive):
  model  = FileBrowseModel(fs);  model.SetMountedPaths(store.MountedSourcePaths())
  folder = prefs.lastDiskCreateFolder  (empty -> Documents\Casso Disks, created on demand)
  dlg    = CreateDiskDialog(model, payloadAvailability)      // modal, themed
  on Create:
     verdict = model.ValidateTarget(name)                    // FR-007/FR-018 handled in-dialog
     spec    = dlg.Spec()                                    // format/contents/bootable
     payload = LoadBootPayloadIfNeeded(spec)                 // reads cached master .dsk files
     BlankDiskBuilder::Build(spec, payload, bytes)           // core, all-or-nothing
     write file (temp + rename for atomicity; FR-011)
     prefs.lastDiskCreateFolder = folder;  SaveGlobalPrefs()
     m_shell.Mount(6, drive, path)                           // existing path: MRU, persistence,
                                                             // door close, drive widget (FR-008)
  occupied drive -> existing replace-confirm before Mount (FR-009)
```

- **Bootable availability**: checked at dialog open AND re-checked on Create;
  the toggle is disabled with explanatory text when the payload file is
  absent. An inline "Download..." affordance invokes
  `AssetBootstrap::DownloadStockBootDisk` (promoted to public static; explicit
  click = consent, same as the picker's stock rows). Offline/failed download
  leaves the toggle disabled; data-only creation still works (FR-017).
- **DiskImageStore** gains `MountedSourcePaths()` (path + drive pairs) for
  FR-018.

## 3. CreateDiskDialog (Casso/Ui/Dialogs/, R-010)

`DxuiDialogWindow` subclass, `PickerDialog`/`PickerBodyPanel` idiom:

| Widget | Role |
|---|---|
| `DxuiListView` | Current folder: Name / Size / Modified; `..` + folders first; Enter/double-click folder navigates; selecting a file puts its name in the name field |
| `DxuiTextInput` | File name (max length per filesystem; placeholder = unique default) |
| `DxuiDropdown` ×2 | Format (WOZ/DSK/PO) — drives extension + contents constraints (FR-010); Contents (DOS 3.3 / ProDOS 1.1.1 / Unformatted) |
| `DxuiCheckbox` | Bootable (disabled + reason when payload unavailable) |
| Buttons | Create (default) / Cancel; up-button beside a current-path label |

Keyboard: full tab order (list → name → format → contents → bootable →
buttons); list keyboard nav via existing `SetKeyboardColumnNav`; Enter in the
name field = Create; Escape = Cancel. Resizable with sensible min size.

## 4. Write-protect toggle (R-011, FR-014..016)

- **Menu**: Disk menu gains per-drive checkable items `IDM_DISK_WP1/2`
  ("Write-Protect Disk 1/2"), enabled only when the drive is mounted; check
  state = `WriteProtectInfo.imageFlag || readOnlyFile` for that drive.
- **Routing**: `WindowCommandManager::OnDiskCommand` →
  `DiskManager::ToggleImageWriteProtect (drive)`:
  - WOZ: flush pending dirty content FIRST, then `SetImageWriteProtected
    (newState)` + mark dirty + flush again so INFO byte 2 persists — ordering
    matters because a protected image skips flush (`FlushEntry` early-out);
    toggling ON must not strand unwritten sectors, and the WP-flag flush
    itself must bypass the write gate (serializer-level write, not guest
    write).
  - DSK/PO: `IFileSystem` set/clear read-only attribute on the backing file.
  - Both: re-run `ProbeFileWritability` + `ApplyExternalWriteProtect` so
    `WriteProtectInfo` (and padlock/tooltip/menu check) reflect reality
    immediately (FR-015).
- **Failure** (attribute change denied, file missing): message dialog with the
  cause; state re-read from reality, never assumed (FR-016).
- **Unchanged**: the Settings>Disk per-drive *user* write-protect checkbox
  (`userSetting` bit, per-machine pref) — different semantic, still OR'd in.

## 5. Prefs (R-012)

`GlobalUserPrefs.lastDiskCreateFolder` (UTF-8; 4-edit recipe). Written on every
successful create; read at dialog open; empty/missing folder falls back to
`Documents\Casso Disks` (created on demand via
`SHGetKnownFolderPath(FOLDERID_Documents)`).
