# Contract: FileBrowseModel (core)

`CassoEmuCore/Ui/FileBrowseModel.h`, the pure navigation/validation engine
behind the create dialog (R-010). All filesystem access through the injected
`IFileSystem`; fully unit-testable with a mock.

```cpp
struct FileBrowseEntry
{
    std::wstring  name;
    bool          isFolder     = false;
    uint64_t      sizeBytes    = 0;
    int64_t       modifiedUnix = 0;
};

enum class TargetVerdict
{
    Ok,                 // create proceeds
    Exists,             // file present -> dialog asks overwrite-confirm (FR-007)
    InvalidName,        // empty / illegal characters / reserved device name
    FolderNotWritable,  // probe failed (FR-011 reports, creates nothing)
    MountedInDrive,     // FR-018 -- refused outright; verdictDrive names it
};

class FileBrowseModel
{
public:
    void  Bind (IFileSystem * fs);

    HRESULT  SetFolder (const std::wstring & absolute);   // entry point + NavigateInto/Up target
    HRESULT  NavigateInto (size_t entryIndex);            // folders only
    HRESULT  NavigateUp ();                                // no-op at a root
    HRESULT  Refresh ();                                   // relist current folder

    //  Folders first, then files matching the active extension filter;
    //  case-insensitive name sort inside each group. A synthetic ".." entry
    //  is index 0 whenever a parent exists.
    const std::vector<FileBrowseEntry> &  Entries () const;
    const std::wstring &                  CurrentFolder () const;

    //  Extension follows the dialog's format choice (.woz/.dsk/.po).
    void  SetExtensionFilter (const std::wstring & ext);

    //  "Blank Disk.woz", "Blank Disk (2).woz", ... first name not present in
    //  the current folder (FR-006/FR-007).
    std::wstring  UniqueDefaultName (const std::wstring & baseName) const;

    //  The shell supplies currently-mounted backing paths (both drives) so
    //  validation can refuse them (FR-018 / R-013); comparison is
    //  case-insensitive on the full path.
    void  SetMountedPaths (std::vector<std::wstring> paths, std::vector<int> drives);

    TargetVerdict  ValidateTarget (const std::wstring & fileName, int & outDrive) const;

    //  Full path the dialog will create: CurrentFolder + fileName with the
    //  filter extension appended when missing.
    std::wstring  ComposeTargetPath (const std::wstring & fileName) const;
};
```

## Behavior notes

- Hidden/system entries are excluded from `Entries()`.
- `SetExtensionFilter` re-filters without re-hitting the filesystem
  (`Refresh` caches the unfiltered listing).
- `ValidateTarget` checks, in order: name validity → mounted-path refusal →
  existence → writability probe; first failure wins, so `MountedInDrive`
  outranks `Exists` (you never get an overwrite-confirm for a live mount).
- The model never writes anything; creation is the shell's edge.

## IFileSystem additions (R-011 shares this seam)

- Enumerate a folder's entries with size + modified time.
- Query/set/clear a file's read-only attribute (used by both `ValidateTarget`'s
  writability probe refinement and the DSK/PO write-protect toggle).
