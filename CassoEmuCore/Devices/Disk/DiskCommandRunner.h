#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"
#include "CommandLineHelp.h"
#include "CommandLineOptions.h"
#include "CommitPlan.h"
#include "DiskCommandResult.h"
#include "DiskImageSession.h"
#include "IDiskFileIo.h"
#include "IVolume.h"
#include "SectorDecodeReport.h"

//  Forward-declared rather than included: pulling VolumeImage.h in would drag
//  DiskImage.h through this header and into the console project, which does not
//  share the core library's Pch conveniences.
enum class VolumeKind;

//  BlankDiskBuilder.h is kept out for the same reason, and the create and init
//  commands take everything from it by reference, so opaque declarations are all
//  this header needs. Including it built the library fine and broke the console
//  project, which is the failure mode the note above is about.
enum class DiskFormat;
enum class BlankDiskContents;
struct BlankDiskSpec;
struct BootPayload;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandResult
//
//  Everything one disk command produced, separated by where it goes. The runner
//  decides all of it; the executable only delivers it.
//
//  Payload is kept apart from output text because the two need different
//  treatment at the platform edge -- text wants the host's line-ending
//  translation and binary must not have it.
//





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner
//
//  Every decision a disk command makes: which command, which filesystem, whether
//  the result is safe to commit, what to say when it is not, and which exit
//  status the whole thing earns.
//
//  It reaches the host only through IDiskFileIo, which is what allows the
//  entire command path to be tested -- the test assembly does not link the
//  console executable, so anything placed there would be unreachable.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCommandRunner
{
public:


    explicit DiskCommandRunner (IDiskFileIo & fileIo);

    DiskCommandResult  Run (const CommandLineOptions & options);


    //  Puts a computed image over the target, or refuses and leaves the target
    //  byte-for-byte as it was.
    //
    //  Makes a new image file, or reformats one that is already there.
    //
    //  TWO COMMANDS BECAUSE THEY ANSWER TO DIFFERENT THINGS. `create` writes a
    //  file that did not exist and decides its container; `init` finds the
    //  container already decided by the file it was handed and only rewrites
    //  what is inside it. So `create` takes --type and `init` does not, and
    //  create refuses to write over something rather than quietly replacing a
    //  disk somebody still wanted.
    //
    //  One row of the container table: the word a reader types and the
    //  format it names.
    struct ContainerName
    {
        const char  *  name;
        DiskFormat     format;
    };

    //  What was just written, in the words the flags asked for it with.
    static std::string  DescribeNewDisk (const BlankDiskSpec & spec);

    //  A disk that starts a binary with no operating system on it. Its own
    //  path rather than a flag on the formatted one: there is no filesystem
    //  here to put the binary into.
    void  BuildDirectBoot (const CommandLineOptions & options,
                           DiskFormat                 format,
                           DiskCommandResult        & result);

    //
    //  Lays a file from the host into an image at a track and a DOS logical sector,
    //  with no filesystem involved.
    //
    //  FOR THE DISKS THAT HAVE NO FILESYSTEM TO PUT A FILE INTO. A demo that
    //  boots its own loader and reads fixed tracks is a real and common
    //  layout, and `put` cannot express it: there is no catalog to make an
    //  entry in and no allocator to ask for space. This writes exactly the
    //  bytes given, exactly where it is told.
    //
    void  RunSectorRead  (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunSectorWrite (const CommandLineOptions & options, DiskCommandResult & result);

    void  RunCreate (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunInit   (const CommandLineOptions & options, DiskCommandResult & result);

    //
    //  The container a new image should be written as, from --type or, failing
    //  that, from the name it is being given.
    //
    //  An unknown word is refused by NAME, with the ones that exist, rather
    //  than defaulting: a reader who typed --type 2mg means it, and silently
    //  handing them a .dsk is worse than saying no.
    //
    HRESULT  ResolveContainer (const CommandLineOptions & options,
                               DiskFormat               & outFormat,
                               DiskCommandResult        & result);

    //  What goes INSIDE it: a DOS 3.3 catalog, a ProDOS directory, or nothing.
    HRESULT  ResolveContents (const CommandLineOptions & options,
                              BlankDiskContents        & outContents,
                              DiskCommandResult        & result);

    //  --volume, which is a number under DOS 3.3 and a name under ProDOS.
    HRESULT  ResolveVolume (const CommandLineOptions & options,
                            BlankDiskSpec            & inOutSpec,
                            DiskCommandResult        & result);

    //  Reads the operating system a --bootable disk is copied FROM, or the
    //  binary a --boot disk starts instead of one.
    HRESULT  ResolveBoot (const CommandLineOptions & options,
                          BlankDiskSpec            & inOutSpec,
                          BootPayload              & outPayload,
                          DiskCommandResult        & result);

    //  Everything create and init share: build the bytes, then put them there.
    void  BuildAndWrite (const CommandLineOptions & options,
                         DiskFormat                 format,
                         bool                       overExisting,
                         DiskCommandResult        & result);



    //
    //  What each status means when a `disk` command returns it.
    //
    //  STATED UNDER `disk` BECAUSE IT IS DISK'S. One combined block used to
    //  stand at the top of the help claiming to describe all three modes, and
    //  it described none of them accurately: an assembly error is 2 under the
    //  assembler and 1 under `run`, and status 1 means "the output was written
    //  anyway" in one mode and "nothing ran" in another. What the three modes
    //  share is only the shape 0/1/2 -- not the meanings, which is exactly what
    //  a script branching on a number needs.
    //
    //  Beside the runner that assigns each one, so the wording and the
    //  behavior are edited together and a test can read both.
    //
    static constexpr const char *  kExitStatusHelpText =
        "    0  Success\n"
        "    1  Success, with a warning: a listing cut short by damage or a file"
        " delivered with unreadable sectors as zeros\n"
        "    2  Error, and nothing was done: a command or option refused, an image"
        " that cannot be read or holds no filesystem, a file that is not on the"
        " volume, a startup program a booting DOS 3.3 cannot run, or a write the"
        " volume or the host refused. The image is"
        " byte-for-byte as it was";


    //
    //  The disk section of the help, in the three pieces the usage text places
    //  separately: what the subcommand DOES, what its OPTIONS are, and the
    //  worked EXAMPLE that goes at the end.
    //
    //  Split because the surrounding help groups by kind and not by subcommand:
    //  a reader looking for options wants every subcommand's options together,
    //  and an example buried among them is one nobody reaches.
    //
    //  EVERY OPTION TAKES THE PREFIX THE READER ASKED FOR. Someone who typed
    //  `/?` is shown `/out`; someone who typed `--help` is shown `--out`. Both
    //  are accepted, so neither is a lie -- which is the whole reason the
    //  parser had to learn `/` before this could be honest.
    //
    //  Assembled here rather than beside the printing code for the reason
    //  kInUseHelpText already gives: the test assembly does not link the console
    //  executable, so help written there is help nothing can check.
    //
struct DiskCommandHelp
{
    //  WHICH COMMAND THIS ROW DESCRIBES, so a caller with a Command in hand
    //  can find its block without matching on the heading text.
    CommandLineOptions::DiskOptions::Command  command;

    const char *  forms;         // every accepted spelling, the plain one first
    const char *  summary;       // one line, for the list at the top
    const char *  grammar;       // where the operands go
    const char *  options;       // the options this command takes, or nothing
    const char *  discussion;    // what no option row can state, or nothing
    const char *  example;       // one line that does something real
};


    //  Every command the page describes, so a test can walk what the help
    //  claims instead of quoting sentences out of it.
    static std::span<const DiskCommandHelp>  GetCommandHelp();

    //  One command's block: its heading, grammar, options, and example.
    //
    //  A COMMAND MISSING AN OPERAND GETS THIS AND NOT THE WHOLE PAGE. The
    //  reader has already said which command they want; answering with eight
    //  of them is answering a question they did not ask, and the one they did
    //  ask is four screens down. Empty when the command has no block.
    static std::string  BuildOneBlock    (const DiskCommandHelp & entry, char flagPrefix);

    //  A paragraph with its runs of spaces collapsed to one.
    //
    //  THE WRAPPER READS A COLUMN BOUNDARY OUT OF THE TEXT: the last run of
    //  two or more spaces is where a continuation line is indented to, which
    //  is right for a two-column option row and wrong for a sentence. One
    //  stray double space mid-paragraph and every line after it hangs off that
    //  column instead of the left margin. Prose has no columns, so it has no
    //  business carrying a gutter.
    static std::string  AsProse           (const std::string & text);

    static std::string  BuildCommandHelp (CommandLineOptions::DiskOptions::Command command,
                                          char flagPrefix);

    //  What a command says when a required operand is not there: that
    //  command's usage, and then which parameter is missing.
    //
    //  A MEMBER, because the command it should print usage for is the one this
    //  runner is running, and the deepest caller that notices a missing image
    //  is OpenImage, which is handed a path and nothing else. Threading the
    //  command through every layer to reach it would put it in six signatures
    //  that have no other use for it.
    void  ReportMissingParameter (const std::string & parameter,
                                  DiskCommandResult & result) const;

    //  Every required operand this command did not get, in the order its
    //  grammar lists them.
    //
    //  ALL OF THEM, NOT THE FIRST ONE NOTICED. Each command used to check its
    //  own operands wherever it happened to need them, so `disk get` with
    //  nothing at all complained that <name> was missing and never mentioned
    //  <image> -- the operand that comes first, and that the reader would have
    //  had to supply before the complaint made sense.
    std::vector<std::string>  MissingParameters (const CommandLineOptions & options) const;

    //  The same report for a list of them.
    void  ReportMissingParameters (const std::vector<std::string> & parameters,
                                   DiskCommandResult & result) const;


    static std::string  ApplyPrefixes      (const std::string & text, char flagPrefix);

    //  The same substitution, against the prefix THIS run was asked for.
    //
    //  For diagnostics rather than for the page, and applied to the literal
    //  rather than to the finished sentence: a diagnostic carries file names
    //  and volume labels, and a sweep over the whole thing would rewrite a
    //  name that happened to hold %L.
    std::string         WithPrefix         (const std::string & text) const;
    static std::string  BuildSubcommandHelp (char flagPrefix);
    static std::string  BuildCommandBlocks  (char flagPrefix);
    static std::string  BuildOptionsHelp    (char flagPrefix);
    static std::string  BuildExampleHelp    (char flagPrefix);

    //  All three together, which is what a test reads when the question is
    //  about the disk help as a whole rather than about where a piece lands.
    //
    //  THE BANNER IS AN ARGUMENT for the reason CommandLineHelp::BuildGeneralHelp
    //  takes one: what the tool is called, which version this is and who holds
    //  the copyright are the EXECUTABLE's knowledge -- the version and the
    //  architecture come from its build -- and this page is assembled in the
    //  library. It heads every other help page, so it heads this one; it
    //  defaults to empty so a test asking what the disk page SAYS is not made to
    //  supply a version first.
    static std::string  BuildHelpText (char flagPrefix = '-', const std::string & banner = "");

    //  The banner the Help command prints above the page, handed over by whoever
    //  built the runner. Empty means no banner, which is what every caller that
    //  is not the console executable wants.
    void  SetBanner (const std::string & banner);

    //  Every command the grammar accepts, aliases included and comma-separated,
    //  for the refusal a word that is none of them earns.
    //
    //  Read from the parser's own table rather than retyped, because a retyped
    //  list is a list that goes stale: the aliases were added to the grammar
    //  and the refusal went on naming the five original commands, so a user who
    //  mistyped `catalgo` was told to try `list, get, put, delete, boot` and
    //  never learned that `catalog` was there all along.
    static std::string  DescribeAcceptedCommands();

private:
    void  RunList   (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunGet    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunPut    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunDelete (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBoot   (const CommandLineOptions & options, DiskCommandResult & result);


    //  Turns a volume layer's refusal into something a user can act on.
    //
    //  THIS IS WHAT KEEPS A PLATFORM CODE OUT OF THE OUTPUT. The volume layer
    //  answers in Win32 codes because they carry the right meanings without
    //  asserting on user input, but a number is not a reason -- the person
    //  reading it wants to know that their file is locked, not that something
    //  returned 0x80070005.
    static std::string  DescribeVolumeRefusal (HRESULT hr);

    //  Turns a tokenizer refusal into something a user can act on: which line,
    //  the line itself, and what is wrong with it. The line's own text is quoted
    //  because a number alone points at nothing in a file a person is reading.
    static std::string  DescribeListingRefusal (const char                   * leadIn,
                                                const ApplesoftListingError  & error);


    //  Whether a booting DOS 3.3 would actually run this file. Its boot command
    //  is RUN, so a greeting RUN does not understand is one in name only --
    //  measured on the stock master, where a binary named as the greeting
    //  leaves the disk booting and the program never running.
    static bool  IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                            const std::string    & name);

    //  Which type byte a placement uses: what the caller named, or the sensible
    //  one for the conversion they asked for.
    //  What a file's own bytes say it is, or 0 when they say nothing.
    //
    //  ONLY CONSULTED WHEN NOBODY NAMED A TYPE. A guess that overrode --type
    //  would be a tool arguing with its operator, and one that fired on a
    //  binary would file it where the guest cannot run it. Anything not
    //  positively recognized stays a binary, which is what a build loop
    //  produces and what the default has always been.
    static Byte     DetectFileType  (const std::vector<Byte> & bytes, VolumeKind kind);

    static HRESULT  ResolveFileType (const CommandLineOptions & options,
                                     VolumeKind                 kind,
                                     const std::vector<Byte>  & hostBytes,
                                     Byte                     & outType,
                                     DiskCommandResult        & result);

    //  The bytes a file from the host becomes on the disk, with whatever conversion was
    //  asked for already applied. Refuses a conversion this build cannot do.
    static HRESULT  BuildPutPayload (const CommandLineOptions  & options,
                                     VolumeKind                  kind,
                                     const std::vector<Byte>   & hostBytes,
                                     FilePayload               & outPayload,
                                     DiskCommandResult         & result);

    //  What the file is called on the disk: --as when given, otherwise the host
    //  file's own last component, which is the name the caller already chose.
    static std::string  OnDiskNameFor (const CommandLineOptions & options);






    //  Applies the requested character conversion, or refuses one this build
    //  cannot perform. A parsed-then-ignored flag is worse than an absent one.
    static HRESULT  ApplyEncoding (const CommandLineOptions & options,
                                   FilePayload              & payload,
                                   DiskCommandResult        & result);

    //  One line per catalog entry, in the shape the guest's own listing uses --
    //  with every column the volume records, since ProDOS fills eof= and aux=
    //  whether or not anybody asks and the widest row still fits in 80.
    static std::string  FormatDos33Entry  (const FileEntry & entry);
    static std::string  FormatProDosEntry (const FileEntry & entry);

    static char  Dos33TypeLetter (Byte type);



    //  The open/commit seam, reachable so the commit-path tests can
    //  drive it directly, which is why it was public on the runner
    //  before it was a class.
public:
    DiskImageSession &  Session ()  { return m_session; }

private:
    IDiskFileIo       & m_fileIo;
    DiskImageSession    m_session;
    std::string         m_banner;

    //  The command being run and the prefix it was typed with, recorded at the
    //  top of Run so a refusal deep inside can answer in the reader's own
    //  spelling and about the command they actually asked for.
    CommandLineOptions::DiskOptions::Command  m_command    =
        CommandLineOptions::DiskOptions::Command::None;
    char                                      m_flagPrefix = '-';

};
