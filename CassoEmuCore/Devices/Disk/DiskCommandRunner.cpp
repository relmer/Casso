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



//
//  Each disk option, and what it does.
//
//  Written without a prefix, because the help answers in whichever one the
//  reader typed and `/out` is one character narrower than `--out`. The column
//  is padded at build time rather than into each literal, or a table spaced for
//  one prefix comes out ragged in the other.
//
//
//  Every container this tool can WRITE, and the word that names it.
//
//  Separate from the reader in DiskImageStore, which recognizes what a file
//  already is. This is the shorter list of what a new one can be made as.
//
static constexpr DiskCommandRunner::ContainerName  s_kContainers[] =
{
    { "dsk", DiskFormat::Dsk },
    { "do",  DiskFormat::Do  },
    { "po",  DiskFormat::Po  },
    { "woz", DiskFormat::Woz },
};



//
//  Every disk command, and everything the page says about it.
//
//  ONE BLOCK PER COMMAND, because the page used to be four lists that a reader
//  had to join up themselves: the commands in one place, a grammar line in
//  another, every option of every command in one flat table, and the prose
//  explaining them somewhere below that. Answering "what can put do" meant
//  reading the whole page and filtering it, and the filtering was the reader's
//  job because the type option means one thing under put and a different thing
//  under create and the table could only say both at once.
//
//  `%L` AND `%S` STAND IN FOR THE PREFIXES the reader asked for, long and
//  short. Written out, every row would have to be built by concatenation and
//  the table would stop being readable as the page it produces.
//
//  The examples belong to their commands for the same reason the options do. A
//  worked loop still closes the page, because the loop is the thing no single
//  command demonstrates.
//
struct DiskCommandHelp
{
    const char *  forms;         // every accepted spelling, the plain one first
    const char *  summary;       // one line, for the list at the top
    const char *  grammar;       // where the operands go
    const char *  options;       // the options this command takes, or nothing
    const char *  discussion;    // what no option row can state, or nothing
    const char *  example;       // one line that does something real
};




static constexpr DiskCommandHelp  s_kDiskCommandHelp[] =
{
    { "list | cat | catalog | dir | ls",
      "Show what is on the disk",
      "CassoCli disk list <image>",
      nullptr,
      "A listing shows every column the volume records, so a ProDOS row carries eof= and"
      " aux=, the exact length of a file and the address a binary loads at. DOS 3.3"
      " records neither.",
      "CassoCli disk list mydisk.dsk" },

    { "get | read",
      "Read a file from the disk, to standard output or to %Lout <file>",
      "CassoCli disk get <image> <path> [%Lout <file>] [%Ltext | %Lbasic]",
      "  %Lout <file>            Write the extracted file here, not to standard output\n"
      "  %Ltext                  Convert the high-bit encoding and the line endings\n"
      "  %Lbasic                 Convert to and from an Applesoft listing\n",
      nullptr,
      "CassoCli disk get mydisk.dsk HELLO %Lbasic %Lout hello.bas" },

    { "put | write",
      "Write a file from the host to the disk",
      "CassoCli disk put <image> <file> [%Las <path>] [%Ltype <t>] [%Laddr $XXXX]\n"
      "                                   [%Ltext | %Lbasic]",
      "  %Las <path>             Name the placed file this on the disk\n"
      "  %Ltype <t>              The file type the catalog records. DOS 3.3 takes T, I,\n"
      "                          A, B or R; ProDOS takes TXT, BIN, BAS or SYS\n"
      "  %Laddr $XXXX            Load address for a placed binary\n"
      "  %Ltext                  Convert the high-bit encoding and the line endings\n"
      "  %Lbasic                 Convert to and from an Applesoft listing\n",
      "%Las is the name the file takes on the disk, and defaults to the name it has"
      " on the host. %Ltype is what the catalog records, and defaults to a binary, or to"
      " Applesoft under %Lbasic, which is the only type the guest will RUN. %Laddr is the"
      " load address a binary carries: a DOS 3.3 B or a ProDOS BIN is refused without one,"
      " every other type ignores it, and %Lbasic refuses it outright because Applesoft"
      " keeps its program at $0801 and nowhere else.",
      "CassoCli disk put mydisk.dsk prog.bin %Las PROG %Ltype B %Laddr $6000" },

    { "delete | del | rm",
      "Delete a file from the disk",
      "CassoCli disk delete <image> <path>",
      nullptr,
      nullptr,
      "CassoCli disk delete mydisk.dsk OLDPROG" },

    { "boot",
      "Set the program that runs when the disk is booted",
      "CassoCli disk boot <image> <path>",
      nullptr,
      "The program has to be on the volume already, named as the catalog records it, and"
      " the image has to carry an operating system on the tracks a boot reads. On ProDOS"
      " it must be a file of type SYS, and not the kernel itself. On DOS 3.3 the boot"
      " command is RUN, so an Applesoft or Integer program runs. Anything else is set,"
      " reported, and the disk boots without running it.",
      "CassoCli disk boot mydisk.dsk STARTUP" },

    { "create | new",
      "Make a new image file, formatted and ready to write to",
      "CassoCli disk create <image> [%Ltype <t>] [%Lformat <f>] [%Lvolume <v>]\n"
      "                               [%Lbootable [<image>]]\n"
      "                               [%Lboot <file> [%Laddr $XXXX] [%Lentry <addr>]]",
      "  %Ltype <t>              The container: dsk, do, po or woz. Taken from the\n"
      "                          name's extension when not given\n"
      "  %Lformat <f>            dos33, prodos or none. Defaults to dos33\n"
      "  %Lvolume <v>            A DOS 3.3 volume number, 1 to 254, or a ProDOS\n"
      "                          volume name\n"
      "  %Lbootable [<image>]    Copy an operating system on from this DOS 3.3 master\n"
      "                          or ProDOS system disk, so the disk boots. Alone, it\n"
      "                          uses the master the emulator already downloaded\n"
      "  %Lboot <file>           Make a disk that starts this binary with no operating\n"
      "                          system at all. It must load between $0900 and $BFFF\n"
      "  %Laddr $XXXX            Where a %Lboot binary loads\n"
      "  %Lentry <addr>          Start a %Lboot binary here rather than at its load\n"
      "                          address\n",
      "It refuses to write over an image that is already there; init is the command for"
      " meaning it. %Lboot and %Lbootable are the two ways to make a disk start something"
      " and cannot both be asked for: one puts an operating system on, the other puts a"
      " loader on instead of one.",
      "CassoCli disk create mydisk.dsk %Lbootable" },

    { "init | format",
      "Format an image that is already there, discarding everything on it",
      "CassoCli disk init <image> [%Lformat <f>] [%Lvolume <v>] [%Lbootable [<image>]]",
      "  %Lformat <f>            dos33, prodos or none. Defaults to dos33\n"
      "  %Lvolume <v>            A DOS 3.3 volume number, 1 to 254, or a ProDOS\n"
      "                          volume name\n"
      "  %Lbootable [<image>]    Copy an operating system on, as create does\n",
      "The container is taken as it is found, so there is no %Ltype here: an image that"
      " already exists already is one. Wanting a different container means wanting a"
      " different file, which is create.",
      "CassoCli disk init mydisk.dsk %Lformat prodos %Lvolume WORK" },

    { "stamp",
      "Lay a file from the host into the image at a track and a sector, with no"
      " filesystem involved",
      "CassoCli disk stamp <image> <file> %Ltrack <n> %Lsector <n>",
      "  %Ltrack <n>             Which track to write at, 0 to 34\n"
      "  %Lsector <n>            Which DOS logical sector to start at, 0 to 15. The\n"
      "                          bytes run on into the next track if they do not fit\n",
      "For a disk that boots its own loader and reads fixed tracks, where there is no"
      " catalog to put anything in. The sector is the LOGICAL one; the interleave is"
      " applied for you, so the bytes land where a running Apple II would look for them.",
      "CassoCli disk stamp boot.dsk loader.bin %Ltrack 0 %Lsector 0" },
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
//  The tables above are written with %L and %S so they stay readable as the
//  page they produce. A reader who typed /help is answered in slashes
//  throughout, which is the whole reason the placeholder exists.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::ApplyPrefixes (const std::string & text, char flagPrefix)
{
    std::string  longPrefix  = CommandLineHelp::LongPrefix  (flagPrefix);
    std::string  shortPrefix = CommandLineHelp::ShortPrefix (flagPrefix);
    std::string  out;



    for (size_t i = 0; i < text.size(); i++)
    {
        bool  isPlaceholder = text[i] == '%' && (i + 1) < text.size();

        if (isPlaceholder && text[i + 1] == 'L')
        {
            out += longPrefix;
            i++;
        }
        else if (isPlaceholder && text[i + 1] == 'S')
        {
            out += shortPrefix;
            i++;
        }
        else
        {
            out += text[i];
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildSubcommandHelp
//
//  One line per command: every form of it, then what it does.
//
//  EVERY ALIAS LEADS THE LINE IT BELONGS TO rather than trailing it in an "also
//  written" clause. A person coming from an Apple II reaches for CATALOG, one
//  coming from the host shell for DIR, one from a Unix shell for LS; all three
//  are accepted, and a reader scanning the left margin for the word they already
//  have in mind finds it there instead of at the end of a sentence about a word
//  they do not use.
//
//  THE DESCRIPTIONS CARRY THE DIRECTION, which is why nothing here explains that
//  put and get are named from the disk's point of view. "Read a file FROM the
//  disk" and "Write a file FROM the host TO the disk" leave nothing for that
//  add; it existed to rescue two command names that the descriptions now state
//  outright.
//
//  This is the contents list. What each command takes is under its own heading
//  further down, which is where a reader goes once they know which one they
//  want.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildSubcommandHelp (char flagPrefix)
{
    const size_t  kDescriptionColumn = 36;
    std::string   text;



    for (const DiskCommandHelp & entry : s_kDiskCommandHelp)
    {
        std::string  line = "  " + std::string (entry.forms);

        while (line.size() < kDescriptionColumn)
        {
            line += " ";
        }

        text += ApplyPrefixes (line + entry.summary, flagPrefix) + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildCommandBlocks
//
//  Each command, with its grammar, its own options, what no option row can
//  state, and one example that does something real.
//
//  THE OPTIONS BELONG TO THE COMMAND rather than to a table of all of them.
//  A flat table cannot say that type names a file type under put and a
//  container under create, so it said both in one row and left the reader to
//  work out which half applied. Two rows under two headings say it once each.
//
//  An option shared by two commands is written under both. That is the point:
//  a reader looking at put should not have to know that get has a text option
//  in order to find out that put does.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildCommandBlocks (char flagPrefix)
{
    std::string  text;



    for (const DiskCommandHelp & entry : s_kDiskCommandHelp)
    {
        text += "\n";
        text += ApplyPrefixes (entry.forms, flagPrefix) + "\n";
        text += ApplyPrefixes (std::string ("  ") + entry.grammar, flagPrefix) + "\n";

        if (entry.options != nullptr)
        {
            text += "\n";
            text += ApplyPrefixes (entry.options, flagPrefix);
        }

        if (entry.discussion != nullptr)
        {
            text += "\n  ";
            text += ApplyPrefixes (entry.discussion, flagPrefix);
            text += "\n";
        }

        //  The round-trip promise is quoted from the tokenizer that keeps it,
        //  so the claim on the page cannot drift from the code making it.
        if (std::string (entry.forms).find ("get") != std::string::npos)
        {
            text += "\n  ";
            text += ApplesoftTokenizer::RoundTripHelpText (flagPrefix);
            text += "\n";
            text += ApplyPrefixes (
                "\n  Naming neither conversion moves the bytes unchanged, which is what"
                " makes extract, edit and replace safe. The length and whatever header the"
                " type carries are still applied, because those record where a file ENDS.\n",
                flagPrefix);
        }

        text += ApplyPrefixes (std::string ("\n  Example:\n    ") + entry.example, flagPrefix) + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildOptionsHelp
//
//  The statuses this subcommand exits with.
//
//  EXIT STATUSES ARE THIS SUBCOMMAND'S OWN. They spent a while stated once at
//  the top of the page for every mode, on the belief that the three modes
//  agreed; they do not. An assembly error exits 2 under the assembler and 1
//  under `run`, and status 1 means "the output was written anyway" in one mode
//  and "nothing ran" in another. So each mode states its own, under itself, and
//  this is disk's -- see kExitStatusHelpText below for the wording and
//  DiskCommandRunner for what assigns each one.
//
//  NOT THE IN-USE PROBE, THOUGH. A locked image is refused by name where it
//  happens ("is open in another program. Close it and try again"), which is
//  the report a user acts on; a paragraph restating that in the help was
//  documentation of an error message.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildOptionsHelp (char flagPrefix)
{
    std::string  text = "Exit codes:\n";



    //  The statuses read the same in either prefix; the parameter
    //  is here because every other page builder takes one.
    (void) flagPrefix;

    text += kExitStatusHelpText;

    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildExampleHelp
//
//  THE LOOP IS THE ONE THING NO SINGLE COMMAND SHOWS. Every command carries its
//  own example now, under its own heading; what is left for the end of the page
//  is the sequence, which is what a reader who has never used the tool actually
//  needs and which no per-command example can demonstrate.
//
//  THE COMMANDS THEMSELVES COME FROM CommandLineHelp, because the general page
//  shows the same loop -- it is the one thing on that page that is not a table
//  of contents -- and a loop written out twice is a loop whose two copies will
//  eventually place different files. What is added here is the prose, which
//  belongs to this page alone: every flag it explains is described on this page
//  and nowhere else.
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
//  `--disk1` keeps the `--` form whatever the reader asked for, here as in
//  the commands themselves: it is the emulator's flag rather than this tool's.
//  See CommandLineHelp::BuildExampleCommands.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildExampleHelp (char flagPrefix)
{
    std::string  lp = CommandLineHelp::LongPrefix (flagPrefix);
    std::string  sp = CommandLineHelp::ShortPrefix (flagPrefix);



    return CommandLineHelp::BuildExampleCommands (flagPrefix) +
        "\n"
        "  create makes the disk the rest of the loop writes to, and " + lp + "bootable copies"
        " an operating system onto it so the machine has something to start. " + sp + "o names"
        " the assembled output file. The last line is the emulator's"
        " own command line rather than this tool's, which is why its flags are written"
        " with two dashes whatever prefix you asked for here: --machine Apple2e opens"
        " an Apple //e, and --disk1 puts the image in drive 1 as it starts.\n"
        "  Assemble with the default output rather than " + lp + "dos-bin: put writes the"
        " DOS 3.3 header itself from " + lp + "addr, and a file that already carries one has"
        " its own header loaded as code where the program should begin.\n"
        "  greet.bas holds one Applesoft line, 10 PRINT CHR$(4);\"BRUN PROG\", because a"
        " booting DOS 3.3 volume RUNs its greeting. Naming the binary there sets the"
        " name and the disk boots without running it.\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildHelpText
//
//  The whole page: what the subcommand is for, the commands it takes, each one
//  in detail, the statuses, and the loop.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::BuildHelpText (char flagPrefix, const std::string & banner)
{
    return (banner.empty() ? std::string() : banner + "\n")
         + "Usage:\n"
         + CommandLineHelp::UsageLineFor (CommandLineOptions::Subcommand::Disk) + "\n"
           "\n"
           "Disk commands:\n"
         + BuildSubcommandHelp (flagPrefix)
         + BuildCommandBlocks  (flagPrefix) + "\n"
         + BuildOptionsHelp    (flagPrefix) + "\n"
         + BuildExampleHelp    (flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeAcceptedCommands
//
//  The command table read out in the order it is written, which is each command
//  followed by its own aliases -- so the suggestion a user is offered groups
//  the way the help does rather than alphabetically, where `cat` would land
//  three words from `catalog`.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeAcceptedCommands()
{
    std::string  text;



    for (const auto & command : CommandLineParser::GetAllDiskCommands())
    {
        if (!text.empty())
        {
            text += ", ";
        }

        text += command.name;
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
                            ? "track 0 sector 0 carries code. The drive's ROM reads\n"
                              "                that sector and jumps into it, so this image"
                              " boots something.\n                It simply keeps its files"
                              " somewhere this tool does not read"
                            : "track 0 sector 0 is blank, so nothing here would boot");

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
    DiskCommandResult  & result,
    bool                 requireFilesystem)
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

    if (outOpened.kind == VolumeKind::Unknown && requireFilesystem)
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
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        return "is locked on this volume. Unlock it on the disk before writing over it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DISK_FULL))
    {
        return "does not fit: the volume has no room left, either for the file's "
               "contents or for another catalog entry";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME))
    {
        return "is not a name this filesystem can store. It must be a single "
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
        return "is a binary, which has to be told where it loads. Give --addr $XXXX";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED))
    {
        return "is a directory, and this tool does not go inside one, so removing "
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
        return "is not a program this volume's boot path launches. On ProDOS that "
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
        return "is write-protected. Clear its read-only attribute and try again. "
               "Nothing was written";
    }

    return "could not be replaced. It may be read-only or in use";
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

    //  A disk being made for the first time has nothing to have changed, so
    //  the check is skipped rather than failed. See OpenedImage::isNew.
    if (!opened.isNew)
    {
        CBRFEx (opened.stampRecorded, HRESULT_FROM_WIN32 (ERROR_CANT_ACCESS_FILE),
                RefuseCommit (opened.imagePath,
                              "could not be checked for changes when it was read, so it will "
                              "not be written over", result));

        hr = m_fileIo.Stat (opened.imagePath, observed);
        CHRF (hr, RefuseCommit (opened.imagePath,
                                "has gone away since it was read", result));

        stale = CommitPlan::IsStale (opened.stamp.sizeBytes, opened.stamp.modifiedUnix,
                                     observed.sizeBytes,     observed.modifiedUnix);

        // STG_E_NOTCURRENT says exactly this and nothing else -- the object
        // changed since it was last read. The Win32 table has no code for it,
        // and inventing a near-miss from it would read as a different problem
        // in a log.
        CBRFEx (!stale, STG_E_NOTCURRENT,
                RefuseCommit (opened.imagePath,
                              "changed since it was read. Nothing was written; read it "
                              "again and retry", result));
    }

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
                          "already has that many temporary files beside it. Remove them "
                          "and try again", result));

    progress.furthestAttempted = CommitPlan::Step::WriteTemporary;

    hr = m_fileIo.WriteAllBytes (tempPath, newImageBytes);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "could not be written beside. The folder may be read-only "
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
        result.diagnostics += Failure (options.disk.imagePath, "",
            note + ". THIS LISTING IS INCOMPLETE, entries may be missing") + "\n";
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
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = kNoOutput;
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
                  "%d sector(s) could not be decoded. THIS FILE IS INCOMPLETE, "
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
//  --as when the caller gave one, and otherwise the last component of the
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
    else
    {
        outType = isDos ? (isText ? Dos33Volume::kTypeText  : Dos33Volume::kTypeBinary)
                        : (isText ? ProDosVolume::kTypeText : ProDosVolume::kTypeBinary);
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
        result.diagnostics += "--type " + options.disk.typeName + " means nothing on this volume: "
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
//  What a file from the host becomes on the disk.
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
                          "byte %u of the file has no Apple II text representation, "
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
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = kNoOutput;
            result.badCommandLine  = true;
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
        result.diagnostics    += "Error: no file named to place\n"
                                 "       put takes the file to copy onto the disk\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;
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
        result.diagnostics    += "Error: no program named to boot\n"
                                 "       boot takes the file on the disk to run after the "
                                 "operating system loads\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;
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
            "is set as the startup program, but a booting DOS 3.3 RUNs its greeting, "
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
    DiskCommandResult  result;
    bool               refused = options.parseVerdict == CommandLineOptions::ParseVerdict::Refused;



    if (refused)
    {
        result.exitStatus = kNoOutput;
        return result;
    }

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

        case CommandLineOptions::DiskOptions::Command::Stamp:
            RunStamp (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Create:
            RunCreate (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Init:
            RunInit (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Help:
            result.output     += BuildHelpText (options.flagPrefix, m_banner);
            result.exitStatus  = kClean;
            break;

        default:
            result.diagnostics    += options.disk.commandWord.empty()
                                         ? std::string ("Error: no disk command given\n")
                                         : "Error: unknown disk command: " + options.disk.commandWord + "\n";
            result.exitStatus      = kNoOutput;
            result.badCommandLine  = true;
            break;
    }

    return result;
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
                                             DiskFormat               & outFormat,
                                             DiskCommandResult        & result)
{
    HRESULT      hr    = S_OK;
    std::string  asked = options.disk.containerType;



    if (asked.empty())
    {
        //  No --type, so the name decides. A name carrying no extension this
        //  tool knows is refused for the same reason an unknown --type is.
        hr = DiskImageStore::DetectFormatByExtension (options.disk.imagePath, outFormat);

        if (FAILED (hr))
        {
            result.diagnostics    += "Error: cannot tell what kind of image " + options.disk.imagePath
                                   + " should be\n"
                                     "       name it .dsk, .do, .po or .woz, or say which with --type\n";
            result.exitStatus      = kNoOutput;
            result.badCommandLine  = true;
        }

        return hr;
    }

    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    for (const ContainerName & entry : s_kContainers)
    {
        if (asked == entry.name)
        {
            outFormat = entry.format;
            return S_OK;
        }
    }

    result.diagnostics    += "Error: unknown image type: " + options.disk.containerType + "\n"
                             "       this tool writes dsk, do, po and woz\n";
    result.exitStatus      = kNoOutput;
    result.badCommandLine  = true;

    return E_INVALIDARG;
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
    std::string  asked = options.disk.formatName;



    if (asked.empty())
    {
        outContents = BlankDiskContents::Dos33;
        return S_OK;
    }

    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    if (asked == "dos33" || asked == "dos" || asked == "dos3.3")
    {
        outContents = BlankDiskContents::Dos33;
        return S_OK;
    }

    if (asked == "prodos")
    {
        outContents = BlankDiskContents::ProDos;
        return S_OK;
    }

    if (asked == "none" || asked == "raw" || asked == "unformatted")
    {
        outContents = BlankDiskContents::Unformatted;
        return S_OK;
    }

    result.diagnostics    += "Error: unknown format: " + options.disk.formatName + "\n"
                             "       this tool formats dos33, prodos, or none\n";
    result.exitStatus      = kNoOutput;
    result.badCommandLine  = true;

    return E_INVALIDARG;
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
    const std::string &  asked  = options.disk.volumeName;
    int                  number = 0;



    if (asked.empty())
    {
        return S_OK;
    }

    if (inOutSpec.contents == BlankDiskContents::ProDos)
    {
        inOutSpec.volumeName = asked;
        return S_OK;
    }

    //  A DOS 3.3 volume number, and only a number. Read by hand so a word that
    //  is not one at all is refused rather than quietly reading as zero.
    for (char letter : asked)
    {
        if (letter < '0' || letter > '9')
        {
            number = -1;
            break;
        }

        number = (number * 10) + (letter - '0');
    }

    if (number < 1 || number > 254)
    {
        result.diagnostics    += "Error: " + asked + " is not a DOS 3.3 volume number\n"
                                 "       give a number from 1 to 254, or format the disk as prodos "
                                 "to name it instead\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return E_INVALIDARG;
    }

    inOutSpec.volumeNumber = (Byte) number;

    return S_OK;
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
    std::string            master   = options.disk.bootableFrom;
    vector<Byte>           osBytes;



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    if (!options.disk.directBootFile.empty() && options.disk.bootable)
    {
        result.diagnostics    += "Error: --bootable and --boot ask for different disks\n"
                                 "       --bootable copies an operating system on; --boot starts a "
                                 "binary with no operating system at all\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return E_INVALIDARG;
    }

    if (!options.disk.bootable)
    {
        return S_OK;
    }

    //  A BARE --bootable MEANS THE ONE THE EMULATOR DOWNLOADED. Which of the
    //  two that is follows the format being written, because a ProDOS disk
    //  cannot be made bootable out of a DOS master.
    if (master.empty())
    {
        if (!StockBootDisks::IsCached (which))
        {
            result.diagnostics    += std::string ("Error: the ")
                                   + (isProDos ? "ProDOS" : "DOS 3.3")
                                   + " master has not been downloaded yet\n"
                                     "       run the emulator once to fetch it, or name a master with "
                                     "--bootable <image>\n";
            result.exitStatus      = kNoOutput;
            result.badCommandLine  = true;

            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        master = StockBootDisks::PathFor (which);
    }

    hr = m_fileIo.ReadAllBytes (master, osBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (master, "",
                                       "cannot be read, so there is no operating system to copy") + "\n";
        result.exitStatus   = kNoOutput;

        return hr;
    }

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

    return S_OK;
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
                                       bool                       overExisting,
                                       DiskCommandResult        & result)
{
    HRESULT        hr = S_OK;
    BlankDiskSpec  spec;
    BootPayload    payload;
    vector<Byte>   imageBytes;
    OpenedImage    target;



    spec.format = format;

    hr = ResolveContents (options, spec.contents, result);
    if (FAILED (hr))
    {
        return;
    }

    hr = ResolveVolume (options, spec, result);
    if (FAILED (hr))
    {
        return;
    }

    hr = ResolveBoot (options, spec, payload, result);
    if (FAILED (hr))
    {
        return;
    }

    hr = BlankDiskBuilder::ValidateSpec (spec);

    if (FAILED (hr))
    {
        //  The pairing rules are the builder's: a DOS 3.3 catalog cannot go in
        //  a .po, a ProDOS directory cannot go in a .dsk, and a bootable spec
        //  needs the master its own format calls for.
        result.diagnostics    += "Error: that combination cannot be written\n"
                                 "       dsk and do carry DOS 3.3, po carries ProDOS, and woz carries "
                                 "either; a bootable disk needs the master for its own format\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    hr = BlankDiskBuilder::Build (spec, payload, imageBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, "", "could not be built") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

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

    hr = CommitImage (target, imageBytes, result);

    if (SUCCEEDED (hr))
    {
        result.output     += options.disk.imagePath + ": " + DescribeNewDisk (spec) + "\n";
        result.exitStatus  = kClean;
    }
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
    HRESULT     hr     = S_OK;
    DiskFormat  format = DiskFormat::Dsk;



    if (options.disk.imagePath.empty())
    {
        result.diagnostics    += "Error: no image named to create\n"
                                 "       create takes the name of the image file to write\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    if (m_fileIo.Exists (options.disk.imagePath))
    {
        result.diagnostics += Failure (options.disk.imagePath, "",
                                       "is already there, and create will not write over it. "
                                       "Use init to reformat it, or choose another name") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    hr = ResolveContainer (options, format, result);
    if (FAILED (hr))
    {
        return;
    }

    if (!options.disk.directBootFile.empty())
    {
        BuildDirectBoot (options, format, result);
        return;
    }

    BuildAndWrite (options, format, false, result);
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
                                         DiskCommandResult        & result)
{
    HRESULT         hr = S_OK;
    DirectBootSpec  spec;
    vector<Byte>    payload;
    vector<Byte>    sectors;
    vector<Byte>    imageBytes;
    OpenedImage     target;
    std::string     refusal;



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    //
    //  Checked here as well as in ResolveBoot, because this path never reaches
    //  ResolveBoot: it builds no filesystem, so it skips the code that would
    //  have caught the pair. Measured before the check went in, the two
    //  together honored --boot and dropped --bootable without a word.
    if (options.disk.bootable)
    {
        result.diagnostics    += "Error: --bootable and --boot ask for different disks\n"
                                 "       --bootable copies an operating system on; --boot starts a "
                                 "binary with no operating system at all\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    //  A filesystem was asked for AND a disk with none. Refused rather than
    //  quietly dropping one of them.
    if (!options.disk.formatName.empty() && options.disk.formatName != "none")
    {
        result.diagnostics    += "Error: --boot writes no filesystem, so --format "
                               + options.disk.formatName + " cannot be honored\n"
                                 "       a direct-boot disk holds the binary and nothing else\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    hr = m_fileIo.ReadAllBytes (options.disk.directBootFile, payload);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.directBootFile, "",
                                       "cannot be read, so there is nothing to boot") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    //  The load address is the one the binary was assembled for, and --addr is
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

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.directBootFile, refusal) + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    hr = BlankDiskBuilder::WrapInContainer (format, false, sectors, imageBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, "", "could not be built") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = true;

    hr = CommitImage (target, imageBytes, result);

    if (SUCCEEDED (hr))
    {
        char  summary[160] = {};

        snprintf (summary, sizeof (summary),
                  "%s: direct boot, %zu bytes loading at $%04X, entered at $%04X\n",
                  options.disk.imagePath.c_str(), payload.size(),
                  (unsigned) spec.loadAddress, (unsigned) spec.entryAddress);

        result.output     += summary;
        result.exitStatus  = kClean;
    }
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
    HRESULT     hr     = S_OK;
    DiskFormat  format = DiskFormat::Dsk;



    if (options.disk.imagePath.empty())
    {
        result.diagnostics    += "Error: no image named to format\n"
                                 "       init takes the image file to format again\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    if (!m_fileIo.Exists (options.disk.imagePath))
    {
        result.diagnostics += Failure (options.disk.imagePath, "",
                                       "is not there. Use create to make one") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    if (!options.disk.containerType.empty())
    {
        result.diagnostics    += "Error: init does not take --type\n"
                                 "       the image already has a container; create makes one with a "
                                 "different container\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    //  From the file's own name, because the file is what is being reformatted.
    hr = DiskImageStore::DetectFormatByExtension (options.disk.imagePath, format);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, "",
                                       "is not a kind of image this tool writes") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    BuildAndWrite (options, format, true, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunStamp
//
//  A file from the host laid into an image at a track and a DOS logical sector.
//
//  NO FILESYSTEM IS INVOLVED, WHICH IS THE POINT. A demo that boots its own
//  loader and reads fixed tracks has no catalog to make an entry in and no
//  allocator to ask for space, so `put` cannot express it at all. This writes
//  the bytes given, where it is told, and nothing else.
//
//  THE SECTOR IS LOGICAL, NOT PHYSICAL. Logical numbering is what a source
//  listing and a boot loader both speak; the position on the disk differs from
//  it by the interleave, and translating between the two belongs to the layer
//  that owns the skew. A caller doing that arithmetic itself is a second copy
//  of the sixteen numbers, which is how an image comes to read back perfectly
//  through our own reader and be garbage on real hardware.
//
//  It runs on past the end of a track into the next one, because a payload
//  longer than 4 KB is ordinary and splitting the call per track would put the
//  wrap arithmetic back in the caller's hands.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunStamp (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT       hr           = S_OK;
    size_t        needed       = 0;
    size_t        written      = 0;
    OpenedImage   opened;
    vector<Byte>  payload;
    vector<Byte>  edited;
    char          summary[160] = {};



    if (options.disk.hostFile.empty())
    {
        result.diagnostics    += "Error: no file named to stamp\n"
                                 "       stamp takes the image and the file to lay into it\n";
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    if (options.disk.track < 0 || options.disk.track >= NibblizationLayer::kTrackCount
     || options.disk.sector < 0 || options.disk.sector >= NibblizationLayer::kSectorsPerTrack)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is not on this disk\n"
                  "       tracks run 0 to %d and sectors 0 to %d\n",
                  options.disk.track, options.disk.sector,
                  NibblizationLayer::kTrackCount - 1,
                  NibblizationLayer::kSectorsPerTrack - 1);

        result.diagnostics    += summary;
        result.exitStatus      = kNoOutput;
        result.badCommandLine  = true;

        return;
    }

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, payload);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.hostFile, "", "cannot be read") + "\n";
        result.exitStatus   = kNoOutput;

        return;
    }

    //  A disk with no filesystem is the ordinary case here rather than a
    //  refusal: a demo that boots its own loader has no catalog, and that
    //  is exactly the disk this command exists to write.
    hr = OpenImage (options.disk.imagePath, opened, result, false);

    if (FAILED (hr))
    {
        return;
    }

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

        if (first + needed > total)
        {
            snprintf (summary, sizeof (summary),
                      "Error: %zu bytes will not fit from track %d sector %d\n"
                      "       that is %zu sectors and the disk has %zu left there\n",
                      payload.size(), options.disk.track, options.disk.sector,
                      needed, total - first);

            result.diagnostics    += summary;
            result.exitStatus      = kNoOutput;
            result.badCommandLine  = true;

            return;
        }

        for (size_t index = 0; index < needed; index++)
        {
            size_t  running = first + index;
            int     track   = (int) (running / (size_t) NibblizationLayer::kSectorsPerTrack);
            int     logical = (int) (running % (size_t) NibblizationLayer::kSectorsPerTrack);
            size_t  at      = (size_t) ((track * NibblizationLayer::kSectorsPerTrack
                                       + NibblizationLayer::DskFileIndexForDosLogicalSector (logical))
                                      * NibblizationLayer::kSectorByteSize);
            size_t  from    = index * (size_t) NibblizationLayer::kSectorByteSize;
            size_t  count   = std::min ((size_t) NibblizationLayer::kSectorByteSize,
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

    hr = SaveAndCommit (opened, edited, result);

    if (FAILED (hr))
    {
        return;
    }

    snprintf (summary, sizeof (summary),
              "%s: %zu bytes at track %d sector %d, %zu sector(s)\n",
              options.disk.imagePath.c_str(), written,
              options.disk.track, options.disk.sector, needed);

    result.output     += summary;
    result.exitStatus  = kClean;
}
