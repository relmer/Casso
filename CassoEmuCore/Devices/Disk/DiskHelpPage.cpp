#include "Pch.h"

#include "DiskHelpPage.h"
#include "CommandLineHelp.h"
#include "CommandLineParser.h"




//
//  Each disk option, and what it does.
//
//  Written without a prefix, because the help answers in whichever one the
//  reader typed and `/out` is one character narrower than `--out`. The column
//  is padded at build time rather than into each literal, or a table spaced for
//  one prefix comes out ragged in the other.
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




static constexpr DiskHelpPage::DiskCommandHelp  s_kDiskCommandHelp[] =
{
    { 
        CommandLineOptions::DiskOptions::Command::List,
        "list | cat | catalog | dir | ls",
        "Show what is on the disk",
        "CassoCli disk list <image>",
        nullptr,
        //  NO PARAGRAPH. It used to explain that a ProDOS row carries eof= and
        //  aux= where a DOS 3.3 row does not, which is a difference the reader
        //  learns by running the command once. The summary says what the command
        //  is for and the columns speak for themselves.
        nullptr,
        "CassoCli disk list mydisk.dsk" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Get,
        "get | read",
        "Read a file from the disk",
        "CassoCli disk get <image> <name> [%Lout <file>] [%Ltext | %Lbasic]",
        "  %Lout <file>            Extract the file to <file>. By default, the file is written to stdout instead\n"
        "  %Ltext                  Convert Apple high-ASCII encoding and line endings to standard ASCII with Windows line endings\n"
        "  %Lbasic                 Convert tokenized Applesoft BASIC to readable text\n",
        "Applesoft BASIC programs are stored in a tokenized form on disk. Retrieving these with 'get' returns the raw tokenized file,"
        " so it can be copied losslessly to another disk. However, the tokenized format is not human-readable. Using the %Lbasic switch"
        " detokenizes it into human-readable form, but this is not an identical copy of the stored program.  It loses whitespace (outside"
        " of strings, REM, and DATA statements), converts lowercase characters (again, only outside of those three constructs) to uppercase,"
        " converts ? shorthand to PRINT statements, and returns lines in numeric order. None of these affect the execution of the code, but"
        " be aware that these conversions happen. Also note that Applesoft BASIC does these same conversions itself to any line entered at"
        " the prompt.",
        "CassoCli disk get mydisk.dsk HELLO %Lbasic %Lout hello.bas" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Put,
        "put | write",
        "Write a file from the host to the disk",
        "CassoCli disk put <image> <file> [%Las <name>] [%Ltype <t>] [%Lload $XXXX]\n"
        "                                   [%Ltext | %Lbasic]",
        "  %Las <name>             Store the file as <name> on the disk\n"
        "  %Ltype <t>              CassoCli detects a file's type automatically; this switch overrides that. For DOS 3.3 the types are"
                                   " T (text), I (Integer BASIC), A (Applesoft BASIC), B (binary) and R (relocatable); for ProDOS,"
                                   " TXT, BIN, BAS and SYS. A file it cannot identify is stored as a binary\n"
        "  %Lload $XXXX            Load address for a binary file, written as $6000 or 0x6000\n"
        "  %Ltext                  Convert text to Apple high-ASCII and Apple line endings\n"
        "  %Lbasic                 Convert readable text to the tokenized form Applesoft BASIC runs\n",
        "To store a human-readable Applesoft BASIC program to disk, use the %Lbasic switch to tokenize the program into Applesoft BASIC's"
        " runnable format. If the Applesoft BASIC program is already tokenized (e.g., you retrieved it from disk without using %Lbasic),"
        " you can simply put it on another disk without conversion.  Use %Lbasic only when conversion from plain text to tokenized Applesoft"
        " BASIC is required.",
        "CassoCli disk put mydisk.dsk prog.bin %Las PROG %Ltype B %Lload $6000" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Delete,
        "delete | del | rm",
        "Delete a file from the disk",
        "CassoCli disk delete <image> <name>",
        nullptr,
        nullptr,
        "CassoCli disk delete mydisk.dsk OLDPROG" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Boot,
        "boot",
        "Set the program that runs when the disk is booted",
        "CassoCli disk boot <image> <name>",
        nullptr,
        "The program has to be on the volume already, and the image must contain the DOS 3.3 or ProDOS operating system; simply"
        " being formatted as DOS 3.3 or ProDOS is not sufficient. On DOS 3.3 disks, the file must be an Applesoft BASIC (type A)"
        " or Integer BASIC (type I) program. On ProDOS disks, the file must be a system file (type SYS), and cannot be the kernel itself.",
        "CassoCli disk boot mydisk.dsk STARTUP" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Create,
        "create | new",
        "Make a new image file, formatted and ready to use",
        "CassoCli disk create <image> [%Ltype <t>] [%Lformat <f>] [%Lvolume <v>]\n"
        "                               [%Lbootable [<image>]]\n"
        "                               [%Lboot <file> [%Lload $XXXX] [%Lexec $XXXX]]",
        "  %Ltype <t>              The container type is taken from the name's extension by default; use this switch to override. "
                                   "Valid types are: dsk, do, po, or woz\n"
        "  %Lformat <f>            The filesystem: dos33, prodos, or none. Defaults to dos33\n"
        "  %Lvolume <v>            For DOS 3.3, a volume number from 1 to 254 (default 254); for ProDOS, the volume name (default NEWDISK)\n"
        "  %Lbootable [<image>]    Makes the disk bootable by copying operating system files to it. It automatically uses the master disk"
                                   " for the selected format, but this can be overridden by supplying an image of your own\n"
        "  %Lboot <file>           Loads and executes the binary <file> directly without copying or relying on operating system files\n"
        "  %Lload $XXXX            Where a %Lboot binary is loaded into memory. Valid addresses are $0900-$BFFF, inclusive\n"
        "  %Lexec $XXXX            Memory address to jump to after loading the %Lboot binary file. Defaults to the load"
                                   " address, and must be an address within the loaded binary\n",
        nullptr,
        "CassoCli disk create mydisk.dsk %Lbootable" 
    },

    { 
        CommandLineOptions::DiskOptions::Command::Init,
        "init | format",
        "Format an existing disk, erasing any existing files",
        "CassoCli disk init <image> [%Lformat <f>] [%Lvolume <v>] [%Lbootable [<image>]]",
        "  %Lformat <f>            The filesystem: dos33, prodos, or none. Defaults to dos33\n"
        "  %Lvolume <v>            For DOS 3.3, a volume number from 1 to 254 (default 254); for ProDOS, the volume name (default NEWDISK)\n"
        "  %Lbootable [<image>]    Makes the disk bootable by copying operating system files to it. It automatically uses the master disk"
                                   " for the selected format, but this can be overridden by supplying an image of your own\n",
        nullptr,
        "CassoCli disk init mydisk.dsk %Lformat prodos %Lvolume WORK" 
    },

    {
        CommandLineOptions::DiskOptions::Command::SectorRead,
        "sectorread",
        "Read sectors directly from the disk",
        "CassoCli disk sectorread <image> %Llogical|%Lphysical %Ltrack <n> %Lsector <n> [%Lcount <n>] [%Lout <file>]",
        "  %Llogical               Sector numbers are DOS logical: what catalogs, DOS tools and reference books use\n"
        "  %Lphysical              Sector numbers are physical: the address-field order a boot loader reads off the drive\n"
        "  %Ltrack <n>             Track to read from, 0 to 34\n"
        "  %Lsector <n>            Sector to start at, 0 to 15\n"
        "  %Lcount <n>             Count of sectors to read. Defaults to 1. Continues to subsequent tracks and sectors as needed\n"
        "  %Lout <file>            File to store the read sectors in. Defaults to stdout if not specified\n",
        "Allows reading data directly from track and sector locations without relying on filesystem structure. Sector numbers can be specified"
        " in %Llogical or %Lphysical sectors.",
        "CassoCli disk sectorread boot.dsk %Llogical %Ltrack 0 %Lsector 0 %Lout boot.bin"
    },

    {
        CommandLineOptions::DiskOptions::Command::SectorWrite,
        "sectorwrite",
        "Write sectors directly to the disk",
        "CassoCli disk sectorwrite <image> <file> %Llogical|%Lphysical %Ltrack <n> %Lsector <n>",
        "  %Llogical               Sector numbers are DOS logical: what catalogs, DOS tools and reference books use\n"
        "  %Lphysical              Sector numbers are physical: the address-field order a boot loader reads off the drive\n"
        "  %Ltrack <n>             Track to write to, 0 to 34\n"
        "  %Lsector <n>            Sector to start at, 0 to 15. Continues to subsequent tracks and sectors if file is larger than one sector\n",
        "Allows writing data directly to track and sector locations without relying on filesystem structure. Sector numbers can be specified"
        " in %Llogical or %Lphysical sectors. If the data doesn't fill"
        " an entire sector, the remaining bytes from the original sector are preserved.",
        "CassoCli disk sectorwrite boot.dsk loader.bin %Lphysical %Ltrack 0 %Lsector 0"
    },

    {
        CommandLineOptions::DiskOptions::Command::BlockRead,
        "blockread",
        "Read 512-byte ProDOS blocks directly from the disk",
        "CassoCli disk blockread <image> %Lblock <n> [%Lcount <n>] [%Lout <file>]",
        "  %Lblock <n>             Block to start at, 0 to 279\n"
        "  %Lcount <n>             Count of blocks to read. Defaults to 1\n"
        "  %Lout <file>            File to store the read blocks in. Defaults to stdout if not specified\n",
        "Allows reading data directly by ProDOS block number without relying on filesystem structure. The disk doesn't need ProDOS on it, and"
        " any container works, not only .po images.",
        "CassoCli disk blockread users.po %Lblock 2 %Lcount 4 %Lout directory.bin"
    },

    {
        CommandLineOptions::DiskOptions::Command::BlockWrite,
        "blockwrite",
        "Write 512-byte ProDOS blocks directly to the disk",
        "CassoCli disk blockwrite <image> <file> %Lblock <n>",
        "  %Lblock <n>             Block to start at, 0 to 279. Continues to subsequent blocks if file is larger than one block\n",
        "Allows writing data directly by ProDOS block number without relying on filesystem structure. If the data doesn't fill an entire"
        " block, the remaining bytes from the original block are preserved.",
        "CassoCli disk blockwrite boot.po boot.bin %Lblock 0"
    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::ApplyPrefixes
//
//  Puts the reader's own prefixes into a line of help.
//
//  The tables above are written with %L and %S so they stay readable as the
//  page they produce. A reader who typed /help is answered in slashes
//  throughout, which is the whole reason the placeholder exists.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DiskHelpPage::DiskCommandHelp> DiskHelpPage::GetCommandHelp()
{
    return std::span<const DiskCommandHelp> (s_kDiskCommandHelp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::ApplyPrefixes
//
//  %L and %S become the long and short prefix the reader asked for.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::ApplyPrefixes (const std::string & text, char flagPrefix)
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
//  DiskHelpPage::BuildSubcommandHelp
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

std::string DiskHelpPage::BuildSubcommandHelp (char flagPrefix)
{
    const size_t  kDescriptionColumn = 36;
    std::string   text;



    for (const DiskHelpPage::DiskCommandHelp & entry : s_kDiskCommandHelp)
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
//  DiskHelpPage::AsProse
//
//  A paragraph with its runs of spaces collapsed to one.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::AsProse (const std::string & text)
{
    std::string  out;
    bool         wasSpace = false;



    for (char letter : text)
    {
        bool  isSpace = letter == ' ';

        if (!isSpace || !wasSpace)
        {
            out += letter;
        }

        wasSpace = isSpace;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildOneBlock
//
//  One command's heading, grammar, options, discussion and example.
//
//  Shared by the whole page and by a single command's usage, so the two can
//  never describe the same command differently.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::BuildOneBlock (const DiskCommandHelp & entry, char flagPrefix)
{
    std::string  text;



    //  UNDERLINED LIKE THE ASSEMBLER'S SECTIONS. Eight headings down a long
    //  page all look like body text until one of them is ruled.
    std::string  heading = ApplyPrefixes (entry.forms, flagPrefix);

    text += heading + "\n";
    text += std::string (heading.size(), '-') + "\n";
    text += ApplyPrefixes (std::string ("  ") + entry.grammar, flagPrefix) + "\n";

    if (entry.options != nullptr)
    {
        text += "\n";
        text += ApplyPrefixes (entry.options, flagPrefix);
    }

    if (entry.discussion != nullptr)
    {
        text += "\n  ";
        text += AsProse (ApplyPrefixes (entry.discussion, flagPrefix));
        text += "\n";
    }

    text += ApplyPrefixes (std::string ("\n  Example:\n    ") + entry.example, flagPrefix) + "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildCommandHelp
//
//  The block for one command, for a caller that has a Command rather than the
//  whole page to print.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::BuildCommandHelp (CommandLineOptions::DiskOptions::Command command,
                                                 char flagPrefix)
{
    std::string  block;



    for (const DiskCommandHelp & entry : s_kDiskCommandHelp)
    {
        if (entry.command == command)
        {
            block = BuildOneBlock (entry, flagPrefix);
            break;
        }
    }

    return block;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildCommandBlocks
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

std::string DiskHelpPage::BuildCommandBlocks (char flagPrefix)
{
    std::string  text;



    for (const DiskHelpPage::DiskCommandHelp & entry : s_kDiskCommandHelp)
    {
        //  TWO BLANK LINES BETWEEN BLOCKS, not one. Every block ends on an
        //  indented example, and a single blank left the next heading looking
        //  like part of it: the page read as one long column rather than as
        //  eight commands a reader can scan and stop at.
        text += "\n\n";
        text += BuildOneBlock (entry, flagPrefix);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildOptionsHelp
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

std::string DiskHelpPage::BuildOptionsHelp (char flagPrefix)
{
    std::string  text = "Exit codes\n"
                        "----------\n";



    //  The statuses read the same in either prefix; the parameter
    //  is here because every other page builder takes one.
    (void) flagPrefix;

    text += kExitStatusHelpText;

    text += "\n";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildExampleHelp
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
//  The first trap is the header. `--dos-bin` and `put --load` each write a DOS
//  3.3 four-byte header, and a file carrying both loads its own header at the
//  load address -- where a `BRUN` executes it. The stale header's first byte is
//  the low half of the load address, which for anything in page $60 and below is
//  a BRK, so the machine lands in the monitor with no clue as to why.
//
//  The second is the greeting. A booting DOS 3.3 RUNs the name in its greeting
//  field, which runs an Applesoft BASIC or Integer BASIC program, so naming a
//  binary there is refused: RUN cannot start one, and the disk would boot into
//  nothing. The example places a one-line greeting that BRUNs the binary, which
//  is what actually closes the loop.
//
//  The emulator's own flags take the reader's prefix like everything else,
//  because Casso.exe parses through the same table. See
//  CommandLineHelp::BuildExampleCommands.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::BuildExampleHelp (char flagPrefix)
{
    std::string  lp = CommandLineHelp::LongPrefix (flagPrefix);
    std::string  sp = CommandLineHelp::ShortPrefix (flagPrefix);



    return CommandLineHelp::BuildExampleCommands (flagPrefix) +
        "\n"
        "  1. The 'create' command makes the disk that the rest of the loop writes to, and " + lp + "bootable copies an operating"
        " system onto it so the machine has something to start.\n"
        "  2. The " + sp + "o sets the name for the assembled output file.\n"
        "  3. We assemble with the default output format rather than " + lp + "dos-bin because 'put' writes the DOS 3.3 header itself"
        " from " + lp + "load, and a file that already carries one has its own header indicating where the program should load.\n"
        "  4. The last line is the emulator's own command line rather than this tool's: " + lp + "machine Apple2e opens an Apple //e, and"
        " " + lp + "disk1 puts the image in drive 1 for it to boot.\n"
        "  5. Greet.bas consists of a single Applesoft BASIC line, \"10 PRINT CHR$(4);\\\"BRUN PROG\\\"\", because a bootable DOS 3.3 volume"
        " executes its startup program using the RUN command. Using a binary file there is not allowed because RUN would fail, and the disk"
        " would not boot.\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::BuildHelpText
//
//  The whole page: what the subcommand is for, the commands it takes, each one
//  in detail, the statuses, and the loop.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::BuildHelpText (char flagPrefix, const std::string & banner)
{
    return (banner.empty() ? std::string() : banner + "\n")
         + "Usage:\n"
         + CommandLineHelp::UsageLineFor (CommandLineOptions::Subcommand::Disk) + "\n"
           "\n"
           "Disk commands:\n"
         + BuildSubcommandHelp (flagPrefix)
         + BuildCommandBlocks  (flagPrefix) + "\n\n"
         + BuildOptionsHelp    (flagPrefix) + "\n\n"
         + BuildExampleHelp    (flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpPage::DescribeAcceptedCommands
//
//  The command table read out in the order it is written, which is each command
//  followed by its own aliases -- so the suggestion a user is offered groups
//  the way the help does rather than alphabetically, where `cat` would land
//  three words from `catalog`.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskHelpPage::DescribeAcceptedCommands()
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
