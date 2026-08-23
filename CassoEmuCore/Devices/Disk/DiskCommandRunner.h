#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"
#include "CommandLineHelp.h"
#include "CommandLineOptions.h"
#include "CommitPlan.h"
#include "IDiskFileIo.h"
#include "IVolume.h"
#include "SectorDecodeReport.h"

//  Forward-declared rather than included: pulling VolumeImage.h in would drag
//  DiskImage.h through this header and into the console project, which does not
//  share the core library's Pch conveniences.
enum class VolumeKind;

//  BlankDiskBuilder.h is kept out for the same reason, and the create and init
//  verbs take everything from it by reference, so opaque declarations are all
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

struct DiskCommandResult
{
    //  0 clean, 1 succeeded with complaints, 2 produced no output. The same
    //  meanings the assembler and run subcommands already assign.
    int           exitStatus  = 0;

    //  Set when what was WRONG was the command line rather than the disk:
    //  a verb this grammar does not have, an operand it needed and did not
    //  get, an encoding nobody offers. The edge prints the disk page ahead
    //  of the diagnostic for these and not for the rest, because "PROG is
    //  not on this volume" is answered by a listing and not by a grammar.
    bool          badCommandLine = false;

    std::string        output;   // stdout, text
    std::string        diagnostics;   // stderr, always
    std::vector<Byte>  payload;   // stdout, binary
    bool               hasPayload  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner
//
//  Every decision a disk command makes: which verb, which filesystem, whether
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
    //
    //  One image, read and identified, together with what a later commit over
    //  it will need.
    //
    //  The stamp is here rather than beside the commit because it has to be
    //  taken at READ time to mean anything -- a stamp read just before writing
    //  would agree with itself and prove nothing. Keeping the two in one object
    //  makes it awkward to write a commit that forgot to record one.
    //
    struct OpenedImage
    {
        std::string         imagePath;

        //  The container's own bytes, kept because rendering an edited volume
        //  back needs them: a bit-stream image preserves every track the edit
        //  did not touch verbatim, and it can only do that from the file it
        //  came from. Re-reading here instead would let the two halves of the
        //  write disagree about what the image held.
        std::vector<Byte>   fileBytes;

        std::vector<Byte>   sectors;
        VolumeKind          kind          = VolumeKind {};
        SectorDecodeReport  report;
        FileStamp           stamp;

        //  False when the platform could not tell us the size and time. A
        //  commit refuses rather than proceeding without the check, because a
        //  guarantee quietly not applied is indistinguishable from one applied.
        bool                stampRecorded = false;

        //
        //  Set when the target does not exist yet, which is `create` and
        //  nothing else.
        //
        //  THE FRESHNESS CHECK IS THE WRONG QUESTION FOR A FILE THAT IS NOT
        //  THERE. It asks whether the image changed between being read and
        //  being written, and a file nobody has read cannot have: with no
        //  stamp to compare, the commit refused every new disk with "could
        //  not be checked for changes". Everything else about the write is
        //  unchanged, the in-use probe included, so a name another program
        //  is already holding is still refused.
        bool                isNew         = false;
    };

    explicit DiskCommandRunner (IDiskFileIo & fileIo);

    DiskCommandResult  Run (const CommandLineOptions & options);

    //  Loads the image, identifies its filesystem and records its stamp, or
    //  explains why not.
    HRESULT  OpenImage (const std::string  & imagePath,
                        OpenedImage        & outOpened,
                        DiskCommandResult  & result);

    //  Puts a computed image over the target, or refuses and leaves the target
    //  byte-for-byte as it was.
    //
    //  Makes a new image file, or reformats one that is already there.
    //
    //  TWO VERBS BECAUSE THEY ANSWER TO DIFFERENT THINGS. `create` writes a
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

    HRESULT  CommitImage (const OpenedImage        & opened,
                          const std::vector<Byte>  & newImageBytes,
                          DiskCommandResult        & result);

    //  Exit-status vocabulary, named so call sites do not write the numbers.
    static constexpr int  kClean          = 0;
    static constexpr int  kWithComplaints = 1;
    static constexpr int  kNoOutput       = 2;

    //
    //  What a commit says when another program is holding the image.
    //
    //  It lives here, beside the code that performs the probe, so the claim and
    //  the capability cannot drift apart -- and so a test can read it, since the
    //  console executable is not linked by the test assembly.
    //
    //  IT IS A DIAGNOSTIC RATHER THAN HELP TEXT, which is the whole change. The
    //  help used to carry a paragraph describing the probe; a user meets the
    //  probe by having a write refused, and the refusal names the image and
    //  says what to do about it. Documenting an error message in the usage text
    //  is documenting the wrong surface.
    //
    static constexpr const char *  kInUseRefusalText =
        "is open in another program. Close it and try again";

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
        "    0  The command was carried out\n"
        "    1  Carried out, with something worth saying: a listing cut short by\n"
        "       damage, a file delivered with unreadable sectors as zeros, or a\n"
        "       startup program a booting DOS 3.3 will not actually run.\n"
        "    2  Nothing was done: a verb or an option that was refused, an image\n"
        "       that cannot be read or holds no filesystem, a file that is not on\n"
        "       the volume, or a write the volume or the host refused. The image is\n"
        "       byte-for-byte as it was.";

    //  The sentence a listing gives when neither filesystem is there. Named so
    //  a test asserts on the wording a user reads rather than a paraphrase.
    static constexpr const char *  kNoFilesystemText =
        "does not have a DOS or ProDOS file system";

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
    static std::string  BuildSubcommandHelp (char flagPrefix);
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

    //  The banner the Help verb prints above the page, handed over by whoever
    //  built the runner. Empty means no banner, which is what every caller that
    //  is not the console executable wants.
    void  SetBanner (const std::string & banner);

    //  Every verb the grammar accepts, aliases included and comma-separated,
    //  for the refusal a word that is none of them earns.
    //
    //  Read from the parser's own table rather than retyped, because a retyped
    //  list is a list that goes stale: the aliases were added to the grammar
    //  and the refusal went on naming the five original verbs, so a user who
    //  mistyped `catalgo` was told to try `list, get, put, delete, boot` and
    //  never learned that `catalog` was there all along.
    static std::string  DescribeAcceptedVerbs();

private:
    void  RunList   (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunGet    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunPut    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunDelete (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBoot   (const CommandLineOptions & options, DiskCommandResult & result);

    //  Renders an edited sector buffer back into the container it came from and
    //  puts it where the old one was. One helper because the two orderings it
    //  imposes -- render first, then commit -- are what keep a refused write
    //  from ever reaching the target.
    HRESULT  SaveAndCommit (const OpenedImage        & opened,
                            const std::vector<Byte>  & editedSectors,
                            DiskCommandResult        & result);

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

    //  The same job for the atomic replace, which is where write protection
    //  enforced by the HOST file's read-only attribute finally surfaces -- the
    //  volume layer never sees it, because nothing about the image's contents
    //  says the file may not be written.
    static std::string  DescribeReplaceFailure (HRESULT hr);

    //  Whether a booting DOS 3.3 would actually run this file. Its boot command
    //  is RUN, so a greeting RUN does not understand is one in name only --
    //  measured on the stock master, where a binary named as the greeting
    //  leaves the disk booting and the program never running.
    static bool  IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                            const std::string    & name);

    //  Which type byte a placement uses: what the caller named, or the sensible
    //  one for the conversion they asked for.
    static HRESULT  ResolveFileType (const CommandLineOptions & options,
                                     VolumeKind                 kind,
                                     Byte                     & outType,
                                     DiskCommandResult        & result);

    //  The bytes a host file becomes on the disk, with whatever conversion was
    //  asked for already applied. Refuses a conversion this build cannot do.
    static HRESULT  BuildPutPayload (const CommandLineOptions  & options,
                                     VolumeKind                  kind,
                                     const std::vector<Byte>   & hostBytes,
                                     FilePayload               & outPayload,
                                     DiskCommandResult         & result);

    //  What the file is called on the disk: --as when given, otherwise the host
    //  file's own last component, which is the name the caller already chose.
    static std::string  OnDiskNameFor (const CommandLineOptions & options);

    //
    //  Everything an image still says about itself once neither filesystem
    //  recognizes it.
    //
    //  A FUNCTIONAL, BOOTABLE DISK IS THE COMMON CASE HERE, not an exotic one.
    //  Most commercial Apple II software never used a filesystem at all: it
    //  booted its own loader off track 0 and read its data by track and sector.
    //  Answering only "no filesystem" describes twelve of fourteen real disks
    //  as though nothing could be learned from them, when the image is holding
    //  its title, its publisher, how much of its surface is formatted and
    //  whether it boots.
    //
    static std::string  DescribeUnrecognizedImage (const OpenedImage & opened);

    //  What the container itself records: for WOZ, the INFO and META chunks and
    //  how much of the surface the track map claims.
    static std::string  DescribeWozChunks (const std::vector<Byte> & fileBytes);

    //  What the decoded surface shows: geometry, how the tracks read, and
    //  whether the first sector a boot reads carries anything.
    static std::string  DescribeSurface (const OpenedImage & opened);

    //  One `  label   value` line, or nothing at all when the value is empty --
    //  so a caller can offer every field it knows about without also deciding
    //  which ones this image answered.
    static std::string  DetailLine (const char * label, const std::string & value);

    //  Names the image and the reason, sets the no-output status, and returns
    //  nothing -- so a refusal path cannot report one without the other.
    static void  RefuseCommit (const std::string  & imagePath,
                               const std::string  & reason,
                               DiskCommandResult  & result);

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

    //  Names the image, the file, and the reason -- in that order, because a
    //  script's user reads the first line and needs to know which disk.
    static std::string  Failure (const std::string & imagePath,
                                 const std::string & fileName,
                                 const std::string & reason);

    IDiskFileIo &  m_fileIo;
    std::string    m_banner;

    //  What separates this runner's temporaries from any other invocation's.
    //  Taken once, at construction, so every commit this runner performs is
    //  distinguishable from every commit anybody else performs.
    uint64_t       m_invocationTag = CommitPlan::NextInvocationTag();
};
