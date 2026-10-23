#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/AppleSingleCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerActions
//
//  The operations a context menu offers, over whatever the browser has
//  selected: get to a host folder, put host files into the image, delete,
//  boot, and rename.
//
//  EVERY WRITE GOES THROUGH DiskOperations, the same runner the command-line
//  tool drives, so a menu verb and the matching command produce the same
//  bytes. After a write the image's cached catalog is dropped, the list is
//  reloaded, and the write callback runs so a Casso with the image mounted
//  can be told to reload it.
//
//  Nothing here asks the user anything. Confirmation, a destination folder,
//  a new name: the window collects them and passes them in.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerActions
{
public:
    using Encoding = DiskOperations::Encoding;

    enum class Verb
    {
        Open,
        Get,
        Put,
        Delete,
        Rename,
        Boot,
        InsertDrive1,
        InsertDrive2,
        OpenInNewCasso,
        NewDisk,
        NewFolder,
        Format,
        ReadSectors,
        WriteSectors,
        ReadBlocks,
        WriteBlocks,
        Refresh,

        //  Windows' own verbs for a real file or folder: the programs that
        //  open it, and the shell's full context menu.
        OpenWith,
        MoreOptions,
        Cut,
        Copy,
        Paste,
        Share,

        //  The Recycle Bin's: its items back where they were, and all of
        //  them gone for good.
        Restore,
        EmptyRecycleBin,

        //  An archive's own Extract All, from the shell.
        ExtractAll,

        //  Every file on a disk image, copied into a folder beside it.
        ExtractFiles,
    };

    //  What a put does with the bytes. ByContent is the rule a left-drag
    //  follows: the name's type first, then the content. The others are the
    //  choices a right-drag's menu offers when the content rule would be a
    //  guess.
    enum class Conversion { ByContent, Text, Applesoft, Binary };

    //  Whether the files a drop holds already say what they are, and the
    //  one row a menu shows when they do: "BIN $2000" for a recorded type,
    //  "Applesoft BASIC" for a listing a name promises.
    struct DropKind
    {
        bool          determined = false;
        std::wstring  label;
    };

    struct Outcome
    {
        HRESULT                   hr      = S_OK;
        std::wstring              message;
        int                       written = 0;

        //  The entries a put or a copy made in the directory it went into,
        //  by catalog path, so an undo can take them out again.
        std::vector<std::string>  created;

        //  A copy that completed from a damaged source: what could not be read
        //  went over as zeros, or entries may be missing. The warning is for
        //  the user, and a move does not take such a source away.
        bool                      damaged = false;
        std::wstring              warning;

        bool  Succeeded() const { return SUCCEEDED (hr); }
        bool  IsWhole  () const { return Succeeded() && !damaged; }
    };

    //  How one host file goes into an image. A descriptive or CiderPress
    //  suffix holding a type writes the bytes as a payload of that type;
    //  anything else goes through the runner with the conversion the content
    //  calls for.
    struct PutPlan
    {
        bool           usePayload       = false;
        std::string    catalogName;
        Encoding       encoding         = Encoding::Verbatim;
        std::string    typeName;
        bool           hasLoadAddress   = false;
        Word           loadAddress      = 0;
        FilePayload    payload;
        bool           guessedAddress   = false;

        //  The file said what it is -- a type or a conversion in its name, or
        //  an AppleSingle container -- rather than the content rule deciding.
        bool           namedItself      = false;
        std::wstring   refusal;
    };

    using WrittenFn = std::function<void (const std::wstring & imagePath)>;

    //  Asked once per binary file whose name records no address, with the
    //  address the content suggests. False skips the file.
    using AddressFn = std::function<bool (const std::wstring & hostName, Word suggested, Word & outAddress)>;

    //  What a copy out does with a file of the same name in the folder, as
    //  in File Explorer's question: write over it, copy nothing, or keep
    //  both with the copy numbered.
    enum class Collision { Replace, Skip, KeepBoth };

    //  Asked once per file that is in the folder already, with its path and
    //  its place among the count of such files. With none, both are kept.
    using CollisionFn = std::function<Collision (const std::wstring & hostPath, int index, int count)>;

    CassoExplorerActions (CassoExplorerBrowser & browser, IFileSystem & fs);

    void  SetOnImageWritten (WrittenFn fn) { m_onWritten = std::move (fn); }

    //  The verbs the list's context menu offers for the current selection.
    std::vector<Verb>  GetListVerbs() const;

    Outcome  GetSelected    (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision = nullptr);

    //  Copy every file on the image into a new folder beside it, called after the
    //  image without its extension, and its directories as folders inside.
    //  `outFolder` is the folder made.
    Outcome  ExtractImage   (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder);

    //  The list's Apple drive: its link, drive and directory. False when the
    //  list is not on one.
    bool     TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const;

    //  Files from an image onto the list's Apple drive, as one image's files
    //  copy onto another: the bytes as stored, the type mapped between file
    //  systems. A folder is not copied.
    Outcome  CopyEntriesToAppleDrive (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths);

    //  A file read from an Apple, converted as a copy out of an image is,
    //  and wrapped for HostFileNaming::Style::AppleSingle.
    static HRESULT  ToHostBytes (FilePayload payload, Encoding encoding, bool appleSingle, const AppleSingleFile & single, std::vector<Byte> & outBytes);

    //  A host file as the payload a put plan makes of it.
    static HRESULT  MakePayload (const std::wstring & hostPath, const PutPlan & plan, const std::vector<Byte> & bytes, VolumeKind kind, FilePayload & outPayload);
    Outcome  PutFiles       (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress = {});

    //  Host files and folders into a directory of an image, by Put's rules. A
    //  folder becomes a ProDOS directory with its contents; DOS 3.3 has no
    //  directories, so a folder there is refused by name rather than
    //  flattened. A disk image goes in as a file.
    Outcome  PutInto        (const std::wstring & imagePath, VolumeKind kind, const std::string & directory,
                             const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress = {},
                             Conversion conversion = Conversion::ByContent);

    //  What the files of a drop are, for the menu a right-drag opens.
    DropKind  DescribeDrop  (const std::vector<std::wstring> & hostPaths, VolumeKind kind);

    //  Whether host files and folders fit on a new 140K disk of this kind,
    //  counted by the blocks or sectors each takes. Empty when they fit;
    //  otherwise why not, for the refusal shown before anything is created.
    std::wstring  CheckFitsNewDisk (VolumeKind kind, const std::vector<std::wstring> & hostPaths);

    //  Entries dragged out of one image into a directory of another, byte for
    //  byte with their type and addresses, the types mapped when the two file
    //  systems differ. A ProDOS directory goes with its contents; DOS 3.3
    //  receives no directories.
    Outcome  CopyEntriesInto (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths,
                              const std::wstring & targetImage, VolumeKind targetKind, const std::string & directory);

    //  The data and structure a file and a directory take, and a new disk's
    //  room for them, by file system. Public so the arithmetic is tested.
    static uint64_t  CountUnitsForFile   (VolumeKind kind, uint64_t sizeBytes);
    static uint64_t  CountUnitsForFolder (VolumeKind kind);
    static uint64_t  GetNewDiskFreeUnits (VolumeKind kind);

    //  A name nothing in `taken` has, from `base`: Explorer's "New folder",
    //  "New folder (2)" on the host, and within ProDOS's fifteen characters,
    //  "NEW.FOLDER", "NEW.FOLDER.2" in an image. Case does not tell names apart.
    static std::wstring  MakeUnusedName (const std::wstring & base, const std::vector<std::wstring> & taken, bool host);

    //  A name for an entry going into an image that no other entry in its
    //  directory has, as Explorer names a collision: "NAME (2)" on DOS 3.3,
    //  and "NAME.2" on ProDOS, whose names take no spaces or parentheses. A
    //  copy beside its original is "NAME - COPY" or "NAME.COPY" first.
    static std::string   MakeFreeEntryName (const std::string & desired, const std::vector<std::string> & taken, VolumeKind kind, bool copy);

    //  Whether the catalog path `directory` is below `folder`, in any case.
    static bool          IsInsideFolder (const std::string & directory, const std::string & folder);

    //  The unused name a new folder in the current location takes.
    std::wstring  GetNewFolderName () const;

    static constexpr const wchar_t *  kHostFolderBase          = L"New folder";
    static constexpr const wchar_t *  kProDosFolderBase        = L"NEW.FOLDER";
    static constexpr size_t           kProDosNameMax           = 15;
    static constexpr size_t           kDos33NameMax            = 30;
    static constexpr uint64_t         kProDosBlockBytes        = 512;
    static constexpr uint64_t         kProDosIndexEntries      = 256;
    static constexpr uint64_t         kProDosNewDiskFreeBlocks = 273;   // 280 less 2 boot, 4 directory, 1 bitmap
    static constexpr uint64_t         kDos33SectorBytes        = 256;
    static constexpr uint64_t         kDos33ListEntries        = 122;
    static constexpr uint64_t         kDos33NewDiskFreeSectors = 496;   // 560 less tracks 0-2 and the catalog track
    Outcome  DeleteSelected ();

    //  Entries by catalog path, for the second half of a move. A path that
    //  lists is a directory, and goes with everything under it.
    Outcome  DeleteEntries (const std::string & image, const std::vector<std::string> & catalogPaths);
    Outcome  BootSelected   ();
    Outcome  RenameSelected (const std::wstring & newName);

    //  A new directory in the ProDOS location the list shows. The name is the
    //  volume's to accept, so an illegal one comes back as a refusal.
    Outcome  CreateFolder (const std::wstring & name);

    //  What deleting the selection would remove, as lines a confirmation shows:
    //  one row per entry with the locked ones marked, then the totals. Empty
    //  when nothing selected is a directory, since a file needs no such list.
    std::vector<std::wstring>  DescribeDeletePlan() const;

    //  A new image in a host folder, refused when the file exists already.
    Outcome  CreateImage (const std::wstring & folder, const std::wstring & fileName, const DiskOperations::NewDiskRequest & request);

    //  Whether a file of that name is already in the folder, which CreateImage
    //  would refuse; asked while the name is being typed.
    bool     IsNameTaken (const std::wstring & folder, const std::wstring & fileName) const;

    //  Formats the selected image in a host folder, or the image the list
    //  shows. Everything on it is lost.
    Outcome  FormatImage (const DiskOperations::NewDiskRequest & request);

    //  The image a format would act on, or empty. The raw sector and block
    //  verbs act on the same image.
    std::wstring  GetFormatTarget() const;

    //  Raw access through the runner's sector and block verbs. Sectors are
    //  numbered as DOS 3.3 numbers them.
    Outcome  ReadSectors  (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical);
    Outcome  WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical);
    Outcome  ReadBlocks   (int block, int count, const std::wstring & hostPath);
    Outcome  WriteBlocks  (int block, const std::wstring & hostPath);

    //  The open Apple drive's whole disk read into an image file, or written
    //  from one, over the link. Each is queued and reported when it is done.
    //  A read with sectors it could not read calls onUnreadable instead of
    //  reporting a failure, so the user can choose to read it again with
    //  those sectors written as zeros.
    using UnreadableFn = std::function<void (const LinkOperation & operation)>;

    Outcome  ReadDiskToImage  (const std::wstring & imagePath, bool blankUnreadable, UnreadableFn onUnreadable);
    Outcome  WriteImageToDisk (const std::wstring & imagePath);

    //  The same for any drive of any link, as a drop onto a drive's node
    //  writes to a drive that may not be open.
    Outcome  WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath);

    //  A drive's whole disk read into an image in hostFolder, with its volume
    //  in the file name, as a drag of the drive's node onto a folder does. The name is
    //  made free of any file already there.
    Outcome  CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder);

    //  Whether a drive has the link's boot disk; `then` runs on the window's
    //  thread with the answer, or with the failure.
    using LinkDiskFn = std::function<void (bool linkDisk, HRESULT result)>;

    HRESULT  CheckLinkDisk (const std::string & link, int drive, LinkDiskFn then);

    //  The file name a drive's image is given: its ProDOS volume's name with
    //  .po, a DOS 3.3 volume as "DOS 3.3 volume 254.dsk", or the drive.
    static std::wstring  MakeDriveImageName (const VolumeListing & listing, VolumeKind kind, int slot, int drive);

    //  The link's boot disk written to the open Apple drive over the link,
    //  through an image in tempFolder: its boot sector loads the shim at the
    //  link's speed.
    Outcome  SaveLinkToDisk   (const std::wstring & tempFolder);

    //  The open Apple drive formatted as DOS 3.3 or ProDOS, bootable or not,
    //  by writing it a new disk built in tempFolder from the request.
    Outcome  FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);
    Outcome  FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder);

    //  The same boot disk as an image file on the PC, for an emulator or to
    //  write to a disk some other way. A Wi-Fi modem link's disk dials this
    //  PC before its first Ready.
    Outcome  SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings);

    //  The cassette bootstrap for a link as a WAV file on the PC, to send into
    //  a ][+ or //e's cassette input from another device, or to insert in
    //  Casso's own tape deck. outCommand is the monitor command that reads it.
    Outcome  SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand);

    //  The open Apple drive as the menus and confirmations name it: its slot,
    //  its drive and its volume, as "Slot 6, Drive 1 (DOS 3.3 volume 254)".
    std::wstring  DescribeAppleDrive () const;

    //  Stops the open Apple drive's link's running operation.
    void  CancelLinkOperation ();

    //  Up to three whole numbers separated by spaces or commas, as typed into
    //  the raw verbs' prompts. Fewer than `required` fails; a missing count
    //  is one.
    static bool  TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers);

    static Encoding  GetEncoding (const FileEntry & entry, VolumeKind kind);

    static PutPlan  PlanPut (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind);
    void            GetAppleSingle  (const std::string & image, const FileEntry & entry, const std::wstring & hostPath, Outcome & inOutOutcome);
    static std::wstring  DescribePlan (const PutPlan & plan, VolumeKind kind);
    static PutPlan  PlanForced      (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion);
    static PutPlan  PlanAppleSingle (const std::wstring & hostName, const AppleSingleFile & file, VolumeKind kind);
    static void     SetTypedPayload (PutPlan & plan, const std::vector<Byte> & bytes, Byte type, bool hasAux, Word aux, bool isDos);

    //  A host file's stem as a legal catalog name: DOS 3.3 keeps up to 30
    //  printable characters; ProDOS keeps up to 15 of letters, digits and
    //  periods, starting with a letter.
    static std::string  MakeCatalogName (const std::wstring & hostName, VolumeKind kind);

    //  The same rules over a name that has no extension to strip.
    static std::string  SanitizeCatalogName (const std::wstring & stem, VolumeKind kind);

    static std::wstring  GetLeafName (const std::wstring & path);

    //  An address as typed: $2000, 0x2000 or 2000 are hexadecimal, as the
    //  Apple II writes addresses; a leading # marks decimal.
    static bool  TryParseAddress (const std::wstring & text, Word & outAddress);

    //  The same forms over the whole 64-bit range, for an offset into a file.
    static bool  TryParseOffset (const std::wstring & text, uint64_t & outValue);

    //  A Go to as typed: an address in TryParseAddress's forms, or one of
    //  them after + or -, which moves that far from `caretAddress`. A range
    //  follows with a hyphen and its last address ($0803-$0810), or with a
    //  comma and a length ($0803,+10). Without a range the first and last
    //  addresses are the same.
    static bool  TryParseGoTo (const std::wstring & text, int64_t caretAddress, int64_t & outFirst, int64_t & outLast);

    //  A search as typed: an even run of hex digits, with spaces anywhere, is
    //  bytes; text in double quotes, or anything else, is characters, and
    //  `outIsText` is set.
    static bool  TryParseSearch (const std::wstring & text, std::vector<Byte> & outBytes, bool & outIsText);

    //  The first match at or after `start`, wrapping to the beginning, or
    //  kNotFound. A text search ignores the high bit and the case of letters.
    static constexpr size_t  kNotFound = SIZE_MAX;
    static size_t  FindBytes (const std::vector<Byte> & haystack, const std::vector<Byte> & pattern, bool isText, size_t start);

    //  The same search over bytes read on demand, a chunk at a time, so a file
    //  of any size is searched without being held in memory. Chunks overlap by
    //  the pattern's length less one, so a match across two is not missed.
    using ReadFn = std::function<void (uint64_t offset, std::span<uint8_t> out)>;

    static constexpr uint64_t  kNotFoundOffset   = UINT64_MAX;
    static constexpr size_t    kSearchChunkBytes = 1024 * 1024;
    static uint64_t  FindInSource (const ReadFn & read, uint64_t count, const std::vector<Byte> & pattern, bool isText, uint64_t start);

private:
    static void  AppendMessage (Outcome & inOutOutcome, HRESULT hr, const std::wstring & message);
    static void  AppendWarning (Outcome & inOutOutcome, const std::wstring & warning);

    bool  IsHostFolder    (const std::wstring & path);

    //  The verbs on an Apple drive. Each queues its operations on the link
    //  and returns at once; each result is reported when the link's
    //  completions run.
    Outcome  GetFromAppleDrive        (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision);

    //  Where a copy out of one file goes, by the collision choice when the
    //  folder has a file of its name; empty to skip it.
    std::wstring  ResolveCopyOutPath (const std::wstring & hostFolder, const std::wstring & hostName, const CollisionFn & collision,
                                      int & inOutIndex, int count);
    int           CountCollisions    (const std::wstring & hostFolder, const std::vector<std::wstring> & hostNames);
    Outcome  PutOnAppleDrive          (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress);
    Outcome  DeleteOnAppleDrive       ();
    Outcome  RenameOnAppleDrive       (const std::string & newName);
    Outcome  CreateFolderOnAppleDrive (const std::string & name);
    void     PutItemsOnAppleDrive     (const std::string & link, int drive, const std::string & directory, std::vector<std::string> taken,
                                       const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress, Outcome & inOutOutcome);
    void     SubmitWrite              (const std::string & link, int drive, CassoLinkConnections::Queuer queue, Outcome & inOutOutcome);
    void  CopyEntry       (const std::string & sourceImage, VolumeKind sourceKind, const std::string & catalogPath,
                           const std::string & targetImage, VolumeKind targetKind, const std::string & directory,
                           int depth, Outcome & inOutOutcome);

    static constexpr int  kMaxCopyDepth = 16;
    void  ExtractDirectory (const std::string & image, VolumeKind kind, const std::string & directory, const std::wstring & hostFolder,
                            HostFileNaming::Style style, int depth, Outcome & inOutOutcome);
    std::wstring  MakeFreeFolderPath (const std::wstring & parent, const std::wstring & name);
    std::wstring  MakeFreeFilePath   (const std::wstring & folder, const std::wstring & name);
    bool  TryGetHostEntry (const std::wstring & path, FileSystemEntry & outEntry);
    void  PutItems     (const std::string & image, VolumeKind kind, const std::string & directory,
                        const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress, Conversion conversion, Outcome & inOutOutcome);
    std::vector<std::string>  GetEntryNames (const std::string & image, const std::string & directory) const;
    void  PutFolder    (const std::string & image, VolumeKind kind, const std::string & directory,
                        const std::wstring & hostFolder, const AddressFn & askAddress, Conversion conversion, Outcome & inOutOutcome);

    void  FinishWrite (const std::wstring & imagePath);

    static bool  TryParseGoToTarget (const std::wstring & text, int64_t caretAddress, int64_t & outAddress);

    static void  Append (Outcome & inOutOutcome, const DiskOperations::Result & result);
    static void  Record (Outcome & inOutOutcome, const DiskOperations::Result & result, const std::string & catalogPath);
    static void  KeepCreatedIn (Outcome & inOutOutcome, const std::string & directory);

    CassoExplorerBrowser  & m_browser;
    IFileSystem           & m_fs;
    WrittenFn               m_onWritten;
};
