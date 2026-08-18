#include "Pch.h"

#include "DiskCommandRunner.h"
#include "AppleTextCodec.h"
#include "CommandLineParser.h"
#include "VolumeImage.h"
#include "Dos33Volume.h"
#include "ProDosVolume.h"
#include "NibblizationLayer.h"
#include "WozLoader.h"
#include "Core/TextEncoding.h"




//
//  The META keys worth leading with, each under a label of our own.
//
//  A WOZ image's metadata is a bag of key/value pairs in whatever order its
//  writer emitted them, and the interesting ones are not first: `image_date`
//  and `contributor` come before `title` on several of the images this was
//  measured against. So the ones that identify the SOFTWARE are pulled to the
//  front, and everything else follows in the order the file stores it --
//  nothing is dropped, because a key nobody anticipated is exactly the sort of
//  thing somebody is looking for.
//
static constexpr const char *  s_kppszMetaHighlights[][2] =
{
    { "title",            "title"     },
    { "subtitle",         "subtitle"  },
    { "publisher",        "publisher" },
    { "developer",        "developer" },
    { "copyright",        "copyright" },
    { "version",          "version"   },
    { "language",         "language"  },
    { "requires_machine", "machine"   },
    { "requires_ram",     "RAM"       },
    { "notes",            "notes"     },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DiskCommandRunner
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandRunner::DiskCommandRunner (IDiskFileIo & fileIo)
    : m_fileIo (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::LongPrefix
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::LongPrefix (char flagPrefix)
{
    return (flagPrefix == '/') ? std::string ("/") : std::string ("--");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildSubcommandHelp
//
//  One line per verb -- every spelling of it, then what it does -- and then the
//  grammar showing where the operands go.
//
//  EVERY ALIAS LEADS THE LINE IT BELONGS TO rather than trailing it in an "also
//  spelled" clause. A person coming from an Apple II reaches for CATALOG, one
//  coming from the host shell for DIR, one from a Unix shell for LS; all three
//  are accepted, and a reader scanning the left margin for the word they already
//  have in mind finds it there instead of at the end of a sentence about a word
//  they do not use.
//
//  THE DESCRIPTIONS CARRY THE DIRECTION, which is why nothing here explains that
//  put and get are named from the disk's point of view. "Read a file FROM the
//  disk" and "Write a host file TO the disk" leave nothing for that sentence to
//  add; it existed to rescue two verb names that the descriptions now state
//  outright.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildSubcommandHelp (char flagPrefix)
{
    std::string  lp = LongPrefix (flagPrefix);



    return
        "  cat | catalog | dir | list | ls   Show files on the disk -- name, type,\n"
        "                                    size, lock state, free space, and any\n"
        "                                    damage found\n"
        "  read | get                        Read a file from the disk, to standard\n"
        "                                    output or to " + lp + "out <file>\n"
        "  write | put                       Write a host file to the disk\n"
        "  del | delete | rm                 Delete a file from the disk and return\n"
        "                                    the space it alone claimed\n"
        "  boot                              Set the program that runs when the disk\n"
        "                                    is booted\n"
        "\n"
        "  CassoCli disk " + lp + "help\n"
        "  CassoCli disk list   <image> [" + lp + "long]\n"
        "  CassoCli disk get    <image> <path> [" + lp + "out <file>]"
                 " [" + lp + "text | " + lp + "basic | " + lp + "verbatim]\n"
        "  CassoCli disk put    <image> <file> [" + lp + "as <path>] ["
                 + lp + "type <t>] [" + lp + "addr $XXXX]\n"
        "                                      [" + lp + "text | " + lp + "basic | "
                 + lp + "verbatim]\n"
        "  CassoCli disk delete <image> <path>\n"
        "  CassoCli disk boot   <image> <path>\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildOptionsHelp
//
//  The disk options, each beside the verb that uses it, and then the three
//  things no single option row can state: what --basic promises, what put's
//  three naming options default to, and what boot will refuse.
//
//  THE --basic PARAGRAPH FOLLOWS ITS OWN FLAG rather than closing the section.
//  It used to sit under the exit statuses, two paragraphs below the row it
//  explains, which is where a reader who has just read `Convert to and from an
//  Applesoft listing` has already stopped looking. `basic` is deliberately the
//  last row of the table so the two are adjacent.
//
//  EXIT STATUSES ARE NOT HERE ANY MORE. They were never disk-specific -- the
//  assembler and `run` return the same three -- and stating them under one
//  subcommand implied the other two had their own. They are stated once, in the
//  overview, from CommandLineParser::kExitStatusHelpText.
//
//  NEITHER IS THE IN-USE PROBE. A locked image is refused by name where it
//  happens ("is open in another program -- close it and try again"), which is
//  the report a user acts on; a paragraph restating that in the help was
//  documentation of an error message.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildOptionsHelp (char flagPrefix)
{
    //  Flag, then what it does. Padded to a fixed description column below
    //  rather than written into each literal, because `/long` is one character
    //  narrower than `--long` and a table spaced for one prefix comes out
    //  ragged in the other.
    static constexpr const char *  kppszOptions[][2] =
    {
        { "long",       "Add the ProDOS columns -- eof= and aux= -- to a listing.\n"
                        "                         A DOS 3.3 volume records neither, and ignores it" },
        { "out <file>", "Write an extracted file here, not to standard output" },
        { "as <path>",  "Name the placed file this on the disk" },
        { "type <t>",   "File type. DOS 3.3 takes T, I, A, B or R;\n"
                        "                         ProDOS takes TXT, BIN, BAS or SYS" },
        { "addr $XXXX", "Load address for a placed binary" },
        { "verbatim",   "Place bytes unchanged (the default)" },
        { "text",       "Convert the high-bit encoding and the line endings" },
        { "basic",      "Convert to and from an Applesoft listing" },
    };

    const size_t  kDescriptionColumn = 25;
    std::string   lp                 = LongPrefix (flagPrefix);
    std::string   text;



    for (const auto & entry : kppszOptions)
    {
        std::string  line = "  " + lp + entry[0];

        while (line.size() < kDescriptionColumn)
        {
            line += " ";
        }

        text += line + entry[1] + "\n";
    }

    text += "\n  ";
    text += ApplesoftTokenizer::RoundTripHelpText (flagPrefix);
    text += "\n";

    text +=
        "\n"
        "  write | put: " + lp + "as is the name the file takes on the disk, and defaults\n"
        "  to the host file's own name. " + lp + "type is what the catalog records, and\n"
        "  defaults to a binary -- or to Applesoft under " + lp + "basic, which is the only\n"
        "  type the guest will RUN. " + lp + "addr is the load address a binary carries: a\n"
        "  DOS 3.3 B or a ProDOS BIN is refused without one, every other type ignores\n"
        "  it, and " + lp + "basic refuses it outright because Applesoft keeps its program\n"
        "  at $0801 and nowhere else.\n"
        "\n"
        "  boot: the program has to be on the volume already, named as the catalog\n"
        "  spells it, and the image has to carry an operating system on the tracks a\n"
        "  boot reads. On ProDOS it must be a file of type SYS, and not the kernel\n"
        "  itself. On DOS 3.3 the boot command is RUN, so an Applesoft or Integer\n"
        "  program runs -- anything else is set, reported, and the disk boots without\n"
        "  running it.\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildExampleHelp
//
//  THE EXAMPLE IS THE POINT. A flag reference tells a reader what each switch
//  spells and leaves them to guess the order and the combination, and the two
//  traps below are exactly what they would guess wrong. So the example runs the
//  loop end to end and then says why two of its steps look the way they do.
//
//  The first trap is the header. `--dos-bin` and `put --addr` each write a DOS
//  3.3 four-byte header, and a file carrying both loads its own header at the
//  load address -- where a `BRUN` executes it. The stale header's first byte is
//  the low half of the load address, which for anything in page $60 and below is
//  a BRK, so the machine lands in the monitor with no clue as to why.
//
//  The second is the greeting. A booting DOS 3.3 RUNs the name in its greeting
//  field, which runs an Applesoft or Integer program, so naming a binary there
//  sets the name and boots without running it. The example places a one-line
//  greeting that BRUNs the binary, which is what actually closes the loop.
//
//  It goes LAST in the usage text, after every option group, because it is the
//  part a reader returns to once they know what the flags are -- and because a
//  worked loop sitting between two flag tables interrupts both.
//
//  ONE FLAG HERE KEEPS THE `--` SPELLING WHATEVER THE READER ASKED FOR, and it
//  is the one belonging to a different program. `--disk1` is the emulator's
//  flag, not this tool's, and the emulator accepts only that spelling; printing
//  `/disk1` because the reader typed `/?` would be a promise this executable is
//  not the one keeping.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildExampleHelp (char flagPrefix)
{
    std::string  lp = LongPrefix (flagPrefix);
    std::string  sp = (flagPrefix == '/') ? std::string ("/") : std::string ("-");



    return std::string (kExampleHeading) + "\n"
        "  CassoCli prog.a65 " + sp + "o prog.bin\n"
        "  CassoCli disk put mydisk.dsk prog.bin " + lp + "as PROG "
                 + lp + "type B " + lp + "addr $6000\n"
        "  CassoCli disk put mydisk.dsk greet.bas " + lp + "as STARTUP " + lp + "basic\n"
        "  CassoCli disk boot mydisk.dsk STARTUP\n"
        "  Casso.exe --disk1 mydisk.dsk\n"
        "\n"
        "  " + sp + "o names the assembled output file; --disk1 mounts an image in\n"
        "  drive 1 as the emulator starts -- and is the emulator's own flag, which\n"
        "  is why it keeps the -- spelling here.\n"
        "  Assemble with the default shape rather than " + lp + "dos-bin: put writes the\n"
        "  DOS 3.3 header itself from " + lp + "addr, and a file that already carries one\n"
        "  has its own header loaded as code where the program should begin.\n"
        "  greet.bas holds one Applesoft line -- 10 PRINT CHR$(4);\"BRUN PROG\"\n"
        "  -- because a booting DOS 3.3 volume RUNs its greeting. Naming the\n"
        "  binary there sets the name and the disk boots without running it.\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildHelpText
//
//  The three pieces in the order a reader meets them when nothing else is
//  interleaved. What the executable prints is the same text with the assembler
//  and run options between the second and third; this is what a test reads, and
//  what a caller with no usage text of its own would print.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildHelpText (char flagPrefix)
{
    return BuildSubcommandHelp (flagPrefix) + "\n"
         + BuildOptionsHelp    (flagPrefix) + "\n"
         + BuildExampleHelp    (flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeAcceptedVerbs
//
//  The verb table read out in the order it is written, which is each verb
//  followed by its own aliases -- so the suggestion a user is offered groups
//  the way the help does rather than alphabetically, where `cat` would land
//  three words from `catalog`.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeAcceptedVerbs()
{
    std::string  text;



    for (const auto & verb : CommandLineParser::GetAllDiskVerbs())
    {
        if (!text.empty())
        {
            text += ", ";
        }

        text += verb.name;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Failure
//
//  Image, file, reason, in that order. A script's user sees the first line and
//  needs to know which disk before anything else; the reason is useless without
//  it when a build places twenty files.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::Failure (
    const std::string & imagePath,
    const std::string & fileName,
    const std::string & reason)
{
    std::string  text = imagePath.empty() ? std::string ("(no image)") : imagePath;



    if (!fileName.empty())
    {
        text += ": " + fileName;
    }

    text += ": " + reason;

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DetailLine
//
//  A value nobody recorded produces NOTHING, rather than a label followed by
//  empty space. That is what lets the callers below offer every field they know
//  how to read without also deciding, field by field, whether this particular
//  image answered -- and it keeps a listing of a sparse image from being mostly
//  blank labels.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DetailLine (const char * label, const std::string & value)
{
    const size_t  kLabelColumn = 16;
    std::string   text;



    if (value.empty())
    {
        return text;
    }

    text = std::string ("  ") + label;

    while (text.size() < kLabelColumn)
    {
        text += " ";
    }

    return text + value + "\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeWozChunks
//
//  What a WOZ image records about itself: who imaged it, what it is, how much
//  of the surface carries data, and -- for a commercially pressed disk -- its
//  title and publisher.
//
//  THE QUARTER-TRACK COUNT IS REAL INFORMATION AND NOT TRIVIA. The head steps
//  in quarter tracks; a disk written on half or quarter tracks is a copy
//  protection, deliberately unreadable by a drive that only visits whole ones.
//  When the positions carrying data outnumber the track slots behind them, the
//  image is telling the reader why an ordinary catalog was never going to work.
//
//  META arrives as UTF-8, which the format specifies, and is converted to the
//  code page the rest of these strings are in -- so the diagnostic reaching the
//  output boundary is in ONE encoding rather than a mixture the boundary would
//  then have to guess about.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeWozChunks (const std::vector<Byte> & fileBytes)
{
    WozLoader::Description  woz;
    std::string             text;
    std::string             media;
    char                    note[160] = {};



    WozLoader::Describe (fileBytes, woz);

    if (!woz.isWoz)
    {
        return text;
    }

    snprintf (note, sizeof (note), "WOZ %d bit-stream image, INFO version %d",
              woz.wozVersion, woz.infoVersion);

    text += DetailLine ("format",  note);
    text += DetailLine ("creator", woz.creator);

    media = (woz.diskType == WozLoader::kDiskType525) ? "5.25-inch disk"
          : (woz.diskType == WozLoader::kDiskType35)  ? "3.5-inch disk"
                                                      : "disk of an unrecorded size";

    if (woz.writeProtected) { media += ", write-protected"; }
    if (woz.synchronized)   { media += ", tracks synchronized to each other"; }
    if (woz.cleaned)        { media += ", cleaned of drive noise"; }

    text += DetailLine ("media", media);

    if (woz.hasBootSectorFormat)
    {
        const char *  boot =
            (woz.bootSectorFormat == WozLoader::kBootSector16)   ? "16-sector"
          : (woz.bootSectorFormat == WozLoader::kBootSector13)   ? "13-sector"
          : (woz.bootSectorFormat == WozLoader::kBootSectorBoth) ? "both 13- and 16-sector"
                                                                 : "not recorded";

        text += DetailLine ("boots as", boot);
    }

    snprintf (note, sizeof (note),
              "%d track positions carry data, reached at %d of the 160 quarter-track "
              "stops the head can make",
              woz.trackSlotsWithData, woz.quarterTracksWithData);

    text += DetailLine ("surface", note);

    // The named fields first, then whatever else the image chose to record.
    for (const auto & highlight : s_kppszMetaHighlights)
    {
        for (const WozLoader::MetaField & field : woz.meta)
        {
            if (field.key == highlight[0])
            {
                text += DetailLine (highlight[1], TextEncoding::Utf8ToNarrow (field.value));
                break;
            }
        }
    }

    for (const WozLoader::MetaField & field : woz.meta)
    {
        bool  highlighted = false;

        for (const auto & highlight : s_kppszMetaHighlights)
        {
            if (field.key == highlight[0])
            {
                highlighted = true;
                break;
            }
        }

        if (!highlighted)
        {
            std::string  label = field.key;

            for (char & c : label)
            {
                c = (c == '_') ? ' ' : c;
            }

            text += DetailLine (label.c_str(), TextEncoding::Utf8ToNarrow (field.value));
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeSurface
//
//  What the decoded sectors show, which is a different question from what the
//  container claims.
//
//  "BOOTABLE, NO FILESYSTEM" IS THE TRUE ANSWER FOR A GREAT MANY DISKS and is
//  the one worth saying out loud. Track 0 sector 0 is what the drive's ROM
//  reads and jumps into; a disk whose first sector carries code boots and runs,
//  whatever it does about files afterwards. This project's own demo disk is
//  exactly that, and reporting only "no filesystem" describes it as though it
//  were broken.
//
//  The per-track decode outcome comes from the read that already happened, so
//  it costs nothing and says the thing a copy-protected disk most wants
//  understood: the tracks are there, they simply are not standard sectors.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeSurface (const OpenedImage & opened)
{
    int          complete    = 0;
    int          partial     = 0;
    int          unformatted = 0;
    int          trackCount  = opened.report.GetTrackCount();
    int          track       = 0;
    size_t       i           = 0;
    bool         bitStream   = trackCount > 0;
    bool         trackZeroOk = true;
    bool         bootCode    = false;
    std::string  text;
    char         note[512]   = {};



    // Geometry belongs to a SECTOR-ORDER file and to nothing else. Printing it
    // for a bit-stream image would describe the buffer this tool decoded into
    // rather than the disk, and for a disk that decoded into almost none of it
    // that is an actively misleading thing to say.
    if (!bitStream)
    {
        snprintf (note, sizeof (note), "%d tracks x %d sectors x %d bytes = %d bytes",
                  NibblizationLayer::kTrackCount,
                  NibblizationLayer::kSectorsPerTrack,
                  NibblizationLayer::kSectorByteSize,
                  NibblizationLayer::kImageByteSize);

        text += DetailLine ("geometry", note);
    }

    for (track = 0; track < trackCount; track++)
    {
        switch (opened.report.GetOutcome (track))
        {
            case TrackDecodeOutcome::Complete:    complete++;    break;
            case TrackDecodeOutcome::Partial:     partial++;     break;
            case TrackDecodeOutcome::Unformatted: unformatted++; break;
            default:                                             break;
        }
    }

    if (bitStream)
    {
        trackZeroOk = opened.report.GetOutcome (0) == TrackDecodeOutcome::Complete;

        // NO CAUSE IS ASSERTED, only the count. A track that will not decode
        // as standard sectors may be protected, may be a format this tool does
        // not read, or may be damaged, and nothing available here separates the
        // three. What IS worth saying is that the first of those is ordinary,
        // so a reader does not conclude their disk is broken.
        snprintf (note, sizeof (note),
                  "of %d tracks, %d read as standard 16-sector data, %d only partly,\n"
                  "                and %d had no standard address fields at all%s",
                  trackCount, complete, partial, unformatted,
                  complete == 0 ? ".\n                A disk that boots and runs can still"
                                  " read this way: most\n                protected software"
                                  " wrote a track format of its own"
                                : "");

        text += DetailLine ("decoded", note);
    }

    // ZEROS IN THE BUFFER MEAN TWO DIFFERENT THINGS and only one of them is
    // "blank". A track that decoded cleanly and holds zeros really is empty; a
    // track that never decoded reads back as zeros too, and calling that one
    // blank would tell somebody their bootable disk does not boot.
    if (!trackZeroOk)
    {
        return text + DetailLine ("boot sector",
            "track 0 did not decode as standard sectors, so what it holds\n"
            "                cannot be judged from here");
    }

    for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize && i < opened.sectors.size(); i++)
    {
        if (opened.sectors[i] != 0)
        {
            bootCode = true;
            break;
        }
    }

    text += DetailLine ("boot sector",
                        bootCode
                            ? "track 0 sector 0 carries code -- the drive's ROM reads\n"
                              "                that sector and jumps into it, so this image"
                              " boots something.\n                It simply keeps its files"
                              " somewhere this tool does not read"
                            : "track 0 sector 0 is blank -- nothing here would boot");

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeUnrecognizedImage
//
//  What is left to say once neither filesystem is there, which on real disks is
//  a great deal.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeUnrecognizedImage (const OpenedImage & opened)
{
    return DescribeWozChunks (opened.fileBytes) + DescribeSurface (opened);
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
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatProDosEntry (const FileEntry & entry, bool longForm)
{
    char         detail[64] = {};
    std::string  text;



    text = std::string (entry.isLocked ? "*" : " ") + entry.name;

    while (text.size() < 18)
    {
        text += " ";
    }

    snprintf (detail, sizeof (detail), "%s $%02X %5u",
              entry.isDirectory ? "DIR" : "   ",
              (unsigned) entry.type,
              (unsigned) entry.sizeUnits);

    text += detail;

    if (longForm)
    {
        char  more[64] = {};

        snprintf (more, sizeof (more), "  eof=%u aux=$%04X",
                  (unsigned) entry.eofBytes, (unsigned) entry.auxType);

        text += more;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::OpenImage
//
//  Reads the image, normalizes its sector order, and identifies the filesystem.
//  Each failure explains itself rather than collapsing into one message,
//  because "cannot read the file" and "read it but do not recognize it" send
//  the user somewhere different.
//
//  IT ALSO RECORDS THE SIZE AND MODIFICATION TIME, HERE AND NOWHERE ELSE. The
//  staleness check compares what the file looked like when its contents were
//  taken against what it looks like at the moment of commit; a stamp taken any
//  later than this agrees with itself and closes no window at all.
//
//  A stamp the platform declines to give is recorded as ABSENT rather than as
//  zero, so a commit can refuse instead of silently comparing two zeros and
//  concluding nothing changed. Reading does not need it and is unaffected.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::OpenImage (
    const std::string  & imagePath,
    OpenedImage        & outOpened,
    DiskCommandResult  & result)
{
    HRESULT       hr      = S_OK;
    bool          named   = !imagePath.empty();
    HRESULT       statHr  = S_OK;
    vector<Byte>  fileBytes;



    outOpened.imagePath = imagePath;

    if (!named)
    {
        result.diagnostics += "no disk image named\n";
        result.exitStatus   = kNoOutput;
        return E_INVALIDARG;
    }

    hr = m_fileIo.ReadAllBytes (imagePath, fileBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (imagePath, "", "cannot be read") + "\n";
        result.exitStatus   = kNoOutput;
        return hr;
    }

    statHr                    = m_fileIo.Stat (imagePath, outOpened.stamp);
    outOpened.stampRecorded   = SUCCEEDED (statHr);
    outOpened.fileBytes       = fileBytes;

    hr = VolumeImage::Load (fileBytes, imagePath, outOpened.sectors, outOpened.report);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (imagePath, "",
            "is not a disk image this tool can read") + "\n";
        result.exitStatus   = kNoOutput;
        return hr;
    }

    outOpened.kind = VolumeImage::DetectFilesystem (outOpened.sectors);

    if (outOpened.kind == VolumeKind::Unknown)
    {
        // THE STATUS AND THE STREAM BOTH STAY WHAT THEY WERE. A caller still
        // got no catalog, so this is still status 2 and still goes to the error
        // stream -- a script that pipes a listing must not suddenly find a
        // survey in the pipe. What changed is only how much the message says.
        result.diagnostics += Failure (imagePath, "", kNoFilesystemText) + "\n";
        result.diagnostics += DescribeUnrecognizedImage (outOpened);
        result.exitStatus   = kNoOutput;
        return E_FAIL;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RefuseCommit
//
//  One place that turns a refused commit into what the user sees, so a path
//  cannot report the reason without also setting the status, or the other way
//  round.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RefuseCommit (
    const std::string  & imagePath,
    const std::string  & reason,
    DiskCommandResult  & result)
{
    result.diagnostics += Failure (imagePath, "", reason) + "\n";
    result.exitStatus   = kNoOutput;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeListingRefusal
//
//  WHICH LINE, AND THE LINE ITSELF. A number on its own is useless against a
//  file numbered by tens, and useless in the other direction too: a program
//  read off a disk has line numbers the host file does not have. So the number
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



    message += " -- ";

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
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        return "is locked on this volume -- unlock it on the disk before writing over it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DISK_FULL))
    {
        return "does not fit: the volume has no room left, either for the file's "
               "contents or for another catalog entry";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME))
    {
        return "is not a name this filesystem can store -- it must be a single "
               "component starting with a letter, short enough for the catalog, "
               "and free of commas and control characters";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
    {
        return "is not on this volume";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE))
    {
        return "is larger than this filesystem can record for one file";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER))
    {
        return "is a binary, which has to be told where it loads -- give --addr $XXXX";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED))
    {
        return "is a directory, and this tool does not go inside one -- so removing "
               "it would strand everything beneath it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF))
    {
        return "has a sector chain that cannot be followed to its end";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED))
    {
        return "cannot be made this volume's startup program: the image carries no "
               "operating system on the tracks a boot reads, so nothing would run it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE))
    {
        return "is not a program this volume's boot path launches -- on ProDOS that "
               "means a file of type SYS, and not the kernel itself";
    }

    return "was refused by the filesystem on this volume";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeReplaceFailure
//
//  WRITE PROTECTION ARRIVES HERE AND NOWHERE ELSE when it comes from the host
//  file's read-only attribute. Nothing about the image's contents says it may
//  not be written, so the volume layer computes a perfectly good new image and
//  the platform refuses at the last step. Reporting that as a generic failure
//  would leave the user with a refusal and no idea which of the two things to
//  fix.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeReplaceFailure (HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        return "is write-protected -- clear its read-only attribute and try again. "
               "Nothing was written";
    }

    return "could not be replaced -- it may be read-only or in use";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::CommitImage
//
//  The whole of putting a computed image where the old one was, in the order
//  the guarantees require: refuse while somebody else has it, refuse if it
//  moved under us, write the new bytes somewhere they cannot be mistaken for
//  the image, and only then let them become the image in one step.
//
//  NOTHING HERE MAY LEAVE THE TARGET PART-WRITTEN, which is why the new bytes
//  never go near it. They go to a temporary beside it and arrive by an atomic
//  replace, so an interruption at any point leaves either the old image or the
//  new one, and never a mixture.
//
//  THE CLEANUP IS DRIVEN BY THE PLAN, NOT BY EACH FAILURE PATH. A removal
//  written beside each bail-out is a removal the next bail-out forgets: the
//  ordering rule is asked once, at the single exit, from a record of how far
//  the sequence actually got. That record tracks the furthest step ATTEMPTED,
//  because a write that failed halfway is exactly the case with a partial file
//  sitting there and exactly the case a reader assumes is clean.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::CommitImage (
    const OpenedImage   & opened,
    const vector<Byte>  & newImageBytes,
    DiskCommandResult   & result)
{
    HRESULT               hr            = S_OK;
    HRESULT               removeHr      = S_OK;
    bool                  held          = false;
    bool                  stale         = false;
    bool                  foundFreeName = false;
    bool                  cleanUp       = false;
    unsigned              attempt       = 0;
    CommitPlan::Progress  progress;
    FileStamp             observed;
    std::string           tempPath;



    // The probe comes first because it is the only check that costs nothing to
    // be wrong about: refusing before anything exists leaves no cleanup to get
    // right. It catches another TOOL holding the file and cannot catch this
    // emulator, which keeps no handle on a mounted image.
    progress.furthestAttempted = CommitPlan::Step::Probe;

    held = m_fileIo.IsHeldByAnotherProcess (opened.imagePath);
    CBRFEx (!held, HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION),
            RefuseCommit (opened.imagePath, kInUseRefusalText, result));

    progress.furthestAttempted = CommitPlan::Step::Reverify;

    CBRFEx (opened.stampRecorded, HRESULT_FROM_WIN32 (ERROR_CANT_ACCESS_FILE),
            RefuseCommit (opened.imagePath,
                          "could not be checked for changes when it was read, so it will "
                          "not be written over", result));

    hr = m_fileIo.Stat (opened.imagePath, observed);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "has gone away since it was read", result));

    stale = CommitPlan::IsStale (opened.stamp.sizeBytes, opened.stamp.modifiedUnix,
                                 observed.sizeBytes,     observed.modifiedUnix);

    // STG_E_NOTCURRENT says exactly this and nothing else -- the object changed
    // since it was last read. The Win32 table has no code for it, and inventing
    // a near-miss from that table would read as a different problem in a log.
    CBRFEx (!stale, STG_E_NOTCURRENT,
            RefuseCommit (opened.imagePath,
                          "changed since it was read -- nothing was written, read it "
                          "again and retry", result));

    // Step over anything already sitting at the name we would take. This is
    // the abandoned-temporary case; the invocation tag inside the name is what
    // handles two live invocations, since both of those would otherwise find
    // attempt zero free at the same instant.
    for (attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
    {
        tempPath      = CommitPlan::TemporaryPathFor (opened.imagePath, m_invocationTag, attempt);
        foundFreeName = !m_fileIo.Exists (tempPath);

        if (foundFreeName)
        {
            break;
        }
    }

    CBRFEx (foundFreeName, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS),
            RefuseCommit (opened.imagePath,
                          "already has that many temporary files beside it -- remove them "
                          "and try again", result));

    progress.furthestAttempted = CommitPlan::Step::WriteTemporary;

    hr = m_fileIo.WriteAllBytes (tempPath, newImageBytes);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "could not be written beside -- the folder may be read-only "
                            "or full", result));

    progress.furthestAttempted = CommitPlan::Step::Replace;

    hr = m_fileIo.ReplaceAtomically (tempPath, opened.imagePath);
    CHRF (hr, RefuseCommit (opened.imagePath, DescribeReplaceFailure (hr), result));

    progress.replaceSucceeded = true;

Error:
    cleanUp = CommitPlan::ShouldRemoveTemporary (progress);

    if (cleanUp)
    {
        removeHr = m_fileIo.Remove (tempPath);
        IGNORE_RETURN_VALUE (removeHr, S_OK);
    }

    return hr;
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
    HRESULT             hr           = S_OK;
    OpenedImage         opened;
    VolumeListing       listing;
    char                summary[128] = {};



    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Enumerate (listing);

        if (FAILED (hr))
        {
            result.diagnostics += Failure (options.disk.imagePath, "",
                "catalog could not be read") + "\n";
            result.exitStatus   = kNoOutput;
            BAIL_OUT_IF (true, hr);
        }

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
                           : FormatProDosEntry (entry, options.disk.longListing);

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
        result.diagnostics += Failure (options.disk.imagePath, "",
            note + " -- THIS LISTING IS INCOMPLETE, entries may be missing") + "\n";
        result.exitStatus   = kWithComplaints;
    }

    if (opened.report.HasDataLoss())
    {
        snprintf (summary, sizeof (summary),
                  "%d sector(s) could not be decoded and read back as zeros "
                  "-- THIS LISTING IS INCOMPLETE, entries may be missing",
                  opened.report.GetUnrecoveredCount());

        result.diagnostics += Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = kWithComplaints;
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
            // ignored, high-ASCII and plain-ASCII text differ in nothing -- so
            // this choice is inert on the read path. It becomes load-bearing
            // when writing, which is where the terminator gets chosen.
            //
            // High ASCII is the measured answer on both filesystems: the ProDOS
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
                    "--basic cannot read this file as an Applesoft program", listingError);

                result.exitStatus   = kNoOutput;
                break;
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics += "unknown encoding -- try: --text, --basic, --verbatim\n";
            result.exitStatus   = kNoOutput;
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
    HRESULT             hr        = S_OK;
    bool                named     = !options.disk.path.empty();
    OpenedImage         opened;
    FilePayload         payload;
    FilePath            path;
    char                note[128] = {};



    if (!named)
    {
        result.diagnostics += "no file named to extract\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Read (path, payload);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
            "could not be read from this volume") + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = ApplyEncoding (options, payload, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload.bytes);

        if (FAILED (hr))
        {
            result.diagnostics += Failure (options.disk.hostFile, "",
                "could not be written") + "\n";
            result.exitStatus   = kNoOutput;
            BAIL_OUT_IF (true, hr);
        }
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
        snprintf (note, sizeof (note),
                  "%d sector(s) could not be decoded -- THIS FILE IS INCOMPLETE, "
                  "unreadable sectors were delivered as zeros",
                  opened.report.GetUnrecoveredCount());

        result.diagnostics += Failure (options.disk.imagePath, options.disk.path, note) + "\n";
        result.exitStatus   = kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::OnDiskNameFor
//
//  --as when the caller gave one, and otherwise the host file's own last
//  component. Nothing is stripped or shortened on the way: the caller already
//  chose that name, and inventing a different one would leave them looking for
//  a file the catalog does not hold. A name the filesystem cannot store is
//  refused by the volume layer, which is where that rule belongs.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::OnDiskNameFor (const CommandLineOptions & options)
{
    size_t  lastSeparator = options.disk.hostFile.find_last_of ("/\\");



    if (!options.disk.path.empty())
    {
        return options.disk.path;
    }

    if (lastSeparator == std::string::npos)
    {
        return options.disk.hostFile;
    }

    return options.disk.hostFile.substr (lastSeparator + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveFileType
//
//  The two filesystems number their types differently and name them
//  differently, so the spellings accepted here are each one's own. A letter
//  that means Applesoft on DOS 3.3 and nothing on ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  A caller who named no type gets the one that matches the conversion they
//  asked for -- text when they asked for text, a binary otherwise, which is
//  what a build loop places.
//
//  AN UNRECOGNIZED SPELLING IS REFUSED, not defaulted. Defaulting would place
//  the file under a type the caller did not ask for and say nothing, and the
//  guest would report the mismatch much later as a file that will not load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveFileType (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
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
    std::string  spelling   = options.disk.typeName;



    if (isBasic)
    {
        // A tokenized program stored under any other type is a file the guest
        // will not RUN, so the conversion picks the type rather than leaving it
        // to a default meant for a build loop's binaries.
        outType = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
    }
    else
    {
        outType = isDos ? (isText ? Dos33Volume::kTypeText  : Dos33Volume::kTypeBinary)
                        : (isText ? ProDosVolume::kTypeText : ProDosVolume::kTypeBinary);
    }

    BAIL_OUT_IF (!named, S_OK);

    for (i = 0; i < spelling.size(); i++)
    {
        if (spelling[i] >= 'a' && spelling[i] <= 'z')
        {
            spelling[i] = (char) (spelling[i] - 'a' + 'A');
        }
    }

    if (isDos)
    {
        if      (spelling == "T" || spelling == "TXT") { outType = Dos33Volume::kTypeText; }
        else if (spelling == "I" || spelling == "INT") { outType = Dos33Volume::kTypeInteger; }
        else if (spelling == "A" || spelling == "BAS") { outType = Dos33Volume::kTypeApplesoft; }
        else if (spelling == "B" || spelling == "BIN") { outType = Dos33Volume::kTypeBinary; }
        else if (spelling == "R" || spelling == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                           { recognized = false; }
    }
    else
    {
        if      (spelling == "T" || spelling == "TXT") { outType = ProDosVolume::kTypeText; }
        else if (spelling == "B" || spelling == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (spelling == "A" || spelling == "BAS") { outType = ProDosVolume::kTypeBasic; }
        else if (spelling == "S" || spelling == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                           { recognized = false; }
    }

    if (!recognized)
    {
        result.diagnostics += "--type " + options.disk.typeName + " means nothing on this volume -- "
                            + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                                     : "ProDOS takes TXT, BIN, BAS or SYS")
                            + "\n";

        result.exitStatus   = kNoOutput;
        hr                  = E_NOTIMPL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildPutPayload
//
//  What the host file becomes on the disk.
//
//  VERBATIM IS THE ABSENCE OF A CHARACTER CONVERSION AND NOTHING MORE. The
//  bytes go down as they came in; the length and whatever header the type
//  carries are still applied below this, because those record where the file
//  ENDS and are its identity rather than a transformation of it. That is what
//  makes extract-edit-replace safe: the bytes the caller did not touch come
//  back exactly as they were.
//
//  Text is written in the high-ASCII convention on BOTH filesystems, which is
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



    hr = ResolveFileType (options, kind, type, result);
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
                          "byte %u of the host file has no Apple II text representation, "
                          "so nothing was converted\n",
                          (unsigned) badOffset);

                result.diagnostics += note;
                result.exitStatus   = kNoOutput;
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
                result.diagnostics += "--addr means nothing with --basic: "
                                      "an Applesoft program always loads at $0801\n";
                result.exitStatus   = kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER);
                break;
            }

            hostText.assign (hostBytes.begin(), hostBytes.end());

            hr = ApplesoftTokenizer::Tokenize (hostText, outPayload.bytes, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DescribeListingRefusal (
                    "--basic cannot make an Applesoft program of this listing", listingError);

                result.exitStatus   = kNoOutput;
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
            result.diagnostics += "unknown encoding -- try: --text, --basic, --verbatim\n";
            result.exitStatus   = kNoOutput;
            hr                  = E_INVALIDARG;
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::SaveAndCommit
//
//  The second half of every edit: render the new sector buffer back into the
//  container it came from, then put it where the old one was.
//
//  THE ORDER IS THE GUARANTEE. Rendering can still refuse -- a bit-stream image
//  whose edit lands on a track that cannot be re-encoded as standard sectors is
//  refused whole, never track by track -- and a refusal at that point has
//  touched nothing, because the commit has not begun. Committing first and
//  rendering afterwards is not a rearrangement of this; it is the half-written
//  image the all-or-nothing rule exists to forbid.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::SaveAndCommit (
    const OpenedImage   & opened,
    const vector<Byte>  & editedSectors,
    DiskCommandResult   & result)
{
    HRESULT       hr = S_OK;
    std::string   refusal;
    std::string   reason;
    vector<Byte>  newFileBytes;



    hr     = VolumeImage::Save (opened.fileBytes, opened.imagePath, editedSectors,
                                newFileBytes, refusal);

    reason = refusal.empty()
           ? std::string ("could not be written back in the format it came from")
           : refusal;

    CHRF (hr, RefuseCommit (opened.imagePath, reason, result));

    hr = CommitImage (opened, newFileBytes, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunPut
//
//  Placing a host file on the disk: read it, convert it if asked, let the
//  filesystem compute the whole new volume, render that back into the container
//  and commit it.
//
//  NOTHING IS WRITTEN UNTIL EVERY DECISION HAS BEEN MADE. Each refusal below --
//  no such host file, a type nobody recognizes, a name the catalog cannot
//  store, a locked file, a volume with no room -- happens with the image on
//  disk untouched, because the only thing this path can commit is a finished
//  buffer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunPut (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT       hr       = S_OK;
    bool          named    = !options.disk.hostFile.empty();
    std::string   diskName = OnDiskNameFor (options);
    OpenedImage   opened;
    FilePayload   payload;
    FilePath      path;
    vector<Byte>  hostBytes;
    vector<Byte>  edited;



    if (!named)
    {
        result.diagnostics += "no host file named to place -- put takes the file to "
                              "copy onto the disk\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, hostBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.hostFile, "", "cannot be read") + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = BuildPutPayload (options, opened.kind, hostBytes, payload, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (diskName);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Write (path, payload, edited);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, diskName,
                                       DescribeVolumeRefusal (hr)) + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = SaveAndCommit (opened, edited, result);
    BAIL_OUT_IF (FAILED (hr), hr);

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
    HRESULT        hr    = S_OK;
    bool           named = !options.disk.path.empty();
    OpenedImage    opened;
    FilePath       path;
    DeleteOutcome  outcome;
    vector<Byte>   edited;



    if (!named)
    {
        result.diagnostics += "no file named to delete\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Delete (path, edited, outcome);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
                                       DescribeVolumeRefusal (hr)) + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = SaveAndCommit (opened, edited, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path, warning) + "\n";
        result.exitStatus   = kWithComplaints;
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
    HRESULT        hr       = S_OK;
    HRESULT        listHr   = S_OK;
    bool           named    = !options.disk.path.empty();
    bool           runnable = true;
    OpenedImage    opened;
    FilePath       path;
    VolumeListing  listing;
    vector<Byte>   edited;



    if (!named)
    {
        result.diagnostics += "no program named to boot -- boot takes the file on the "
                              "disk to run after the operating system loads\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

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

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
                                       DescribeVolumeRefusal (hr)) + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = SaveAndCommit (opened, edited, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    if (!runnable)
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
            "is set as the startup program, but a booting DOS 3.3 RUNs its greeting -- "
            "which runs an Applesoft or Integer program. This file is neither, so the "
            "disk will boot without running it") + "\n";

        result.exitStatus   = kWithComplaints;
    }

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
    for (const FileEntry & entry : listing.entries)
    {
        if (_stricmp (entry.name.c_str(), name.c_str()) == 0)
        {
            return entry.type == Dos33Volume::kTypeApplesoft
                || entry.type == Dos33Volume::kTypeInteger;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Run
//
//  A verb this build does not implement reports failure rather than doing
//  nothing quietly, so an absent capability cannot be mistaken for a completed
//  operation.
//
//  A HELP REQUEST IS A VERB HERE, and it is answered on the output stream with
//  a clean status. It used to be recognized only as the FIRST argument of the
//  whole command line, so `disk --help` offered `--help` to the verb table and
//  answered a request for the grammar by refusing to run and complaining about
//  the grammar -- exit 2 for a question the tool knows the answer to.
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandResult DiskCommandRunner::Run (const CommandLineOptions & options)
{
    DiskCommandResult  result;



    switch (options.disk.verb)
    {
        case CommandLineOptions::DiskOptions::Verb::List:
            RunList (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Get:
            RunGet (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Put:
            RunPut (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Delete:
            RunDelete (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Boot:
            RunBoot (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Help:
            result.output     += BuildHelpText (options.flagPrefix);
            result.exitStatus  = kClean;
            break;

        default:
            result.diagnostics += "unknown disk verb -- try: " + DescribeAcceptedVerbs() + "\n";
            result.exitStatus   = kNoOutput;
            break;
    }

    return result;
}
