# Contract: picker mode in the Casso Explorer window

What a `CassoExplorerWindow` does when its context has a `PickerRequest`.

## Hosting

`ExplorerPickerHost` creates the window in the emulator's process.

| Service in `CassoExplorerWindowContext` | Picker value |
|---|---|
| `fs`, `baseDir`, `watcher` | supplied |
| `pictures`, `givenNames` | supplied when the emulator has them, else null |
| `host` | the `ExplorerPickerHost` |
| `links`, `running`, `announced`, `firewall`, `cards` | null, always |

`ExplorerPickerHost` as an `ICassoExplorerHost`:

| Method | In the picker |
|---|---|
| `OpenWindow`, `RunLaunch`, `ExitAll`, `CheckForUpdates` | does nothing; returns null where a window is returned |
| `SaveSession` | does nothing |
| `CloseWindow` | saves the picker's view to `PickerPrefs`, then releases the window |
| `GetWindowFor`, `ForEachWindow` | the open pickers only |
| `InspectDiskImages`, `CompareDiskImages`, `OfferBootRoms` | does nothing |

Guarantees (FR-031):
- No `CassoLinkConnections`, serial port or socket is created.
- `CassoExplorerSession` is neither loaded nor saved, and `CassoExplorerPrefs.json` is neither read nor written.
- The window class is not `CassoExplorerShell::kWindowClass` or `kTestCopyClass`, so no Casso Explorer launch forwards to a picker.

## Commands

Allowed in picker mode: back, forward, up, go to a path, refresh, change view, sort, choose columns, size columns, search in the listing, show or hide the preview pane, select, confirm, cancel, create new disk or blank tape.

Every other command id is disabled and hidden, including by shortcut and context menu: copy, cut, paste, move, delete, rename, new folder, new disk from the file manager's menu, properties, undo, format, inspector, compare, transfers, CassoLink, serial, cards, options, updates, new window, new tab, tab tear-off, and drag to any target other than the emulator's drives.

`PickerCommandGateTests` sweeps every id in `CassoExplorerCommands` and asserts each is on the allowlist or disabled in picker mode. A command added later is disabled in the picker until it is added to the allowlist.

## Listing

- Folders are always listed. Files are listed when `PickerScope::Passes` is true.
- Opening a file row confirms it. Opening a folder row opens the folder. A disk image or an archive is a file here: the picker does not open it as a folder.
- The Casso node lists images (FR-043). With a receiving drive and at least one stock disk that passes the rule, the tree has a "Stock disks" node under the Casso node, and its listing is those stock disks.
- The status bar reports how many files are listed and how many the scope left out, so an empty list is not mistaken for an empty folder.

## Picker bar

| Element | Shown when |
|---|---|
| Header: drive label and `DriveImageRule::GetAcceptedText` | the request has a receiving drive |
| Filter choice | the request has more than one filter entry |
| "Create new disk..." / "New blank tape" | `onCreateNew` is set |
| Insert (or Open for a request without a drive) | always; enabled when the selection is one file row |
| Cancel | always |

Keys: Enter confirms a file or opens a folder; Esc cancels; the list's existing navigation and type-to-select keys apply.

## First-use view

A request with a receiving drive always opens on the Casso node. With no saved view it is in details view with the columns Name, Form factor, Formatted size, File system, Volume name, Last used and Folder, sorted by Last used, newest first. A request without a drive opens on its starting folder in details view with Name, Date modified, Type and Size, sorted by name.
