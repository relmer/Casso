#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/DiskOperations.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTypes
//
//  The types CassoExplorerActions and its parts share. CassoExplorerActions derives from it, so
//  each is also CassoExplorerActions::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct ActionsTypes
{
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

        //  An item the user chose not to write, by canceling its prompt or
        //  skipping it at a name clash. A move does not take its source away.
        bool                      skipped = false;

        bool  Succeeded() const { return SUCCEEDED (hr); }
        bool  IsWhole  () const { return Succeeded() && !damaged && !skipped; }
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

    //  The open Apple drive's whole disk read into an image file, or written
    //  from one, over the link. Each is queued and reported when it is done.
    //  A read with sectors it could not read calls onUnreadable instead of
    //  reporting a failure, so the user can choose to read it again with
    //  those sectors written as zeros.
    using UnreadableFn = std::function<void (const LinkOperation & operation, const std::string & link, int drive)>;

    //  Whether a drive has the link's boot disk; `then` runs on the window's
    //  thread with the answer, or with the failure.
    using LinkDiskFn = std::function<void (bool linkDisk, HRESULT result)>;

    //  The same search over bytes read on demand, a chunk at a time, so a file
    //  of any size is searched without being held in memory. Chunks overlap by
    //  the pattern's length less one, so a match across two is not missed.
    using ReadFn = std::function<void (uint64_t offset, std::span<uint8_t> out)>;
};
