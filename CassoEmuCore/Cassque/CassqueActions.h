#pragma once

#include "Pch.h"

#include "Cassque/CassqueBrowser.h"
#include "Cassque/Model/DiskOperations.h"
#include "Cassque/Model/HostFileNaming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueActions
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

class CassqueActions
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
    };

    struct Outcome
    {
        HRESULT       hr      = S_OK;
        std::wstring  message;
        int           written = 0;

        bool  Succeeded() const { return SUCCEEDED (hr); }
    };

    //  How one host file goes into an image. A descriptive or CiderPress
    //  suffix carrying a type writes the bytes as a payload of that type;
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
        std::wstring   refusal;
    };

    using WrittenFn = std::function<void (const std::wstring & imagePath)>;

    //  Asked once per binary file whose name records no address, with the
    //  address the content suggests. False skips the file.
    using AddressFn = std::function<bool (const std::wstring & hostName, Word suggested, Word & outAddress)>;

    CassqueActions (CassqueBrowser & browser, IFileSystem & fs);

    void  SetOnImageWritten (WrittenFn fn) { m_onWritten = std::move (fn); }

    //  The verbs the list's context menu offers for the current selection.
    std::vector<Verb>  GetListVerbs() const;

    Outcome  GetSelected    (const std::wstring & hostFolder, HostFileNaming::Style style);
    Outcome  PutFiles       (const std::vector<std::wstring> & hostPaths, const AddressFn & askAddress = {});
    Outcome  DeleteSelected ();
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
    Outcome  ReadSectors  (int track, int sector, int count, const std::wstring & hostPath);
    Outcome  WriteSectors (int track, int sector, const std::wstring & hostPath);
    Outcome  ReadBlocks   (int block, int count, const std::wstring & hostPath);
    Outcome  WriteBlocks  (int block, const std::wstring & hostPath);

    //  Up to three whole numbers separated by spaces or commas, as typed into
    //  the raw verbs' prompts. Fewer than `required` fails; a missing count
    //  is one.
    static bool  TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers);

    static Encoding  GetEncoding (const FileEntry & entry, VolumeKind kind);

    static PutPlan  PlanPut (const std::wstring & hostName, const std::vector<Byte> & bytes, VolumeKind kind);

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
    void  FinishWrite (const std::wstring & imagePath);

    static bool  TryParseGoToTarget (const std::wstring & text, int64_t caretAddress, int64_t & outAddress);

    static void  Append (Outcome & inOutOutcome, const DiskOperations::Result & result);

    CassqueBrowser  & m_browser;
    IFileSystem     & m_fs;
    WrittenFn         m_onWritten;
};
