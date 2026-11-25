#pragma once

#include "CassoExplorer/Model/CopyOptionRows.h"
#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "CassoExplorer/Actions/ActionsAppleDrive.h"
#include "CassoExplorer/Actions/ActionsArchive.h"
#include "CassoExplorer/Actions/ActionsCopy.h"
#include "CassoExplorer/Actions/ActionsDriveImage.h"
#include "CassoExplorer/Actions/ActionsGoTo.h"
#include "CassoExplorer/Actions/ActionsLinkDisk.h"
#include "CassoExplorer/Actions/ActionsPartition.h"
#include "CassoExplorer/Actions/ActionsTape.h"
#include "CassoExplorer/Actions/IImageWriteTarget.h"
#include "Devices/Disk/IDiskFileIo.h"

struct AppleSingleFile;
class CassoExplorerBrowser;
enum class DiskImageFormat;





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

class CassoExplorerActions : public ActionsTypes, private IImageWriteTarget
{
public:

    CassoExplorerActions (CassoExplorerBrowser & browser, IFileSystem & fs);

    void  SetOnImageWritten (WrittenFn fn) { m_onWritten = std::move (fn); }
    void  SetTapeTiming     (const TapeRecordingTiming & timing) { m_tapes.SetTiming (timing); }

    //  The verbs the list's context menu offers for the current selection.
    std::vector<Verb>  GetListVerbs() const;

    Outcome  GetSelected    (const std::wstring & hostFolder, HostFileNaming::Style style, const CollisionFn & collision = nullptr, bool raw = false);

    //  Files a copy put on the clipboard as file descriptors and contents,
    //  written into the host folder by Get's rules for a name in use, so a
    //  paste of an image's or a linked Apple's files asks Replace, Skip or
    //  Keep both as Get does. `outCreated` is each file written, for the
    //  paste's undo.
    Outcome  PasteVirtualFiles (IDataObject * data, const std::wstring & hostFolder, const CollisionFn & collision,
                                std::vector<std::wstring> & outCreated) { return m_copier.PasteVirtualFiles (data, hostFolder, collision, outCreated); }
    void     WriteZipSidecars  (const std::vector<std::wstring> & sources, const std::vector<std::wstring> & copies) { m_copier.WriteZipSidecars (sources, copies); }

    //  Copy every file on the image into a new folder beside it, called after the
    //  image without its extension, and its directories as folders inside.
    //  `outFolder` is the folder made.
    Outcome  ExtractImage   (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder) { return m_copier.ExtractImage (imagePath, style, outFolder); }

    //  A ShrinkIt archive of files on the PC, a ShrinkIt disk of an image, and
    //  the test of an image or an archive.
    Outcome  MakeArchive            (const std::vector<std::wstring> & hostPaths, std::wstring & outPath) { return m_archiver.MakeArchive (hostPaths, outPath); }
    Outcome  MakeShrinkItDisk       (const std::wstring & imagePath, std::wstring & outPath)              { return m_archiver.MakeShrinkItDisk (imagePath, outPath); }
    Outcome  ConvertImage           (const std::wstring & imagePath, DiskImageFormat format, std::wstring & outPath) { return m_archiver.ConvertImage (imagePath, format, outPath); }
    Outcome  TestImage              (const std::wstring & imagePath, std::wstring & outReport)            { return m_archiver.Test (imagePath, outReport); }
    Outcome  MakeTapeRecording      (const std::wstring & target)                                         { return m_tapes.MakeRecording (target); }
    Outcome  MakeDiskRecording      (const std::wstring & imagePath, const std::wstring & target, std::string & outCommand, double & outSeconds) { return m_tapes.MakeDiskRecording (imagePath, target, outCommand, outSeconds); }
    Outcome  AddToA2retroNet        (const std::wstring & imagePath)                                      { return m_a2retroNet.Add (imagePath); }
    bool     CanAddToA2retroNet     (const std::wstring & imagePath) const                              { return m_a2retroNet.CanAdd (imagePath); }
    Outcome  CreateA2retroNetConfig (const std::wstring & root)                                       { return m_a2retroNet.CreateConfig (root); }
    Outcome  RepairA2retroNetConfig (const std::wstring & root)                                       { return m_a2retroNet.Repair (root); }
    bool     IsA2PicoCard           (const std::wstring & folder) const                              { return m_a2retroNet.IsCard (folder); }
    HRESULT  ReadA2PicoCard         (const std::wstring & root, ActionsA2retroNet::Card & outCard)   { return m_a2retroNet.ReadCard (root, outCard); }
    Outcome  SaveA2retroNetConfig   (const std::wstring & root, const A2retroNetConfig & config)    { return m_a2retroNet.Save (root, config); }
    HRESULT  ListA2PicoImages       (const std::wstring & root, std::vector<std::string> & outPaths) { return m_a2retroNet.ListImages (root, outPaths); }

    //  The A2Pico card actions, for A2retroNET drives.
    ActionsA2retroNet & GetA2retroNet () { return m_a2retroNet; }

    //  The CassoLink boot disks and the cassette bootstrap.
    ActionsLinkDisk   & GetLinkDisks  () { return m_linkDisks; }

    //  Whole disks on a linked Apple's drive, and raw sectors and blocks.
    ActionsDriveImage & GetDriveImages() { return m_driveImages; }

    //  The linked Apple drive the list is on: its files, its file system and its link.
    ActionsAppleDrive & GetAppleDrive () { return m_appleDrive; }

    //  One partition of a card image: the one selected or open, its copy as
    //  an image file beside the card image, and an image file written over it
    //  after the question DescribeReplace returns.
    bool     TryGetPartition          (std::wstring & outPartitionPath) const                                                          { return m_partitions.TryGetTarget (outPartitionPath); }

    //  The selected image in a gzip file or a ZIP that the disk inspector's
    //  block mode opens: a .gz in a folder, the image in a .gz whose contents
    //  are listed, or an image in a ZIP.
    bool     TryGetWrappedImage       (std::wstring & outPath) const;
    Outcome  CopyPartitionToImage     (const std::wstring & partitionPath, std::wstring & outPath, const std::wstring & target = {})   { return m_partitions.CopyToImage (partitionPath, outPath, target); }
    Outcome  ReplacePartition         (const std::wstring & partitionPath, const std::wstring & imagePath)                             { return m_partitions.Replace (partitionPath, imagePath); }
    Outcome  DescribePartitionReplace (const std::wstring & partitionPath, const std::wstring & imagePath, std::wstring & outQuestion) { return m_partitions.DescribeReplace (partitionPath, imagePath, outQuestion); }

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
    static constexpr size_t           kCpmBaseMax              = 8;
    Outcome  DeleteSelected ();

    //  Entries by catalog path, for the second half of a move. A path that
    //  lists is a directory, and goes with everything under it.
    Outcome  DeleteEntries  (const std::string & image, const std::vector<std::string> & catalogPaths);

    Outcome  BootSelected         ();
    Outcome  RenameSelected       (const std::wstring & newName);

    //  Locks or unlocks every selected entry.
    Outcome  SetLockedSelected (bool locked);

    //  The Apple file properties dialog's edits, on each of the entries; a
    //  new name only when there is one entry.
    Outcome  ApplyProperties (std::vector<FileEntry> entries, const AppleFileProperties::Edits & edits);

    //  Each selected file converted, as BasicConversion converts it, into a
    //  new file beside it. A file with no conversion is passed over. Those
    //  passed over and those that fail are listed in the outcome's warning
    //  when any file was converted, and in its message when none was.
    //  outNewName is the new file's name when one file was converted.
    Outcome  ConvertSelected (std::string & outNewName);

    //  The access an entry has locked or unlocked: ProDOS's destroy, rename
    //  and write bits cleared or set, the rest as they are. On DOS 3.3 the
    //  write bit is the lock.
    static FileAttributes  MakeLockAttributes (const FileEntry & entry, bool locked);

    //  What the selected files of a disk offer besides copy, delete and
    //  rename.
    void  AddFileVerbs (const std::vector<FileEntry> & entries, std::vector<Verb> & inOutVerbs) const;

    //  Whether every selected image is one the disk inspector opens.
    bool  CanInspectSelected () const;

    //  Whether the disk inspector opens what the list is on: the selected
    //  disk images in a folder, or the image whose contents are listed. And
    //  whether the selection is two images the inspector compares.
    bool  CanInspect () const;
    bool  CanCompare () const;

    //  The image picked by Select for compare, for a later Compare to.
    void                  SetComparePick    (const std::wstring & imagePath) { m_comparePick = imagePath; }
    void                  ClearComparePick  ()                               { m_comparePick.clear(); }
    const std::wstring  & GetComparePick    () const                         { return m_comparePick; }
    bool                  CanCompareToPick  (const std::wstring & imagePath) const;

    //  Separators with nothing on one side of them, or beside another one,
    //  taken out of a verb list.
    static void  RemoveExtraSeparators (std::vector<Verb> & inOutVerbs);

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

    //  Combines the free space of the Apple Pascal disk the format verb acts on.
    Outcome  KrunchImage    ();
    bool     IsTargetPascal () const;
    bool     IsImagePascal  (const std::wstring & image) const;

    //  The image a format would act on, or empty. The raw sector and block
    //  verbs act on the same image.
    std::wstring  GetFormatTarget() const override;

    //  The blocks the format target has, for a raw block read or write.
    int           GetTargetBlockCount (const std::wstring & target = {}) const;

    //  An image that detection doesn't recognize, whose file system is chosen.
    bool  CanChooseFilesystem   (const std::wstring & image) const;
    void  ChooseImageFilesystem (const std::wstring & image, VolumeKind kind);


    static Encoding  GetEncoding (const FileEntry & entry, VolumeKind kind);

    //  The rows Copy to folder with options has for the selected files.
    void  FindCopyOptionRows (std::vector<CopyOptionRows::Row> & outRows);

    static PutPlan       PlanPut         (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind);
    static std::wstring  DescribePlan    (const PutPlan & plan, VolumeKind kind);
    static PutPlan       PlanForced      (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion);
    static PutPlan       PlanAppleSingle (const std::wstring & hostName, const AppleSingleFile & file, VolumeKind kind);
    static void          SetTypedPayload (PutPlan & plan, const std::vector<Byte> & bytes, Byte type, bool hasAux, Word aux, bool isDos);

    static PutPlan       PlanPutByContent     (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind);
    static PutPlan       PlanForcedConversion (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind, Conversion conversion);
    static PutPlan       AdaptPlanToVolume    (PutPlan plan, VolumeKind kind);
    static void          PlanImport           (PutPlan & inOutPlan, FileFormat format, const std::vector<Byte> & bytes, VolumeKind kind);
    static std::string   SanitizePascalName   (const std::wstring & stem);
    static std::string   SanitizeCpmName      (const std::wstring & stem);

    void                 AddFilesystemVerbs   (std::vector<Verb> & inOutVerbs) const;

    //  A host file's stem as a legal catalog name: DOS 3.3 keeps up to 30
    //  printable characters; ProDOS keeps up to 15 of letters, digits and
    //  periods, starting with a letter.
    static std::string  MakeCatalogName (const std::wstring & hostName, VolumeKind kind);

    //  The same rules over a name that has no extension to strip.
    static std::string  SanitizeCatalogName (const std::wstring & stem, VolumeKind kind);

    //  An entry's name for a copy to another file system.
    static std::string  FitEntryName (const std::string & name, VolumeKind source, VolumeKind target);

    //  The file system's name, for a message.
    static std::wstring  GetVolumeSystemName (VolumeKind kind);

    static std::wstring  GetLeafName (const std::wstring & path);

    //  The first match at or after `start`, wrapping to the beginning, or
    //  kNotFound. A text search ignores the high bit and the case of letters.
    static constexpr size_t  kNotFound = SIZE_MAX;

    static constexpr uint64_t  kNotFoundOffset   = UINT64_MAX;
    static constexpr size_t    kSearchChunkBytes = 1024 * 1024;

    static void  Append (Outcome & inOutOutcome, const DiskOperations::Result & result);

private:
    //  What reading an image found, reused until its size or time changes.
    //  The toolbar's commands are checked on every paint, and reading a WOZ
    //  image's tracks takes far longer than a frame.
    struct ImageFacts
    {
        FileStamp  stamp;
        bool       checkedFormat = false;
        bool       unrecognized  = false;
        bool       checkedKind   = false;
        bool       isPascal      = false;
    };

    static constexpr size_t  kMaxImageFacts = 256;

    void                      AddImageVerbs (bool onDisk, std::vector<Verb> & inOutVerbs) const;
    std::vector<std::string>  GetEntryNames (const std::string & image, const std::string & directory) const;
    Outcome                   ConvertOne    (const FileEntry & entry, BasicConversion::Kind kind, const std::vector<std::string> & made, std::string & outNewName);
    ImageFacts              & GetImageFacts (const std::wstring & image) const;

    void  FinishWrite (const std::wstring & imagePath) override;

    CassoExplorerBrowser                        & m_browser;
    IFileSystem                                 & m_fs;
    WrittenFn                                     m_onWritten;
    mutable std::map<std::wstring, ImageFacts>    m_imageFacts;
    std::wstring                                  m_comparePick;

    //  The actions' parts, each with the state it uses.
    friend class       ActionsA2retroNet;
    friend class       ActionsAppleDrive;
    friend class       ActionsArchive;
    friend class       ActionsCopy;
    friend class       ActionsGoTo;
    friend class       ActionsPartition;
    friend class       ActionsTape;
    ActionsAppleDrive  m_appleDrive      { *this };
    ActionsDriveImage  m_driveImages     { m_browser, *this, m_appleDrive };
    ActionsCopy        m_copier          { *this };
    ActionsArchive     m_archiver        { *this };
    ActionsGoTo        m_goTo            { *this };
    ActionsPartition   m_partitions      { *this };
    ActionsTape        m_tapes           { *this };
    ActionsA2retroNet  m_a2retroNet      { *this };
    ActionsLinkDisk    m_linkDisks       { m_browser };
};

