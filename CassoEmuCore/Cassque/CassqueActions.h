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
        Format,
        Refresh,
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

    //  A new image in a host folder, refused when the file exists already.
    Outcome  CreateImage (const std::wstring & folder, const std::wstring & fileName, const DiskOperations::NewDiskRequest & request);

    //  Formats the selected image in a host folder, or the image the list
    //  shows. Everything on it is lost.
    Outcome  FormatImage (const DiskOperations::NewDiskRequest & request);

    //  The image a format would act on, or empty.
    std::wstring  GetFormatTarget() const;

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

private:
    void  FinishWrite (const std::wstring & imagePath);

    static void  Append (Outcome & inOutOutcome, const DiskOperations::Result & result);

    CassqueBrowser  & m_browser;
    IFileSystem     & m_fs;
    WrittenFn         m_onWritten;
};
