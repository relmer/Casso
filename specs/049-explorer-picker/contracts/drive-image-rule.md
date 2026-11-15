# Contract: DriveImageRule

`CassoEmuCore/Devices/Disk/DriveImageRule.h`.

```cpp
enum class ReceivingDriveKind { Floppy525, Apple35, UniDisk35, HardDisk, Recorder };

struct ReceivingDrive
{
    ReceivingDriveKind  kind      = ReceivingDriveKind::Floppy525;
    uint32_t            maxBlocks = 0;      // HardDisk only
};

class DriveImageRule
{
public:
    static bool          Accepts         (const ReceivingDrive & drive, const ImageClass & imageClass);
    static std::wstring  GetAcceptedText (const ReceivingDrive & drive);     // "400K and 800K images"
};
```

`DeskDrivePlan::GetReceivingKind (DeskDriveModel model)` in `Ui/Scene/DeskDrivePlan.h` maps the desk's models. Its switch has no `default:` arm.

`EmulatorShell::TryGetReceivingDrive (int dropTag, ReceivingDrive & outDrive, DriveAddress & outAddress)` resolves a drop tag (Disk II drives 0 and 1, the tape tag, desk extras from `kExtraDropTagBase`, drive bar entries from `kBarDropTagBase`) to the drive under it, and is false for a tag with no drive.

## The rule

| Kind | Accepts when |
|---|---|
| any | `imageClass.status == Classified`, else false |
| `Floppy525` | `formFactor == Floppy525` |
| `Apple35` | `formFactor == Floppy35` |
| `UniDisk35` | `formFactor == Floppy35` and `isBlockReadable` |
| `HardDisk` | `formFactor == HardDisk` and `BlockDeviceCatalog::kMinHardDiskBlocks <= blockCount <= maxBlocks` |
| `Recorder` | `formFactor == Tape` |

## Guarantees

- The picker's row filter, the drop cursor, `OnFileDropped` and the tests are the only callers, and each calls `Accepts`. No other code compares form factors or block counts before a mount.
- For every device class in the emulator, a mount of an image succeeds for form factor and size exactly when `Accepts` is true (FR-015). `DriveRuleMountAgreementTests` asserts it.
- Every `DeskDriveModel` value has a mapping; a sweep of the enum asserts it (FR-014).
- CFFA and A2Pico block devices are not `ReceivingDrive` kinds. The rule is not applied to them.

## Drops

- `OverFn` returns `DROPEFFECT_COPY` when the drag has exactly one file, `TryGetReceivingDrive` resolves the tag, and `Accepts` is true; otherwise `DROPEFFECT_NONE`.
- `OnFileDropped` makes the same three checks and returns without a mount, an eject or a dialog when any fails.
