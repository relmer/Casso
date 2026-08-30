#include "Pch.h"

#include "DiskCommandRunner.h"
#include "AppleTextCodec.h"
#include "CommandLineParser.h"
#include "VolumeImage.h"
#include "Dos33Volume.h"
#include "ProDosVolume.h"
#include "DiskImageStore.h"
#include "BlankDiskBuilder.h"
#include "StockBootDisks.h"
#include "DirectBootBuilder.h"
#include "NibbleImageCodec.h"
#include "NibblizationLayer.h"
#include "ProDosSkeleton.h"
#include "WozLoader.h"
#include "Core/TextEncoding.h"
#include "Utils.h"




//
//  Every container this tool can WRITE, and the word that names it.
//
//  Separate from the reader in DiskImageStore, which recognizes what a file
//  already is. This is the shorter list of what a new one can be made as.
//
static constexpr DiskCommandRunner::ContainerName  s_kContainers[] =
{
    { "dsk", DiskFormat::Dsk, 0 },
    { "do",  DiskFormat::Do,  0 },
    { "po",  DiskFormat::Po,  0 },
    { "woz", DiskFormat::Woz, 0 },
    { "nib", DiskFormat::Nib, NibbleImageCodec::kNibTrackSize },
    { "nb2", DiskFormat::Nib, NibbleImageCodec::kNb2TrackSize },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DiskCommandRunner
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandRunner::DiskCommandRunner (IDiskFileIo & fileIo)
    : m_fileIo  (fileIo)
    , m_session (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::SetBanner
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::SetBanner (const std::string & banner)
{
    m_banner = banner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyPrefixes
//
//  Puts the reader's own prefixes into a line of help.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::WithPrefix (const std::string & text) const
{
    return DiskHelpPage::ApplyPrefixes (text, m_flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FindMissingParameters
//
//  Every required operand this command did not get, in grammar order.
//
//  THE LIST MATCHES THE GRAMMAR LINE the block prints, which is what makes the
//  refusal readable: a reader told that <image> and <name> are missing can see
//  both of them in the usage directly above, in that order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DiskCommandRunner::FindMissingParameters (const CommandLineOptions & options) const
{
    using Command = CommandLineOptions::DiskOptions::Command;

    std::vector<std::string>  missing;
    Command                   command   = options.disk.command;
    bool                      wantsFile = command == Command::Put || command == Command::SectorWrite
                                       || command == Command::BlockWrite;
    bool                      wantsName = command == Command::Get || command == Command::Delete
                                       || command == Command::Boot;



    //  Help asks for nothing, and a command the table does not know is
    //  answered as an unknown command rather than as a missing operand.
    HRESULT  hr    = S_OK;   // vestigial, for the bail
    bool     asked = command != Command::None && command != Command::Help;



    BAIL_OUT_IF (!asked, S_OK);

    if (options.disk.imagePath.empty())
    {
        missing.push_back ("<image>");
    }

    if (wantsName && options.disk.path.empty())
    {
        missing.push_back ("<name>");
    }

    if (wantsFile && options.disk.hostFile.empty())
    {
        missing.push_back ("<file>");
    }

Error:
    return missing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReportMissingParameters
//
//  That command's usage, and then every operand it did not get.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReportMissingParameters (const std::vector<std::string> & parameters,
                                                 DiskCommandResult & result) const
{
    std::string  list;
    size_t       i = 0;



    for (i = 0; i < parameters.size(); i++)
    {
        bool  last = (i + 1) == parameters.size();

        list += parameters[i];

        if (!last)
        {
            list += (parameters.size() == 2) ? " and " : (i + 2 == parameters.size() ? ", and " : ", ");
        }
    }

    result.output        += DiskHelpPage::BuildCommandHelp (m_command, m_flagPrefix);
    result.diagnostics   += std::string ("Error: required parameter")
                          + ((parameters.size() > 1) ? "s " : " ") + list + " missing\n";
    result.exitStatus     = DiskCommandResult::kNoOutput;
    result.badCommandLine = true;
    result.usageShown     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReportMissingParameter
//
//  A required operand that was not supplied.
//
//  THAT COMMAND'S USAGE, THEN THE PARAMETER, in the shape a Windows
//  command-line tool answers with. What went before was a bare sentence with
//  no usage at all for some commands and the WHOLE page for others, and the
//  bare sentence was written in a voice the tool uses nowhere else.
//
//  The usage comes first and the error last, for the reason every other
//  refusal here puts it last: a reader sees the bottom of the screen.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReportMissingParameter (const std::string & parameter,
                                                DiskCommandResult & result) const
{
    ReportMissingParameters (std::vector<std::string> { parameter }, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Dos33TypeLetter
//
//  The letters DOS itself prints. The lock bit is already masked off by the
//  reader, so only the type remains.
//
////////////////////////////////////////////////////////////////////////////////

char DiskCommandRunner::Dos33TypeLetter (Byte type)
{
    switch (type)
    {
        case 0x00: return 'T';
        case 0x01: return 'I';
        case 0x02: return 'A';
        case 0x04: return 'B';
        case 0x08: return 'S';
        case 0x10: return 'R';
        case 0x20: return 'A';
        case 0x40: return 'B';
        default:   return '?';
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatDos33Entry
//
//  The shape a booted machine prints: lock flag, type letter, sector count in
//  three digits, then the name.
//
//  ENTRIES OCCUPYING NO SECTORS ARE RENDERED, NOT FILTERED, and that is a
//  decision rather than an oversight. Real disks use zero-sector catalog
//  entries to draw section headings -- Merlin Pro's own disk has twenty of
//  them among sixty-three, reading "MERLIN PRO - DOS 3.3" and a rule of
//  underscores beneath it. DOS renders them, the vendor's own printed catalog
//  shows them, and their author put them there to be seen.
//
//  Hiding them would make this listing disagree with the machine's, and the
//  disagreement would look like an enumeration bug rather than a display
//  choice. Anyone who wants them gone is asking for a filter, which is a
//  different request from enumerating differently.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatDos33Entry (const FileEntry & entry)
{
    char  sectors[8] = {};



    snprintf (sectors, sizeof (sectors), "%03u", (unsigned) (entry.sizeUnits & 0x3FF));

    return std::string (entry.isLocked ? "*" : " ")
         + std::string (1, Dos33TypeLetter (entry.type))
         + " " + sectors + " " + entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatProDosEntry
//
//  EVERY COLUMN, ALWAYS. `eof=` and `aux=` were behind a `--long` flag, and the
//  flag was paying for nothing: ProDosVolume::Enumerate fills both fields
//  unconditionally, so withholding them cost a read of the help to discover and
//  a second run of the command to get. The widest row this produces still fits
//  inside 80 columns.
//
//  They are the two fields a build loop actually needs, which is what made the
//  flag worst: the exact length of a file and the address a binary loads at are
//  what you check after placing one, and both were the ones hidden.
//
//  A DOS 3.3 listing is unaffected -- it has its own formatter, and the
//  filesystem records neither field.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatProDosEntry (const FileEntry & entry)
{
    char         detail[96] = {};
    std::string  text;



    text = std::string (entry.isLocked ? "*" : " ") + entry.name;

    while (text.size() < 18)
    {
        text += " ";
    }

    snprintf (detail, sizeof (detail), "%s $%02X %5u  eof=%u aux=$%04X",
              entry.isDirectory ? "DIR" : "   ",
              (unsigned) entry.type,
              (unsigned) entry.sizeUnits,
              (unsigned) entry.eofBytes,
              (unsigned) entry.auxType);

    text += detail;

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeListingRefusal
//
//  WHICH LINE, AND THE LINE ITSELF. A number on its own is useless against a
//  file numbered by tens, and useless in the other direction too: a program
//  read off a disk has line numbers the file on the host does not have. So the number
//  is given when there is one, the file's own position when there is not, and
//  the offending text is quoted underneath whenever it is known.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeListingRefusal (
    const char                   *  leadIn,
    const ApplesoftListingError  &  error)
{
    char         note[96] = {};
    std::string  message  = leadIn;



    message += ": ";

    if (error.hasLineNumber)
    {
        snprintf (note, sizeof (note), "line %u ", (unsigned) error.lineNumber);
        message += note;
    }
    else if (error.sourceLineIndex > 0)
    {
        snprintf (note, sizeof (note), "the line at file position %u ",
                  (unsigned) error.sourceLineIndex);
        message += note;
    }

    message += error.reason;
    message += "\n";

    if (!error.sourceLine.empty())
    {
        message += "    ";
        message += error.sourceLine;
        message += "\n";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeVolumeRefusal
//
//  A verdict from the filesystem layer, in words.
//
//  That layer answers in Win32 codes on purpose -- they carry the meanings a
//  refusal needs without asserting the way E_INVALIDARG does, since everything
//  it refuses came from a user rather than from a caller's bug. What they are
//  not is an explanation. Printing the code would tell someone whose file is
//  locked that the tool returned a number, and the one thing they could have
//  done about it would be missing from the message.
//
//  Anything unrecognized still gets a sentence rather than a code, because a
//  case added below this layer must not be able to leak one upward.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeVolumeRefusal (HRESULT hr)
{
    //  A lookup rather than a ladder: eleven ifs were a table wearing
    //  control flow, and every row added was another early return.
    static constexpr struct { HRESULT code; const char * sentence; }  kSentences[] =
    {
        { HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
          "is locked on this volume. Unlock it on the disk before writing over it" },
        { HRESULT_FROM_WIN32 (ERROR_DISK_FULL),
          "does not fit. The volume has no room for the file's contents or "
          "for another catalog entry" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
          "is not a valid name for this filesystem. It must be a single "
          "component starting with a letter, short enough for the catalog, "
          "and free of commas and control characters" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
          "is not on this volume" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE),
          "is larger than this filesystem can record for one file" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
          "is a binary and requires a load address. Use %Lload $XXXX" },
        { HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED),
          "is a directory. Removing it would strand the files beneath it" },
        { HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF),
          "has a sector chain that cannot be followed to its end" },
        { HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
          "cannot be the volume's startup program. The image has no operating "
          "system on the boot tracks" },
        { HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE),
          "is not a program this volume's boot path launches. On ProDOS that "
          "requires a file of type SYS, not the kernel itself" },
    };
    const char *  sentence = "was refused by the filesystem on this volume";



    for (const auto & row : kSentences)
    {
        if (row.code == hr)
        {
            sentence = row.sentence;
            break;
        }
    }

    return sentence;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunList
//
//  Damage does not fail the listing. The developer this serves is recovering
//  old disks, and the entries that DID read are the ones they need; a refusal
//  would withhold exactly the information the situation calls for. It earns the
//  complaints status instead, so a script can tell a partial read from a clean
//  one without parsing anything.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunList (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr           = S_OK;
    DiskImageSession::OpenedImage  opened;
    VolumeListing                  listing;
    char                           summary[128] = {};



    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);



        hr = volume.Enumerate (listing);

        CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

        if (listing.hasVolumeName)
        {
            result.output += "/" + listing.volumeName + "\n\n";
        }
        else if (listing.hasVolumeNumber)
        {
            snprintf (summary, sizeof (summary), "DISK VOLUME %u\n\n",
                      (unsigned) listing.volumeNumber);
            result.output += summary;
        }

        for (const FileEntry & entry : listing.entries)
        {
            result.output += (opened.kind == VolumeKind::Dos33)
                           ? FormatDos33Entry (entry)
                           : FormatProDosEntry (entry);

            result.output += "\n";
        }

        snprintf (summary, sizeof (summary), "\n%u %s free of %u\n",
                  (unsigned) listing.freeUnits,
                  (opened.kind == VolumeKind::Dos33) ? "sectors" : "blocks",
                  (unsigned) listing.totalUnits);

        result.output += summary;
    }

    // Damage from the catalog walk, and from the track layer beneath it.
    //
    // Each message must say the LISTING IS INCOMPLETE, not merely that the disk
    // is damaged. A pipeline that treats status 1 as a warning and carries on
    // will otherwise consume a truncated listing that reads as whole, and its
    // log will describe a disk problem rather than a missing-entries problem.
    // The status carries the distinction for a script; the wording has to carry
    // it for whoever reads the log afterwards.
    for (const std::string & note : listing.damage)
    {
        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, "",
            note + ". THIS LISTING IS INCOMPLETE, entries may be missing") + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded and read back as zeros "
                  "-- THIS LISTING IS INCOMPLETE, entries may be missing",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyEncoding
//
//  Turns the file's stored bytes into what the caller asked for. Verbatim is
//  the absence of a conversion and is the default, so a caller who says nothing
//  gets the bytes the disk holds.
//
//  AN ENCODING THIS BUILD CANNOT PERFORM IS REFUSED, NOT IGNORED. A flag that
//  is parsed and then silently dropped is worse than one that does not exist:
//  the caller reads "converted to a listing" in the help, gets tokenized bytes,
//  and has no way to tell the difference from a file that needed no conversion.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ApplyEncoding (
    const CommandLineOptions &  options,
    FilePayload              &  payload,
    DiskCommandResult        &  result)
{
    HRESULT                hr = S_OK;
    std::string            hostText;
    ApplesoftListingError  listingError;



    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            // Both conventions decode identically -- once the high bit is
            // ignored, Apple high-ASCII and plain-ASCII text differ in nothing -- so
            // this choice is inert on the read path. It becomes load-bearing
            // when writing, which is where the terminator gets chosen.
            //
            // Apple high-ASCII is the measured answer on both filesystems: the ProDOS
            // fixture volumes carry TXT files that are predominantly high-bit
            // with $8D terminators, not the plain seven-bit form usually
            // assumed for that filesystem.
            AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, hostText);

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            hr = ApplesoftTokenizer::Detokenize (payload.bytes, hostText, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot read this file as an Applesoft "
                                   "BASIC program", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunGet
//
//  Extraction goes to a named file when one is given and to the process's own
//  output otherwise, which is what makes it pipeable. Both destinations go
//  through the seam so the platform's opinion about binary output is settled in
//  one place rather than at each call site.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunGet (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.path.empty();
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    char                           note[128] = {};



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Read (path, payload);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, options.disk.path,
                    "could not be read from this volume"));

    hr = ApplyEncoding (options, payload, result);
    CHR (hr);

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload.bytes);

        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));
    }
    else
    {
        result.payload    = payload.bytes;
        result.hasPayload = true;
    }

    if (payload.hasLoadAddress)
    {
        snprintf (note, sizeof (note), "%s: loads at $%04X, %u bytes\n",
                  options.disk.path.c_str(),
                  (unsigned) payload.loadAddress,
                  (unsigned) payload.bytes.size());

        result.diagnostics += note;
    }

    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (note, sizeof (note),
                  "%d %s could not be decoded. THIS FILE IS INCOMPLETE, "
                  "unreadable sectors were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, options.disk.path, note) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::OnDiskNameFor
//
//  --as when the caller gave one, and otherwise the last component of the
//  component. Nothing is stripped or shortened on the way: the caller already
//  chose that name, and inventing a different one would leave them looking for
//  a file the catalog does not hold. A name the filesystem cannot store is
//  refused by the volume layer, which is where that rule belongs.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::OnDiskNameFor (const CommandLineOptions & options)
{
    size_t       lastSeparator = options.disk.hostFile.find_last_of ("/\\");
    std::string  name          = options.disk.path;



    if (name.empty())
    {
        name = (lastSeparator == std::string::npos)
                   ? options.disk.hostFile
                   : options.disk.hostFile.substr (lastSeparator + 1);
    }

    return name;
}





//  Integer BASIC stops numbering lines at 32767, so the high bit of a line
//  number is clear and zero is not a line at all.
static constexpr Word    s_kMaxIntegerLineNumber = 32767;


//  How many links have to agree before a chain is a chain rather than a
//  coincidence. One proves nothing on a short file.
static constexpr size_t  s_kLinesThatProveAChain = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DetectFileType
//
//  What a file's own bytes say it is, or 0 when they say nothing.
//
//  APPLESOFT IS A LINKED LIST AND THAT IS WHAT IS WALKED. Every line is a
//  two-byte pointer to the next line, a two-byte line number, tokens, and a
//  zero terminator; a zero pointer ends the program. Walking that chain with
//  the pointers rising and the line numbers rising, and landing EXACTLY on the
//  last byte of the file, is a structure arbitrary data does not have. The
//  exact landing is what makes a false positive vanishingly unlikely: a binary
//  whose first four bytes happen to look like a header still has to have every
//  later line agree, and end where the file ends.
//
//  INTEGER BASIC IS THE SAME SHAPE INSIDE OUT: a leading LENGTH byte rather
//  than a trailing zero. Walked the same way, to the same exact landing.
//
//  A DOS 3.3 FOUR-BYTE HEADER IS DELIBERATELY NOT READ AS A TYPE. `put --load`
//  writes that header itself, so a file already carrying one is the doubled-
//  header mistake the worked example warns about, and quietly agreeing with it
//  would file bytes the guest runs as code.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskCommandRunner::DetectFileType (const vector<Byte> & bytes, VolumeKind kind)
{
    Byte    type     = 0;
    bool    isDos    = kind == VolumeKind::Dos33;
    size_t  at       = 0;
    Word    previous = 0;
    Word    lastLine = 0;
    size_t  seen     = 0;



    //  Applesoft: a chain of lines ending on a zero next-pointer.
    //  Two bytes, not four: the chain ENDS on a bare zero pointer, and a
    //  loop that demanded a whole line would walk past it and never see
    //  the end of the program.
    while (at + 2 <= bytes.size())
    {
        Word    next       = (Word) (bytes[at] | (bytes[at + 1] << 8));
        Word    line       = 0;
        size_t  terminator = at + 4;



        if (next == 0)
        {
            //  The chain ends here, and it has to end WITH the file.
            bool  landedOnTheEnd = (at + 2 == bytes.size());

            if (landedOnTheEnd && seen > 0)
            {
                type = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
            }

            break;
        }

        if (at + 4 > bytes.size())
        {
            break;
        }

        line = (Word) (bytes[at + 2] | (bytes[at + 3] << 8));

        //  Pointers rise, and so do line numbers.
        if (next <= previous || (seen > 0 && line <= lastLine))
        {
            break;
        }

        while (terminator < bytes.size() && bytes[terminator] != 0)
        {
            terminator++;
        }

        if (terminator >= bytes.size())
        {
            break;
        }

        previous = next;
        lastLine = line;
        seen++;
        at       = terminator + 1;
    }

    //  Integer BASIC: a chain of length-prefixed lines, and DOS 3.3's alone.
    //
    //  TIGHTER THAN THE APPLESOFT WALK BECAUSE IT HAS LESS TO GO ON. Applesoft
    //  carries a pointer per line that has to rise AND land on the last byte,
    //  and measured over 200,000 random blobs it never once fired. Integer has
    //  only a length byte, so a short file can chain to its own end by
    //  accident: the same measurement put it at 0.39% on 16-byte blobs.
    //
    //  Two free constraints close that. A line number stops at 32767 on this
    //  machine, so the high bit is clear and zero is not a line; and three
    //  links have to agree rather than one. Re-measured, 0.39% becomes 0.0025%,
    //  and what a rejected file falls back to is B, which is where it would
    //  have gone anyway.
    at       = 0;
    lastLine = 0;
    seen     = 0;

    while (type == 0 && isDos && at + 3 <= bytes.size())
    {
        Byte  length = bytes[at];
        Word  line   = (Word) (bytes[at + 1] | (bytes[at + 2] << 8));



        if (length < 4 || at + length > bytes.size())
        {
            break;
        }

        if (line == 0 || line > s_kMaxIntegerLineNumber)
        {
            break;
        }

        if (seen > 0 && line <= lastLine)
        {
            break;
        }

        lastLine = line;
        seen++;
        at      += length;

        if (at == bytes.size())
        {
            type = (seen >= s_kLinesThatProveAChain) ? Dos33Volume::kTypeInteger : (Byte) 0;
            break;
        }
    }

    return type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveFileType
//
//  The two filesystems number their types differently and name them
//  differently, so the forms accepted here are each one's own. A letter
//  that means Applesoft on DOS 3.3 and nothing on ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  A caller who named no type gets the one that matches the conversion they
//  asked for -- text when they asked for text, a binary otherwise, which is
//  what a build loop places.
//
//  AN UNRECOGNIZED FORM IS REFUSED, not defaulted. Defaulting would place
//  the file under a type the caller did not ask for and say nothing, and the
//  guest would report the mismatch much later as a file that will not load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveFileType (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    Byte                      & outType,
    DiskCommandResult         & result)
{
    HRESULT      hr         = S_OK;
    bool         isDos      = kind == VolumeKind::Dos33;
    bool         isText     = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text;
    bool         isBasic    = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Basic;
    bool         named      = !options.disk.typeName.empty();
    bool         recognized = true;
    size_t       i          = 0;
    std::string  form       = options.disk.typeName;



    if (isBasic)
    {
        // A tokenized program stored under any other type is a file the guest
        // will not RUN, so the conversion picks the type rather than leaving it
        // to a default meant for a build loop's binaries.
        outType = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
    }
    else if (isText)
    {
        outType = isDos ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
    }
    else
    {
        //  What the file says it is, and a binary when it says nothing.
        Byte  detected = DetectFileType (hostBytes, kind);

        outType = (detected != 0)
                      ? detected
                      : (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
    }

    BAIL_OUT_IF (!named, S_OK);

    for (i = 0; i < form.size(); i++)
    {
        if (form[i] >= 'a' && form[i] <= 'z')
        {
            form[i] = (char) (form[i] - 'a' + 'A');
        }
    }

    if (isDos)
    {
        if      (form == "T" || form == "TXT") { outType = Dos33Volume::kTypeText; }
        else if (form == "I" || form == "INT") { outType = Dos33Volume::kTypeInteger; }
        else if (form == "A" || form == "BAS") { outType = Dos33Volume::kTypeApplesoft; }
        else if (form == "B" || form == "BIN") { outType = Dos33Volume::kTypeBinary; }
        else if (form == "R" || form == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                           { recognized = false; }
    }
    else
    {
        if      (form == "T" || form == "TXT") { outType = ProDosVolume::kTypeText; }
        else if (form == "B" || form == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (form == "A" || form == "BAS") { outType = ProDosVolume::kTypeBasic; }
        else if (form == "S" || form == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                           { recognized = false; }
    }

    if (!recognized)
    {
        result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype ", options.flagPrefix) + options.disk.typeName
                            + " means nothing on this volume: "
                            + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                                     : "ProDOS takes TXT, BIN, BAS or SYS")
                            + "\n";

        result.exitStatus   = DiskCommandResult::kNoOutput;
        hr                  = E_NOTIMPL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildPutPayload
//
//  What a file from the host becomes on the disk.
//
//  VERBATIM IS THE ABSENCE OF A CHARACTER CONVERSION AND NOTHING MORE. The
//  bytes go down as they came in; the length and whatever header the type
//  carries are still applied below this, because those record where the file
//  ENDS and are its identity rather than a transformation of it. That is what
//  makes extract-edit-replace safe: the bytes the caller did not touch come
//  back exactly as they were.
//
//  Text is written in the Apple high-ASCII convention on BOTH filesystems, which is
//  the measured answer rather than the assumed one -- the ProDOS fixture
//  volumes' own TXT files are predominantly high-bit with $8D terminators. It
//  is also what the read path decodes, so the two directions agree.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::BuildPutPayload (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    FilePayload               & outPayload,
    DiskCommandResult         & result)
{
    HRESULT                hr        = S_OK;
    Byte                   type      = 0;
    size_t                 badOffset = 0;
    char                   note[160] = {};
    std::string            hostText;
    ApplesoftListingError  listingError;



    hr = ResolveFileType (options, kind, hostBytes, type, result);
    CHR (hr);

    outPayload.type           = type;
    outPayload.loadAddress    = options.disk.loadAddress;
    outPayload.hasLoadAddress = options.disk.hasLoadAddress;

    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            outPayload.bytes    = hostBytes;
            outPayload.encoding = PayloadEncoding::Verbatim;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            hostText.assign (hostBytes.begin(), hostBytes.end());

            hr = AppleTextCodec::Encode (hostText, AppleTextConvention::HighAscii,
                                         outPayload.bytes, badOffset);

            if (FAILED (hr))
            {
                // Naming the offset is the whole value of the refusal. A smart
                // quote pasted into a listing looks identical to a plain one in
                // an editor, and "somewhere in this file" is not actionable.
                snprintf (note, sizeof (note),
                          "byte %u of the file has no Apple II text representation, "
                          "so nothing was converted\n",
                          (unsigned) badOffset);

                result.diagnostics += note;
                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            outPayload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            if (options.disk.hasLoadAddress)
            {
                // An Applesoft program loads where Applesoft keeps its program
                // and nowhere else, so an address here is a request that cannot
                // be honored. Accepting and ignoring it would place the program
                // and leave the caller believing it loads somewhere it does not.
                result.diagnostics += DiskHelpPage::ApplyPrefixes (
                    "%Lload means nothing with %Lbasic: "
                    "an Applesoft BASIC program always loads at $0801\n", options.flagPrefix);
                result.exitStatus   = DiskCommandResult::kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER);
                break;
            }

            hostText.assign (hostBytes.begin(), hostBytes.end());

            hr = ApplesoftTokenizer::Tokenize (hostText, outPayload.bytes, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot make an Applesoft BASIC program of "
                                   "this listing", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            // ProDOS records where a BASIC program loads in the auxiliary type;
            // DOS 3.3 records nothing and ignores this. Setting it on both keeps
            // the branch out of here, where it would be a second statement of
            // which filesystem stores what.
            outPayload.auxType        = ApplesoftTokenizer::kProgramBase;
            outPayload.hasAuxType     = true;
            outPayload.hasLoadAddress = false;
            outPayload.encoding       = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunPut
//
//  Placing a file from the host on the disk: read it, convert it if asked, let the
//  filesystem compute the whole new volume, render that back into the container
//  and commit it.
//
//  NOTHING IS WRITTEN UNTIL EVERY DECISION HAS BEEN MADE. Each refusal below --
//  no such file on the host, a type nobody recognizes, a name the catalog cannot
//  store, a locked file, a volume with no room -- happens with the image on
//  disk untouched, because the only thing this path can commit is a finished
//  buffer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunPut (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.hostFile.empty();
    std::string                    diskName  = OnDiskNameFor (options);
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    vector<Byte>                   hostBytes;
    vector<Byte>                   edited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<file>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, hostBytes);

    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    hr = BuildPutPayload (options, opened.kind, hostBytes, payload, result);
    CHR (hr);

    path = FilePath::Parse (diskName);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Write (path, payload, edited);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, diskName,
                    WithPrefix (DescribeVolumeRefusal (hr))));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunDelete
//
//  Removal, plus the account of what it declined to do.
//
//  THE ACCOUNT IS THE POINT OF REPORTING AT ALL. A delete frees only space the
//  file uniquely owned; anything another catalog entry also claims is left
//  allocated, because freeing it would damage that other file and nothing would
//  say so until somebody read it, possibly weeks later. Leaked space is
//  recoverable at any time and a cross-linked free is not, so the asymmetry
//  runs this way -- and the user is told, on the error stream, with the command
//  still reporting that it succeeded.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunDelete (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr      = S_OK;
    bool                           named   = !options.disk.path.empty();
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    DeleteOutcome                  outcome;
    vector<Byte>                   edited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);



        hr = volume.Delete (path, edited, outcome);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, options.disk.path,
                    WithPrefix (DescribeVolumeRefusal (hr))));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, options.disk.path, warning) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBoot
//
//  Which program the disk runs once its operating system has loaded.
//
//  A PROGRAM THAT IS NOT ON THE VOLUME IS REFUSED, BY NAME. Setting a startup
//  program is the one edit whose mistake is invisible until somebody boots the
//  disk -- there is no listing that shows it and no file that appears -- so a
//  typo accepted here surfaces as a machine that boots to an error, hours later
//  and somewhere else. The volume layer looks the name up for exactly this
//  reason, and the message says which name failed rather than that something
//  did.
//
//  The two filesystems reach the same outcome by entirely different means, and
//  neither of them is a write of a file: DOS 3.3 patches a name into its own
//  image and ProDOS reorders its volume directory. Both arrive here as one call
//  and leave through the same commit as every other edit.
//
//  A DOS 3.3 GREETING IS RUN, NOT BRUN, AND THAT IS SAID OUT LOUD. Booting the
//  stock master with a binary named as its greeting was measured: the disk
//  boots, the program does not run, and the screen shows no complaint anybody
//  would connect to it. The name field is the only thing this sets, so the
//  command DOS issues stays RUN -- which leaves a placement that succeeded and a
//  disk that does nothing. Saying so costs one line and the complaints status;
//  refusing outright would be wrong, since a disk whose boot command has been
//  patched by hand is a real thing and its owner knows what they are doing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBoot (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    HRESULT                        listHr   = S_OK;
    bool                           named    = !options.disk.path.empty();
    bool                           runnable = true;
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    VolumeListing                  listing;
    vector<Byte>                   edited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);



        hr = volume.SetStartupProgram (path, edited);

        if (SUCCEEDED (hr) && opened.kind == VolumeKind::Dos33)
        {
            listHr = volume.Enumerate (listing);
            IGNORE_RETURN_VALUE (listHr, S_OK);

            runnable = IsRunnableAsDos33Greeting (listing, options.disk.path);
        }
    }

    CHRF (hr, result.Fail (options.disk.imagePath, options.disk.path,
                    WithPrefix (DescribeVolumeRefusal (hr))));

    //  REFUSED BEFORE ANYTHING IS WRITTEN, not reported after.
    //
    //  This used to set the name, commit the image, and then say the disk
    //  would boot without running it. The reasoning was that a DOS patched by
    //  hand to BRUN rather than RUN is a real thing and refusing would block
    //  it. What it produced for everyone else was a command that reported
    //  trouble and changed the disk anyway, leaving a volume configured to
    //  start a program that cannot start -- which is what ProDOS refuses
    //  outright, two screens away in the same command.
    CBRFEx (runnable, HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE),
            result.Fail (options.disk.imagePath, options.disk.path,
                  "is not a program a booting DOS 3.3 can run. Its greeting is RUN, "
                  "which starts an Applesoft BASIC or Integer BASIC program, and "
                  "this file is neither"));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::IsRunnableAsDos33Greeting
//
//  Whether a booting DOS 3.3 would actually run this file.
//
//  Measured against the stock master rather than reasoned about: with a binary
//  named as the greeting the machine boots and the program never runs, because
//  the command DOS issues at boot is RUN. Anything RUN does not understand is a
//  greeting in name only.
//
//  A name that is not on the volume answers true, because the refusal for that
//  belongs to the layer that looked it up and one refusal per problem is the
//  rule.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                                   const std::string    & name)
{
    bool  runnable = true;



    for (const FileEntry & entry : listing.entries)
    {
        if (_stricmp (entry.name.c_str(), name.c_str()) == 0)
        {
            runnable = entry.type == Dos33Volume::kTypeApplesoft
                    || entry.type == Dos33Volume::kTypeInteger;
            break;
        }
    }

    return runnable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Run
//
//  A command this build does not implement reports failure rather than doing
//  nothing quietly, so an absent capability cannot be mistaken for a completed
//  operation.
//
//  A HELP REQUEST IS A COMMAND HERE, and it is answered on the output stream with
//  a clean status. It used to be recognized only as the FIRST argument of the
//  whole command line, so `disk --help` offered `--help` to the command table and
//  answered a request for the grammar by refusing to run and complaining about
//  the grammar -- exit 2 for a question the tool knows the answer to.
//
//  A REFUSED COMMAND LINE RUNS NOTHING, for the reason `run` gives: this
//  grammar has no ignorable mistake in it. An option it did not recognize might
//  have named the file to write, chosen the type the catalog records, or given
//  the load address a binary needs, so carrying on would edit a disk on terms
//  nobody asked for. The parser has already said what was wrong, in the words
//  of the argument it could not take; repeating it here would report one
//  mistake twice.
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandResult DiskCommandRunner::Run (const CommandLineOptions & options)
{
    HRESULT                   hr               = S_OK;   // vestigial, for the bails
    DiskCommandResult         result;
    std::vector<std::string>  missing;
    bool                      refused          = options.parseVerdict == CommandLineOptions::ParseVerdict::Refused;
    bool                      allOperandsGiven = false;



    m_command    = options.disk.command;
    m_flagPrefix = options.flagPrefix;

    CBRF (!refused, result.exitStatus = DiskCommandResult::kNoOutput);

    //  ASKED ONCE, BEFORE ANY COMMAND RUNS. Each command used to check its own
    //  operands wherever it first needed them, which reported whichever one
    //  that command happened to reach first and never the rest.
    missing          = FindMissingParameters (options);
    allOperandsGiven = missing.empty();
    CBRF (allOperandsGiven, ReportMissingParameters (missing, result));

    switch (options.disk.command)
    {
        case CommandLineOptions::DiskOptions::Command::List:
            RunList (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Get:
            RunGet (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Put:
            RunPut (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Delete:
            RunDelete (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Boot:
            RunBoot (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockRead:
            RunBlockRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockWrite:
            RunBlockWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorRead:
            RunSectorRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorWrite:
            RunSectorWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Create:
            RunCreate (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Init:
            RunInit (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Help:
            result.output     += DiskHelpPage::BuildHelpText (options.flagPrefix, m_banner);
            result.exitStatus  = DiskCommandResult::kClean;
            break;

        default:
            //  NAMING NO COMMAND PRINTS THE PAGE AND SAYS NOTHING ELSE. There
            //  is one thing a reader in that position needs and it is the list
            //  of commands; a sentence telling them they gave none is a line
            //  between them and it. The status is still 2, because nothing was
            //  done: a script that reached here with an empty variable has to
            //  be able to tell.
            if (options.disk.commandWord.empty())
            {
                //  AND SAYS SO, or the edge prints the page a second time.
                //  A bad command line makes the caller print the whole disk
                //  page unless the runner has already answered with usage;
                //  this arm was answering with usage and not saying it, so
                //  `disk` alone printed 326 lines where 163 were meant.
                result.output     += DiskHelpPage::BuildHelpText (options.flagPrefix, m_banner);
                result.usageShown  = true;
            }
            else
            {
                result.diagnostics += "Error: unknown disk command: "
                                    + options.disk.commandWord + "\n";
            }

            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            break;
    }

    //  THE ONE COMMAND'S BLOCK, NOT ALL NINE.
    //
    //  A missing operand already answered this way and a bad option value did
    //  not, so the same command answered `sectorread` with no image in 18
    //  lines and `sectorread --track 99` in 194. Both readers have said which
    //  command they want; the difference was in how they got it wrong, which
    //  is no reason to hand one of them every other command on the page.
    //
    //  Here rather than at each refusal, so a refusal added later cannot
    //  forget it and the sites stay about the reason. An unrecognized command
    //  word is deliberately excluded: it leaves the command as None, and a
    //  reader who has not landed on a command is the one case the whole page
    //  is the answer to.
    if (result.badCommandLine
        && !result.usageShown
        && options.disk.command != CommandLineOptions::DiskOptions::Command::None
        && options.disk.command != CommandLineOptions::DiskOptions::Command::Help)
    {
        result.output     += DiskHelpPage::BuildCommandHelp (m_command, m_flagPrefix);
        result.usageShown  = true;
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::AdvertisedContainers
//
//  The container table, for anyone who has to agree with it.
//
////////////////////////////////////////////////////////////////////////////////

const DiskCommandRunner::ContainerName * DiskCommandRunner::AdvertisedContainers (size_t & outCount)
{
    outCount = _countof (s_kContainers);

    return s_kContainers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ContainerWordList
//
//  The advertised words as a list in a sentence: `dsk, do, po and woz`.
//
//  READ OFF THE TABLE RATHER THAN TYPED OUT, because a sentence that names
//  the containers is a promise about what the tool accepts. Written by hand it
//  is a promise nothing keeps: `do` sat in the table, in both of these
//  sentences and in the extension reader, and the builder refused it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::ContainerWordList (const char * prefix, const char * conjunction)
{
    std::string  list;
    size_t       count = _countof (s_kContainers);
    size_t       i     = 0;



    for (i = 0; i < count; i++)
    {
        if (i > 0)
        {
            //  Serial comma before the last of three or more.
            if (i + 1 < count)
            {
                list += ", ";
            }
            else
            {
                list += (count > 2) ? std::string (", ") + conjunction + " "
                                    : std::string (" ") + conjunction + " ";
            }
        }

        list += prefix;
        list += s_kContainers[i].name;
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeSpecRefusal
//
//  Why a settled spec cannot be written, in words -- or empty when it can be.
//
//  THE BUILDER'S RULES ARE ASKED, NOT RESTATED. CheckSpec holds the pairing
//  matrix and answers in verdicts precisely so this layer can say which rule
//  was broken; a second copy of the matrix here would be a second thing to get
//  wrong, and the two would disagree the first time one of them changed.
//
//  Reporting only the broken rule matters more than it looks. The message
//  this replaced recited all three at once, so somebody who mistyped a
//  ProDOS volume name was handed a paragraph about sector order.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeSpecRefusal (const BlankDiskSpec & spec)
{
    std::string       text;
    BlankDiskVerdict  verdict = BlankDiskBuilder::CheckSpec (spec);



    switch (verdict)
    {
        case BlankDiskVerdict::ContentsNotInContainer:
            text = "Error: illegal container and filesystem combination\n"
                   "       .dsk and .do hold DOS 3.3, .po holds ProDOS, and .woz holds\n"
                   "       either.\n";
            break;

        case BlankDiskVerdict::BootableNeedsFilesystem:
            text = "Error: cannot make an unformatted disk bootable\n"
                   "       There is no filesystem to copy an operating system into.\n"
                   "       Format the disk as dos33 or prodos.\n";
            break;

        case BlankDiskVerdict::ProDosNameUnusable:
            text = "Error: illegal volume name\n"
                   "       ProDOS volume names are 1-15 characters, starting with a\n"
                   "       letter, and can include letters, digits, and periods.\n";
            break;

        default:
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveContainer
//
//  Which container a new image is written as.
//
//  --type when it is given, the file's own extension when it is not. An
//  unknown word is refused BY NAME with the ones that exist: somebody who
//  typed `--type 2mg` meant it, and handing them a .dsk instead is a disk they
//  did not ask for under a name they did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveContainer (const CommandLineOptions & options,
                                             size_t                   & outNibbleTrackSize,
                                             DiskFormat               & outFormat,
                                             DiskCommandResult        & result)
{
    HRESULT      hr         = S_OK;
    bool         found      = false;
    bool         asWord     = !options.disk.containerType.empty();
    std::string  asked      = options.disk.containerType;
    std::string  words      = ContainerWordList ("", "and");
    std::string  extensions = ContainerWordList (".", "and");
    size_t       dot        = 0;



    if (!asWord)
    {
        //  No --type, so the name decides. THE WRITE LIST ANSWERS IT, not the
        //  extension reader: the reader recognizes what a file already is, and
        //  is the wrong question for what a new one may be made as. The word
        //  and the extension are the same string for every container here, so
        //  one table serves both branches.
        dot   = options.disk.imagePath.find_last_of ('.');
        asked = (dot == std::string::npos) ? std::string()
                                           : options.disk.imagePath.substr (dot + 1);
    }

    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    for (const ContainerName & entry : s_kContainers)
    {
        if (!asked.empty() && asked == entry.name)
        {
            outFormat          = entry.format;
            outNibbleTrackSize = entry.nibbleTrackSize;
            found              = true;
            break;
        }
    }

    if (!asWord)
    {
        CBRF (found, (result.diagnostics    += "Error: missing image type: "
                                             + options.disk.imagePath + "\n"
                                             + "       Valid extensions are " + extensions + ".\n"
                                             + WithPrefix ("       Use %Ltype to specify the type.\n"),
                      result.exitStatus      = DiskCommandResult::kNoOutput,
                      result.badCommandLine  = true));
    }
    else
    {
        CBRFEx (found, E_INVALIDARG,
                (result.diagnostics    += "Error: unknown image type: "
                                        + options.disk.containerType + "\n"
                                          "       Valid types are " + words + ".\n",
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveContents
//
//  What goes inside the container: a DOS 3.3 catalog, a ProDOS directory, or
//  nothing at all.
//
//  DOS 3.3 IS THE DEFAULT because it is what a disk written by this tool is
//  most likely to be for: `put` and `boot` both work on one without anything
//  further. `none` is a genuinely useful answer -- a raw sector image for a
//  guest that formats it itself -- so it is offered by name rather than being
//  where an unrecognized word quietly lands.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveContents (const CommandLineOptions & options,
                                            BlankDiskContents        & outContents,
                                            DiskCommandResult        & result)
{
    HRESULT      hr         = S_OK;   // vestigial, for the bail
    bool         recognized = true;
    std::string  asked      = options.disk.formatName;



    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    if (asked.empty() || asked == "dos33" || asked == "dos" || asked == "dos3.3")
    {
        outContents = BlankDiskContents::Dos33;
    }
    else if (asked == "prodos")
    {
        outContents = BlankDiskContents::ProDos;
    }
    else if (asked == "none" || asked == "raw" || asked == "unformatted")
    {
        outContents = BlankDiskContents::Unformatted;
    }
    else
    {
        recognized = false;
    }

    CBRFEx (recognized, E_INVALIDARG,
            (result.diagnostics    += "Error: unknown format: " + options.disk.formatName + "\n"
                                      "       Valid formats are dos33, prodos, and none.\n",
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveVolume
//
//  --volume, which is a NUMBER under DOS 3.3 and a NAME under ProDOS.
//
//  The two filesystems label a disk differently and one flag serves both, so
//  which one the word is read as follows from the format already chosen rather
//  than from how the word looks. A ProDOS volume legitimately called `254`
//  would otherwise become a DOS volume number, silently.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveVolume (const CommandLineOptions & options,
                                          BlankDiskSpec            & inOutSpec,
                                          DiskCommandResult        & result)
{
    HRESULT              hr             = S_OK;   // vestigial, for the bail
    const std::string &  asked          = options.disk.volumeName;
    bool                 named          = !asked.empty();
    int                  number         = 0;
    bool                 isVolumeNumber = false;



    //  No %Lvolume at all keeps the builder's default, which is not a
    //  failure of anything.
    BAIL_OUT_IF (!named, S_OK);

    //  UPPERCASED HERE, WHERE IT IS ACCEPTED, and not only where it is
    //  written. ProDOS holds a volume name in upper case and compares without
    //  regard to case, so `--volume mydisk` is a perfectly good way to ask for
    //  /MYDISK. The skeleton has always stored it correctly. What it did not do
    //  was tell the spec, so the line confirming the disk read back the name
    //  that was typed while `disk list` read back the name that is there, and
    //  the two disagreed over a disk that was right all along.
    if (inOutSpec.contents == BlankDiskContents::ProDos)
    {
        inOutSpec.volumeName = asked;

        for (char & letter : inOutSpec.volumeName)
        {
            letter = (char) toupper ((unsigned char) letter);
        }
    }
    else
    {
        //  A DOS 3.3 volume number, and only a number. Read by hand so a word
        //  that is not one at all is refused rather than quietly reading as
        //  zero.
        for (char letter : asked)
        {
            if (letter < '0' || letter > '9')
            {
                number = -1;
                break;
            }

            number = (number * 10) + (letter - '0');
        }

        isVolumeNumber = number >= 1 && number <= 254;
        CBRFEx (isVolumeNumber, E_INVALIDARG,
                (result.diagnostics    += "Error: illegal volume number\n"
                                          "       DOS 3.3 volume numbers are 1-254. Format the "
                                          "disk as prodos for a\n"
                                          "       volume with a name instead of a number.\n",
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        inOutSpec.volumeNumber = (Byte) number;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveBoot
//
//  Whether the disk boots, and from which operating system.
//
//  THE MASTER IS NAMED RATHER THAN FOUND. Making a disk bootable means copying
//  an operating system onto it, so there has to be one to copy from -- and
//  where the emulator keeps its downloaded copy is the executable's knowledge
//  rather than this library's. So the path arrives on the command line, where
//  a script points at whichever master it has.
//
//  --boot, which starts a binary with no operating system at all, is not here
//  yet: DirectBootBuilder produces a whole image rather than a boot sector to
//  lay over a formatted one, so it is its own path and not a flag on this one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveBoot (const CommandLineOptions & options,
                                        BlankDiskSpec            & inOutSpec,
                                        BootPayload              & outPayload,
                                        DiskCommandResult        & result)
{
    HRESULT                hr       = S_OK;
    bool                   isProDos = inOutSpec.contents == BlankDiskContents::ProDos;
    StockBootDisks::Which  which    = isProDos ? StockBootDisks::Which::ProDosUsersDisk
                                               : StockBootDisks::Which::Dos33Master;
    std::string   master        = options.disk.bootableFrom;
    bool          bothWaysAsked = false;
    bool          cached        = false;
    vector<Byte>  osBytes;



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    bothWaysAsked = !options.disk.directBootFile.empty() && options.disk.bootable;
    CBRFEx (!bothWaysAsked, E_INVALIDARG,
            (result.diagnostics    += WithPrefix (
                 "Error: %Lbootable and %Lboot are mutually exclusive\n"
                 "       %Lbootable copies an operating system onto the disk.\n"
                 "       %Lboot writes a binary that runs without one.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    //  Not asked for at all is the ordinary case, not a failure.
    BAIL_OUT_IF (!options.disk.bootable, S_OK);

    //  A BARE --bootable MEANS THE ONE THE EMULATOR DOWNLOADED. Which of the
    //  two that is follows the format being written, because a ProDOS disk
    //  cannot be made bootable out of a DOS master.
    if (master.empty())
    {
        cached = StockBootDisks::IsCached (which);
        CBRFEx (cached, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                (result.diagnostics    += std::string ("Error: the ")
                                        + (isProDos ? "ProDOS" : "DOS 3.3")
                                        + " master has not been downloaded\n"
                                        + WithPrefix ("       Run the emulator once to download it, "
                                                      "or supply a master\n"
                                                      "       image with %Lbootable <image>.\n"),
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        master = StockBootDisks::PathFor (which);
    }

    hr = m_fileIo.ReadAllBytes (master, osBytes);
    CHRF (hr, result.Fail (master, "", "cannot be read, so there is no operating system to copy"));

    //  Which slot it fills follows the format being written, because that is
    //  the one the builder will reach for.
    if (isProDos)
    {
        outPayload.proDosUsersDisk = osBytes;
    }
    else
    {
        outPayload.dosMasterSectors = osBytes;
    }

    inOutSpec.bootable = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeNewDisk
//
//  What was just written, in the words the flags used to ask for it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeNewDisk (const BlankDiskSpec & spec)
{
    std::string  text;



    switch (spec.contents)
    {
        case BlankDiskContents::Dos33:       text = "DOS 3.3";      break;
        case BlankDiskContents::ProDos:      text = "ProDOS";       break;
        default:                             text = "unformatted";  break;
    }

    if (spec.contents == BlankDiskContents::Dos33)
    {
        text += ", volume " + std::to_string ((int) spec.volumeNumber);
    }
    else if (spec.contents == BlankDiskContents::ProDos)
    {
        text += ", volume " + spec.volumeName;
    }

    text += spec.bootable ? ", bootable" : ", not bootable";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildAndWrite
//
//  Everything create and init share: settle the spec, build the bytes, put
//  them where they go.
//
//  ALL OR NOTHING, through the same commit path every other write uses. A
//  build that fails leaves the target exactly as it was, which for `init`
//  means the disk somebody was reformatting is still the disk they had.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::BuildAndWrite (const CommandLineOptions & options,
                                       DiskFormat                 format,
                                       size_t                     nibbleTrackSize,
                                       bool                       overExisting,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    BlankDiskSpec                  spec;
    BootPayload                    payload;
    vector<Byte>                   imageBytes;
    DiskImageSession::OpenedImage  target;
    std::string                    refusal;
    bool                           pairingHolds = false;



    spec.format          = format;
    spec.nibbleTrackSize = nibbleTrackSize;

    hr = ResolveContents (options, spec.contents, result);
    CHR (hr);

    hr = ResolveVolume (options, spec, result);
    CHR (hr);

    hr = ResolveBoot (options, spec, payload, result);
    CHR (hr);

    //  THE PAIRING RULES ARE THE BUILDER'S, AND ITS VERDICT IS ASKED FOR
    //  RATHER THAN ITS HRESULT. ValidateSpec answers in E_INVALIDARG, which
    //  asserts, and that was right while the create dialog was the only
    //  caller: its dropdowns cannot express an illegal combination. This
    //  command line can, in one word, so every rule the builder holds is
    //  now reachable by typing and none of them is a caller's bug.
    refusal      = DescribeSpecRefusal (spec);
    pairingHolds = refusal.empty();

    CBRF (pairingHolds,
          (result.diagnostics    += refusal,
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = BlankDiskBuilder::Build (spec, payload, imageBytes);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    //  An image being made for the first time has nothing to be stale against,
    //  so the freshness guard the commit path applies to an edit is the wrong
    //  question here. Reformatting one that already exists keeps it.
    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = !overExisting;

    if (overExisting)
    {
        hr = m_fileIo.Stat (options.disk.imagePath, target.stamp);
        target.stampRecorded = SUCCEEDED (hr);
    }

    hr = m_session.CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += options.disk.imagePath + ": " + DescribeNewDisk (spec) + "\n";
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunCreate
//
//  A new image file, of a container this tool decides here.
//
//  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
//  keystroke from a disk they no longer have, and `create` is the command they
//  reach for when they are not thinking about what is already there. The
//  refusal names `init`, which is the command for meaning it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunCreate (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT     hr              = S_OK;
    DiskFormat  format          = DiskFormat::Dsk;
    size_t      nibbleTrackSize = 0;
    bool        named           = !options.disk.imagePath.empty();
    bool        alreadyThere    = false;



    CBRF (named, ReportMissingParameter ("<image>", result));

    alreadyThere = m_fileIo.Exists (options.disk.imagePath);
    CBRF (!alreadyThere, result.Fail (options.disk.imagePath, "",
                                      "is already there, and create will not write over it. "
                                      "Use init to reformat it, or choose another name"));

    hr = ResolveContainer (options, nibbleTrackSize, format, result);
    CHR (hr);

    if (!options.disk.directBootFile.empty())
    {
        BuildDirectBoot (options, format, nibbleTrackSize, result);
    }
    else
    {
        BuildAndWrite (options, format, nibbleTrackSize, false, result);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildDirectBoot
//
//  A disk that starts a binary with no operating system on it at all.
//
//  A SEPARATE PATH RATHER THAN A FLAG ON THE OTHER ONE, because there is no
//  filesystem here to put the binary into. DirectBootBuilder writes a loader
//  into the boot sector and lays the payload down in the sectors after it; the
//  disk has no VTOC, no catalog and no directory, and `list` will say so.
//
//  Both builders hand back the same 143,360-byte DOS-ordered buffer, so the
//  container is settled by the same function either way.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::BuildDirectBoot (const CommandLineOptions & options,
                                         DiskFormat                 format,
                                         size_t                     nibbleTrackSize,
                                         DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    DirectBootSpec                 spec;
    vector<Byte>                   payload;
    vector<Byte>                   sectors;
    vector<Byte>                   imageBytes;
    DiskImageSession::OpenedImage  target;
    std::string                    refusal;
    bool                           formatAgrees = false;
    char                           summary[160] = {};



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    //
    //  Checked here as well as in ResolveBoot, because this path never reaches
    //  ResolveBoot: it builds no filesystem, so it skips the code that would
    //  have caught the pair. Measured before the check went in, the two
    //  together honored --boot and dropped --bootable without a word.
    CBRF (!options.disk.bootable,
          (result.diagnostics    += WithPrefix (
               "Error: %Lbootable and %Lboot are mutually exclusive\n"
               "       %Lbootable copies an operating system onto the disk.\n"
               "       %Lboot writes a binary that runs without one.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    //  A filesystem was asked for AND a disk with none. Refused rather than
    //  quietly dropping one of them.
    formatAgrees = options.disk.formatName.empty() || options.disk.formatName == "none";
    CBRF (formatAgrees,
          (result.diagnostics    += WithPrefix ("Error: %Lboot and %Lformat are mutually exclusive\n"
                                                "       %Lboot writes no filesystem. A direct-boot "
                                                "disk contains the\n"
                                                "       binary and nothing else.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = m_fileIo.ReadAllBytes (options.disk.directBootFile, payload);
    CHRF (hr, result.Fail (options.disk.directBootFile, "", "cannot be read, so there is nothing to boot"));

    //  The load address is the one the binary was assembled for, and --load is
    //  how the caller says so. The entry follows it unless named separately.
    if (options.disk.hasLoadAddress)
    {
        spec.loadAddress = options.disk.loadAddress;
    }

    spec.entryAddress = options.disk.hasEntryAddress ? options.disk.entryAddress
                                                     : spec.loadAddress;

    //  The builder names exactly one reason, and it is the reason: an address
    //  outside the window has a capacity of zero, so blaming the payload's
    //  length for an address problem would be answering the wrong question.
    hr = DirectBootBuilder::Build (payload, spec, sectors, refusal);
    CHRF (hr, result.Fail (options.disk.imagePath, options.disk.directBootFile, refusal));

    hr = BlankDiskBuilder::WrapInContainer (format, nibbleTrackSize, false, sectors, imageBytes);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = true;

    hr = m_session.CommitImage (target, imageBytes, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: direct boot, %zu bytes loading at $%04X, entered at $%04X\n",
              options.disk.imagePath.c_str(), payload.size(),
              (unsigned) spec.loadAddress, (unsigned) spec.entryAddress);

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunInit
//
//  A disk that is already there, formatted again.
//
//  THE CONTAINER IS NOT A CHOICE HERE. It was decided when the file was made,
//  and this command rewrites what is INSIDE it -- so `init` takes no --type, and
//  a reader who wants a different container wants a different file, which is
//  what `create` makes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunInit (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT     hr              = S_OK;
    DiskFormat  format          = DiskFormat::Dsk;
    size_t      nibbleTrackSize = 0;
    FileStamp   stamp;
    bool        named           = !options.disk.imagePath.empty();
    bool        exists          = false;
    bool        untyped         = options.disk.containerType.empty();



    CBRF (named, ReportMissingParameter ("<image>", result));

    exists = m_fileIo.Exists (options.disk.imagePath);
    CBRF (exists, result.Fail (options.disk.imagePath, "", "is not there. Use create to make one"));

    CBRF (untyped,
          (result.diagnostics    += WithPrefix (
               "Error: init does not accept %Ltype\n"
               "       The image already has a container type. Use create to make an\n"
               "       image with a different type.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    //  From the file's own name, because the file is what is being reformatted.
    hr = DiskImageStore::GetSourceFormatByExtension (options.disk.imagePath, format);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "is not a kind of image this tool writes"));

    if (format == DiskFormat::Nib)
    {
        //  THE LENGTH DECIDES HERE, NOT THE NAME, which is the opposite of
        //  create and for a good reason: this file exists. Either nibble track
        //  size circulates under either extension, so reformatting by the name
        //  would resize a .nib that happens to hold the smaller tracks --
        //  turning a reformat into a rewrite of the whole container.
        hr = m_fileIo.Stat (options.disk.imagePath, stamp);
        CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be measured"));

        hr = NibbleImageCodec::ResolveGeometry ((size_t) stamp.sizeBytes, nibbleTrackSize);
        CHRF (hr, result.Fail (options.disk.imagePath, "",
                               "is not a nibble image of a length this tool recognizes"));
    }

    BuildAndWrite (options, format, nibbleTrackSize, true, result);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorRead
//
//  THE OTHER HALF OF sectorwrite, and the reason it is worth having: get goes
//  through a catalog, so on a disk with no filesystem there was no way at all
//  to read back what had just been written. The listing said as much -- "it
//  simply keeps its files somewhere this tool does not read" -- which was true
//  and is no longer.
//
//  A count is a parameter here and not on the write because a write knows its
//  own length and a read cannot: what usually records where a file ends is the
//  catalog, and these are the disks that have none.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RefuseBadValue (DiskCommandResult & result, const char * summary)
{
    result.diagnostics    += summary;
    result.exitStatus      = DiskCommandResult::kNoOutput;
    result.badCommandLine  = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorRead
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunSectorRead (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         first        = 0;
    size_t                         total        = 0;
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    //  WHICH NUMBERING, OR NOTHING. The same sixteen sectors answer to two
    //  orders, and a command line that does not say which is how bytes once
    //  landed on the wrong sector -- so there is no default to fall back to.
    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, RefuseBadValue (result,
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibblizationLayer::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibblizationLayer::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibblizationLayer::kTrackCount - 1,
                  NibblizationLayer::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal sector count: %d\n"
                  "       The sector count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, RefuseBadValue (result, summary));

    first = (size_t) (options.disk.track * NibblizationLayer::kSectorsPerTrack
                    + options.disk.sector);
    total = (size_t) (NibblizationLayer::kTrackCount * NibblizationLayer::kSectorsPerTrack);
    fits  = first + (size_t) options.disk.count <= total;

    if (!fits)
    {
        size_t  spare = total - first;

        snprintf (summary, sizeof (summary),
                  "Error: not enough sectors available\n"
                  "       Requested %d %s starting at track %d sector %d,\n"
                  "       but only %zu %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"),
                  options.disk.track, options.disk.sector,
                  spare,
                  Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    //  No filesystem needed, and none looked for. That is the whole point of
    //  the command: a disk built by sectorwrite may have no catalog at all.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    //  Each position maps through the STATED numbering, one at a time. Under
    //  --logical that is the identity into the DOS-ordered buffer; under
    //  --physical it is the interleave, so consecutive positions deliver the
    //  sectors in the order the drive presents them under consecutive
    //  address marks.
    for (int index = 0; index < options.disk.count; index++)
    {
        size_t  at = SectorRecordOffset (options.disk.numbering, first + (size_t) index);

        if (at + (size_t) NibblizationLayer::kSectorByteSize > opened.sectors.size())
        {
            break;
        }

        payload.insert (payload.end(),
                        opened.sectors.begin() + (ptrdiff_t) at,
                        opened.sectors.begin() + (ptrdiff_t) (at + NibblizationLayer::kSectorByteSize));
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from track %d %s sector %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.track, how, options.disk.sector,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, and it matters more here than
    //  anywhere: these bytes have no catalog and no length behind them, so a
    //  sector delivered as zeros looks exactly like a sector that holds zeros.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorWrite
//
//  A file from the host laid into an image at a track and a sector, in
//  whichever numbering the command line declared.
//
//  NO FILESYSTEM IS INVOLVED, WHICH IS THE POINT. A demo that boots its own
//  loader and reads fixed tracks has no catalog to make an entry in and no
//  allocator to ask for space, so `put` cannot express it at all. This writes
//  the bytes given, where it is told, and nothing else.
//
//  THE NUMBERING IS DECLARED, NEVER DEFAULTED. The same sixteen sectors
//  answer to two orders: --logical is what catalogs, RWTS callers and every
//  DOS-era sector editor speak, the identity into a DOS-ordered image; and
//  --physical is the address-field order a boot loader asking the drive ROM
//  sees, the interleave. This command once applied the interleave silently
//  under the belief the image was physical-ordered -- `--sector 1` landed on
//  logical sector 7, and sectorread's matching belief read it back perfectly
//  -- which is why an unstated numbering is refused rather than guessed. The
//  skew case in DirectBootTests is what settled the orientation against
//  DOS's own table at $084D.
//
//  It runs on past the end of a track into the next one, because a payload
//  longer than 4 KB is ordinary and splitting the call per track would put the
//  wrap arithmetic back in the caller's hands. Under --physical the run-on
//  advances by address mark, so page N of the payload sits under mark N --
//  exactly what a loader that files sectors by address mark wants.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunSectorWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, ReportMissingParameter ("<file>", result));

    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, RefuseBadValue (result,
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibblizationLayer::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibblizationLayer::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibblizationLayer::kTrackCount - 1,
                  NibblizationLayer::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A disk with no filesystem is the ordinary case here rather than a
    //  refusal: a demo that boots its own loader has no catalog, and that
    //  is exactly the disk this command exists to write.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    edited = opened.sectors;

    //  Whole sectors, because that is the unit the drive reads. A payload that
    //  does not fill its last one is padded with what was already there rather
    //  than with a value this command invented.
    needed = (payload.size() + (size_t) NibblizationLayer::kSectorByteSize - 1)
           / (size_t) NibblizationLayer::kSectorByteSize;

    {
        size_t  first = (size_t) (options.disk.track * NibblizationLayer::kSectorsPerTrack
                                + options.disk.sector);
        size_t  total = (size_t) (NibblizationLayer::kTrackCount * NibblizationLayer::kSectorsPerTrack);

        fits = first + needed <= total;

        if (!fits)
        {
            size_t       spare  = total - first;
            snprintf (summary, sizeof (summary),
                      "Error: not enough sectors available\n"
                      "       Writing %zu bytes starting at track %d sector %d requires\n"
                      "       %zu %s, but only %zu %s %s on the disk.\n",
                      payload.size(), options.disk.track, options.disk.sector,
                      needed,
                      Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"),
                      spare,
                      Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                      (spare == 1) ? "remains" : "remain");
        }

        CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

        for (size_t index = 0; index < needed; index++)
        {
            size_t  at    = SectorRecordOffset (options.disk.numbering, first + index);
            size_t  from  = index * (size_t) NibblizationLayer::kSectorByteSize;
            size_t  count = std::min ((size_t) NibblizationLayer::kSectorByteSize,
                                      payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at track %d %s sector %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.track, how, options.disk.sector,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::SectorRecordOffset
//
//  Positions advance linearly -- sector, then track -- and each one maps
//  through the stated numbering on its own. Under --logical that is the
//  identity into the DOS-ordered buffer; under --physical it is the
//  interleave, answered by the layer that owns the skew rather than by a
//  second copy of the sixteen numbers here.
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskCommandRunner::SectorRecordOffset (
    CommandLineOptions::DiskOptions::Numbering  numbering,
    size_t                                      running)
{
    size_t  track    = running / (size_t) NibblizationLayer::kSectorsPerTrack;
    size_t  position = running % (size_t) NibblizationLayer::kSectorsPerTrack;
    bool    physical = numbering == CommandLineOptions::DiskOptions::Numbering::Physical;
    size_t  record   = running;



    if (physical)
    {
        record = track * (size_t) NibblizationLayer::kSectorsPerTrack
               + (size_t) NibblizationLayer::DosFileIndexForPhysicalSector ((int) position);
    }

    return record * (size_t) NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBlockRead
//
//  The 512-byte view: a ProDOS block is two sector records spread across its
//  track by the ProDOS interleave, and the map is ProDosSkeleton's -- the
//  same single copy the ProDOS reader, writer and builder address blocks
//  through.
//
//  A BLOCK NUMBER NEEDS NO --logical OR --physical, because blocks have only
//  one order: the block map is defined over the volume, so the question the
//  sector commands must ask has one answer here.
//
//  And it works over any container, not only .po. The session normalizes
//  every image into the same DOS-ordered buffer, so a block is a lens over
//  that buffer -- which is how a ProDOS volume shipped inside a .dsk reads
//  naturally.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBlockRead (const CommandLineOptions & options,
                                      DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes = (size_t) NibblizationLayer::kSectorByteSize;



    HRESULT                        hr           = S_OK;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    onDisk = options.disk.block >= 0 && options.disk.block < ProDosSkeleton::kTotalBlocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, ProDosSkeleton::kTotalBlocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal block count: %d\n"
                  "       The block count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, RefuseBadValue (result, summary));

    fits = options.disk.block + options.disk.count <= ProDosSkeleton::kTotalBlocks;

    if (!fits)
    {
        int  spare = ProDosSkeleton::kTotalBlocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Requested %d %s starting at block %d,\n"
                  "       but only %d %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"),
                  options.disk.block,
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    //  No filesystem needed, and none looked for: a block is geometry, not
    //  a volume, and blocks 0-1 of a disk with no filesystem are still worth
    //  looking at.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    for (int index = 0; index < options.disk.count; index++)
    {
        int  block = options.disk.block + index;

        for (int half = 0; half < 2; half++)
        {
            size_t  at = ProDosSkeleton::BlockByteOffset (block, (size_t) half * kHalfBytes);

            if (at + kHalfBytes > opened.sectors.size())
            {
                break;
            }

            payload.insert (payload.end(),
                            opened.sectors.begin() + (ptrdiff_t) at,
                            opened.sectors.begin() + (ptrdiff_t) (at + kHalfBytes));
        }
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from block %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.block,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, exactly as the sector read
    //  reports it: a block delivered as zeros looks like a block that holds
    //  zeros, and nothing behind these bytes records the difference.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBlockWrite
//
//  The write half of the 512-byte view, through the same single block map.
//
//  Whole blocks, the way the sector write works in whole sectors: a payload
//  that does not fill its last block leaves the rest of that block as it
//  was, rather than padded with a value this command invented. It runs on
//  into later blocks for the same reason the sector write runs on into
//  later tracks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBlockWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes  = (size_t) NibblizationLayer::kSectorByteSize;
    constexpr size_t  kBlockBytes = (size_t) ProDosSkeleton::kBlockByteSize;



    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           onDisk       = false;
    bool                           fits         = false;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, ReportMissingParameter ("<file>", result));

    onDisk = options.disk.block >= 0 && options.disk.block < ProDosSkeleton::kTotalBlocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, ProDosSkeleton::kTotalBlocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    edited = opened.sectors;

    needed = (payload.size() + kBlockBytes - 1) / kBlockBytes;

    fits = (size_t) options.disk.block + needed <= (size_t) ProDosSkeleton::kTotalBlocks;

    if (!fits)
    {
        int  spare = ProDosSkeleton::kTotalBlocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Writing %zu bytes starting at block %d requires\n"
                  "       %zu %s, but only %d %s %s on the disk.\n",
                  payload.size(), options.disk.block,
                  needed,
                  Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"),
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    for (size_t index = 0; index < needed; index++)
    {
        int  block = options.disk.block + (int) index;

        for (int half = 0; half < 2; half++)
        {
            size_t  from  = index * kBlockBytes + (size_t) half * kHalfBytes;
            size_t  at    = ProDosSkeleton::BlockByteOffset (block, (size_t) half * kHalfBytes);
            size_t  count = 0;

            if (from >= payload.size())
            {
                break;
            }

            count = std::min (kHalfBytes, payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at block %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.block,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}
