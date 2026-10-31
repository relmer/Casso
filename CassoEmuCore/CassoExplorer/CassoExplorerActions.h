#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsAppleDrive.h"
#include "CassoExplorer/Actions/ActionsCopy.h"
#include "CassoExplorer/Actions/ActionsGoTo.h"

struct AppleSingleFile;
class CassoExplorerBrowser;





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

class CassoExplorerActions : public ActionsTypes
{
public:

    CassoExplorerActions (CassoExplorerBrowser & browser, IFileSystem & fs);

    void  SetOnImageWritten (WrittenFn fn) { m_onWritten = std::move (fn); }

    //  The verbs the list's context menu offers for the current selection.
    std::vector<Verb>  GetListVerbs() const;

    Outcome  GetSelected    (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision = nullptr);

    //  Files a copy put on the clipboard as file descriptors and contents,
    //  written into the host folder by Get's rules for a name in use, so a
    //  paste of an image's or a linked Apple's files asks Replace, Skip or
    //  Keep both as Get does. `outCreated` is each file written, for the
    //  paste's undo.
    Outcome  PasteVirtualFiles (IDataObject * data, const std::wstring & hostFolder, const CollisionFn & collision,
                                std::vector<std::wstring> & outCreated) { return m_copier.PasteVirtualFiles (data, hostFolder, collision, outCreated); }

    //  Copy every file on the image into a new folder beside it, called after the
    //  image without its extension, and its directories as folders inside.
    //  `outFolder` is the folder made.
    Outcome  ExtractImage   (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder) { return m_copier.ExtractImage (imagePath, style, outFolder); }

    //  The list's Apple drive: its link, drive and directory. False when the
    //  list is not on one.
    bool     TryGetAppleDrive (std::string & outLink, int & outDrive, std::string & outDirectory) const { return m_appleDrive.TryGetAppleDrive (outLink, outDrive, outDirectory); }

    //  Files from an image onto the list's Apple drive, as one image's files
    //  copy onto another: the bytes as stored, the type mapped between file
    //  systems. A folder is not copied.
    Outcome  CopyEntriesToAppleDrive (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths) { return m_appleDrive.CopyEntriesToAppleDrive (sourceImage, sourceKind, catalogPaths); }
    Outcome  PutFiles                (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress = {}) { return m_copier.PutFiles (hostPaths, askAddress); }

    //  Host files and folders into a directory of an image, by Put's rules. A
    //  folder becomes a ProDOS directory with its contents; DOS 3.3 has no
    //  directories, so a folder there is refused by name rather than
    //  flattened. A disk image goes in as a file.
    Outcome  PutInto        (const std::wstring & imagePath, VolumeKind kind, const std::string & directory,
                             const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress = {},
                             Conversion conversion = Conversion::ByContent) { return m_copier.PutInto (imagePath, kind, directory, hostPaths, askAddress, conversion); }

    //  What the files of a drop are, for the menu a right-drag opens.
    DropKind  DescribeDrop  (const std::vector<std::wstring> & hostPaths, VolumeKind kind);

    //  Whether host files and folders fit on a new 140K disk of this kind,
    //  counted by the blocks or sectors each takes. Empty when they fit;
    //  otherwise why not, for the refusal shown before anything is created.
    std::wstring  CheckFitsNewDisk (VolumeKind kind, const std::vector<std::wstring> & hostPaths) { return m_copier.CheckFitsNewDisk (kind, hostPaths); }

    //  Entries dragged out of one image into a directory of another, byte for
    //  byte with their type and addresses, the types mapped when the two file
    //  systems differ. A ProDOS directory goes with its contents; DOS 3.3
    //  receives no directories.
    Outcome  CopyEntriesInto (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths,
                              const std::wstring & targetImage, VolumeKind targetKind, const std::string & directory) { return m_copier.CopyEntriesInto (sourceImage, sourceKind, catalogPaths, targetImage, targetKind, directory); }

    //  A name nothing in `taken` has, from `base`: Explorer's "New folder",
    //  "New folder (2)" on the host, and within ProDOS's fifteen characters,
    //  "NEW.FOLDER", "NEW.FOLDER.2" in an image. Case does not tell names apart.
    static std::wstring  MakeUnusedName (const std::wstring & base, const std::vector<std::wstring> & taken, bool host);

    //  A name for an entry going into an image that no other entry in its
    //  directory has, as Explorer names a collision: "NAME (2)" on DOS 3.3,
    //  and "NAME.2" on ProDOS, whose names take no spaces or parentheses. A
    //  copy beside its original is "NAME - COPY" or "NAME.COPY" first.
    static std::string   MakeFreeEntryName (const std::string & desired, const std::vector<std::string> & taken, VolumeKind kind, bool copy);

    //  The unused name a new folder in the current location takes.
    std::wstring  GetNewFolderName () const;

    static constexpr const wchar_t *  kHostFolderBase          = L"New folder";
    static constexpr const wchar_t *  kProDosFolderBase        = L"NEW.FOLDER";
    static constexpr size_t           kProDosNameMax           = 15;
    static constexpr size_t           kDos33NameMax            = 30;
    Outcome  DeleteSelected ();

    //  Entries by catalog path, for the second half of a move. A path that
    //  lists is a directory, and goes with everything under it.
    Outcome  DeleteEntries  (const std::string & image, const std::vector<std::string> & catalogPaths);

    //  Entries on the open linked drive, for the second half of a move.
    Outcome  DeleteDriveEntries (const std::vector<FileEntry> & entries) { return m_appleDrive.DeleteOnAppleDrive (entries); }

    //  The selected entries of the drive on display, to delete later.
    bool     TryPlanDriveDeletion (ActionsAppleDrive::DriveDeletion & outPlan) const { return m_appleDrive.TryPlanDeletion ({}, outPlan); }
    Outcome  RunDriveDeletion     (const ActionsAppleDrive::DriveDeletion & plan)    { return m_appleDrive.RunDeletion (plan); }
    Outcome  BootSelected         ();
    Outcome  RenameSelected       (const std::wstring & newName);

    //  Locks or unlocks every selected entry.
    Outcome  SetLockedSelected (bool locked);

    //  The Apple file properties dialog's edits, on each of the entries; a
    //  new name only when there is one entry.
    Outcome  ApplyProperties (std::vector<FileEntry> entries, const AppleFileProperties::Edits & edits);

    //  The selected file converted, as BasicConversion converts it, into a
    //  new file beside it; outNewName is the new file's name.
    Outcome  ConvertSelected (std::string & outNewName);

    //  The access an entry has locked or unlocked: ProDOS's destroy, rename
    //  and write bits cleared or set, the rest as they are. On DOS 3.3 the
    //  write bit is the lock.
    static FileAttributes  MakeLockAttributes (const FileEntry & entry, bool locked);

    //  What the selected files of a disk offer besides copy, delete and
    //  rename.
    void  AddFileVerbs (const std::vector<FileEntry> & entries, std::vector<Verb> & inOutVerbs) const;
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
    Outcome  ReadSectors  (int track, int sector, int count, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical) { return m_appleDrive.ReadSectors (track, sector, count, hostPath, numbering); }
    Outcome  WriteSectors (int track, int sector, const std::wstring & hostPath, DiskOperations::Numbering numbering = DiskOperations::Numbering::Logical) { return m_appleDrive.WriteSectors (track, sector, hostPath, numbering); }
    Outcome  ReadBlocks   (int block, int count, const std::wstring & hostPath) { return m_appleDrive.ReadBlocks (block, count, hostPath); }
    Outcome  WriteBlocks  (int block, const std::wstring & hostPath) { return m_appleDrive.WriteBlocks (block, hostPath); }

    //  The open drive's disk, or the given link's drive, so a second read
    //  goes to the drive the first one did, wherever the window is by then.
    Outcome  ReadDiskToImage  (const std::wstring & imagePath, bool blankUnreadable, UnreadableFn onUnreadable) { return m_appleDrive.ReadDiskToImage (imagePath, blankUnreadable, onUnreadable); }
    Outcome  ReadDiskToImage  (const std::string & link, int drive, const std::wstring & imagePath, bool blankUnreadable, UnreadableFn onUnreadable) { return m_appleDrive.ReadDiskToImage (link, drive, imagePath, blankUnreadable, onUnreadable); }
    Outcome  StartFullSend    (const std::string & link, const std::wstring & folder, const std::wstring & extension) { return m_appleDrive.StartFullSend (link, folder, extension); }
    Outcome  WriteImageToDisk (const std::wstring & imagePath) { return m_appleDrive.WriteImageToDisk (imagePath); }

    //  The same for any drive of any link, as a drop onto a drive's node
    //  writes to a drive that may not be open.
    Outcome  WriteImageToAppleDrive (const std::string & link, int drive, const std::wstring & imagePath) { return m_appleDrive.WriteImageToAppleDrive (link, drive, imagePath); }

    //  A drive's whole disk read into an image in hostFolder, with its volume
    //  in the file name, as a drag of the drive's node onto a folder does. The name is
    //  made free of any file already there.
    Outcome  CreateImageFromAppleDrive (const std::string & link, int drive, const std::wstring & hostFolder) { return m_appleDrive.CreateImageFromAppleDrive (link, drive, hostFolder); }

    HRESULT  CheckLinkDisk (const std::string & link, int drive, LinkDiskFn then) { return m_appleDrive.CheckLinkDisk (link, drive, then); }

    //  The link's boot disk written to the open Apple drive over the link,
    //  through an image in tempFolder: its boot sector loads the shim at the
    //  link's speed.
    Outcome  SaveLinkToDisk   (const std::string & link, int drive, const std::wstring & tempFolder) { return m_appleDrive.SaveLinkToDisk (link, drive, tempFolder); }

    //  The open Apple drive formatted as DOS 3.3 or ProDOS, bootable or not,
    //  by writing it a new disk built in tempFolder from the request.
    Outcome  FormatAppleDrive (const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder) { return m_appleDrive.FormatAppleDrive (request, tempFolder); }
    Outcome  FormatAppleDrive (const std::string & link, int drive, const DiskOperations::NewDiskRequest & request, const std::wstring & tempFolder) { return m_appleDrive.FormatAppleDrive (link, drive, request, tempFolder); }

    //  The same boot disk as an image file on the PC, for an emulator or to
    //  write to a disk some other way. A Wi-Fi modem link's disk dials this
    //  PC before its first Ready.
    Outcome  SaveLinkDiskImage (const std::wstring & imagePath, const CassoLinkSettings & settings) { return m_appleDrive.SaveLinkDiskImage (imagePath, settings); }

    //  The cassette bootstrap for a link as a WAV file on the PC, to send into
    //  a ][+ or //e's cassette input from another device, or to insert in
    //  Casso's own tape deck. outCommand is the monitor command that reads it.
    Outcome  SaveCassetteBootstrap (const std::wstring & wavPath, const CassoLinkSettings & settings, std::string & outCommand) { return m_appleDrive.SaveCassetteBootstrap (wavPath, settings, outCommand); }

    //  The open Apple drive as the menus and confirmations name it: its slot,
    //  its drive and its volume, as "Slot 6, Drive 1 (DOS 3.3 volume 254)".
    std::wstring  DescribeAppleDrive () const { return m_appleDrive.DescribeAppleDrive(); }

    //  Stops the open Apple drive's link's running operation.
    void  CancelLinkOperation () { m_appleDrive.CancelLinkOperation(); }

    static Encoding  GetEncoding (const FileEntry & entry, VolumeKind kind);

    static PutPlan       PlanPut         (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind);
    void                 GetAppleSingle  (const std::string & image, const FileEntry & entry, const std::wstring & hostPath, Outcome & inOutOutcome) { m_appleDrive.GetAppleSingle (image, entry, hostPath, inOutOutcome); }
    static std::wstring  DescribePlan    (const PutPlan & plan, VolumeKind kind);
    static PutPlan       PlanForced      (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion);
    static PutPlan       PlanAppleSingle (const std::wstring & hostName, const AppleSingleFile & file, VolumeKind kind);
    static void          SetTypedPayload (PutPlan & plan, const std::vector<Byte> & bytes, Byte type, bool hasAux, Word aux, bool isDos);

    //  A host file's stem as a legal catalog name: DOS 3.3 keeps up to 30
    //  printable characters; ProDOS keeps up to 15 of letters, digits and
    //  periods, starting with a letter.
    static std::string  MakeCatalogName (const std::wstring & hostName, VolumeKind kind);

    //  The same rules over a name that has no extension to strip.
    static std::string  SanitizeCatalogName (const std::wstring & stem, VolumeKind kind);

    static std::wstring  GetLeafName (const std::wstring & path);

    //  The first match at or after `start`, wrapping to the beginning, or
    //  kNotFound. A text search ignores the high bit and the case of letters.
    static constexpr size_t  kNotFound = SIZE_MAX;

    static constexpr uint64_t  kNotFoundOffset   = UINT64_MAX;
    static constexpr size_t    kSearchChunkBytes = 1024 * 1024;

private:
    std::vector<std::string>  GetEntryNames (const std::string & image, const std::string & directory) const;

    void  FinishWrite (const std::wstring & imagePath);

    static void  Append (Outcome & inOutOutcome, const DiskOperations::Result & result);

    CassoExplorerBrowser  & m_browser;
    IFileSystem           & m_fs;
    WrittenFn               m_onWritten;

    //  The actions' parts, each with the state it uses.
    friend class       ActionsAppleDrive;
    friend class       ActionsCopy;
    friend class       ActionsGoTo;
    ActionsAppleDrive  m_appleDrive      { *this };
    ActionsCopy        m_copier          { *this };
    ActionsGoTo        m_goTo            { *this };
};

