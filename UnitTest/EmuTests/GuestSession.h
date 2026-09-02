#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"
#include "Devices/Disk/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GuestSession
//
//  Driving a real 6502 off an image this tool produced, and reading what it
//  made of it.
//
//  The gates that use this settle questions our own reader structurally cannot:
//  a file written through a wrong understanding and read back through the same
//  wrong understanding comes back perfectly. The witnesses here are the guest's
//  own -- what DOS 3.3 and ProDOS print, and what they leave in memory.
//
//  NOTHING HERE STARTS A PROCESSOR BEFORE THE CHEAP QUESTIONS ARE ASKED. A
//  guest handed an image it cannot make sense of executes whatever it managed
//  to read, and this build writes a look-back for every illegal opcode it
//  reaches. The ring bounds what is RETAINED, not what is EMITTED: one boot of
//  a mis-ordered image has written over three gigabytes and taken the test host
//  with it. So Mount decodes the container the way the DRIVE will and refuses
//  to start anything when the boot ROM's own first read is not there, and the
//  gates above it ask whatever else they can afford to ask first.
//
//  ONE COPY OF THE PAGING RULES, ON PURPOSE. DOS 3.3's catalog pager is
//  indistinguishable from a prompt to everything MachineIdle looks at -- drive
//  stopped, screen still, a short bottom row -- and a caller that stops there
//  types its next command into the pager, where the first character is
//  swallowed as the keypress the pager was waiting for. That produced a BLOAD
//  arriving as LOAD, with the payload never reaching memory and the screen
//  still looking plausible. A second copy of these helpers is a second place
//  for that to be got wrong.
//
////////////////////////////////////////////////////////////////////////////////

class GuestSession
{
public:
    //  The stock DOS 3.3 master, or a FAILED test.
    //
    //  Not a skip. The master is fetched rather than committed and does not
    //  travel between machines, so a case needing it can find it absent -- and
    //  a guest-visible gate that never started a guest has checked nothing,
    //  while a skip is indistinguishable in the output from a case that ran.
    static std::vector<Byte>  RequireDos33Master();

    //  The stock ProDOS Users Disk, or a FAILED test, on the same grounds.
    static std::vector<Byte>  RequireProDosUsersDisk();

    //  Mounts the bytes in slot 6 drive 1 and parks the processor on the boot
    //  ROM's entry, having executed nothing.
    //
    //  Separate from MountAndBoot so a caller that wants to COUNT what the
    //  boot costs can drive the processor itself. Idling to quiet is the
    //  wrong instrument for that: it stops when the machine settles, which is
    //  a different moment from when the guest's own code was reached, and it
    //  is the same call for both sides of a comparison so neither side's
    //  answer would be about the disk.
    static void  Mount        (HeadlessHost             & host,
                               EmulatorCore             & core,
                               const std::vector<Byte>  & bytes);

    //  Mounts the bytes in slot 6 drive 1 and runs the boot ROM until the
    //  machine settles or the ceiling is spent.
    static void  MountAndBoot (HeadlessHost             & host,
                               EmulatorCore             & core,
                               const std::vector<Byte>  & bytes);

    //  Boots and pages through whatever the disk's own startup program prints,
    //  leaving the guest at a prompt that will accept a command.
    static void  BootToPrompt (HeadlessHost             & host,
                               EmulatorCore             & core,
                               const std::vector<Byte>  & bytes);

    //  The container decoded the way the DRIVE reads it: laid down as physical
    //  nibbles and recovered through the hardware interleave, rather than
    //  through the path that wrote it.
    static std::vector<Byte>  DecodeThroughTheDrive (const std::vector<Byte> & bytes);

    //  The same question of a container already loaded into a drive, which is
    //  the only form a WOZ can be asked in -- its bit streams come off the file
    //  itself and there is no sector buffer to nibblize.
    static std::vector<Byte>  DecodeThroughTheDrive (const DiskImage     & image,
                                                     SectorDecodeReport  & outReport);

    //  THE BOUND ON A FAILING BOOT GATE, in three strengths. All three are the
    //  same question -- what does the drive read off this? -- asked through the
    //  one decoder above, and a caller takes the strongest its material lets it
    //  answer:
    //
    //    a sector image mounted from bytes    -> the bytes come back
    //    a container built as bits (a WOZ)    -> the ROM's own first read works
    //    a copy-protected container           -> the tracks are there at all
    //
    //  Ordered by what INDEPENDENT thing there is to compare against, not by
    //  taste. The first has the caller's own buffer, which is why it is the one
    //  to prefer: a decoder checked against its own inverse agrees with itself
    //  while presenting something no guest would recognize.


    //  The drive must present exactly the bytes that were handed to it.
    //
    //  THE STRONGEST OF THE THREE AND THE CHEAPEST TO BE SURE OF. A sector one
    //  place out of step still carries a valid header, so the controller ROM
    //  reads it, believes it, and jumps into another sector's data -- the exact
    //  shape that has twice made a guest execute the disk and write an
    //  instruction look-back per illegal opcode until the run died. Nothing
    //  weaker than a byte comparison sees it.
    static void  AssertTheDrivePresentsWhatWasMounted (const DiskImage          & image,
                                                       const std::vector<Byte>  & mounted,
                                                       const wchar_t            * what);

    //  The boot ROM's own first read, asked before any processor is started.
    //
    //  For a container with no sector buffer behind it to compare against. The
    //  ROM reads track 0's first sector into $0800 and jumps to $0801 whatever
    //  that sector holds; an image whose first sector the drive cannot recover,
    //  or which holds nothing at all, guarantees a 6502 executing data.
    //
    //  Deliberately not a filesystem question. A disk built by this tool need
    //  not carry a volume -- the direct-boot images do not -- and a gate that
    //  demanded one could not be applied to them.
    //
    //  FOR STANDARD MEDIA ONLY, and that is a limit of the reader rather than
    //  of the idea. Measured on this branch: `Denibblize` recovers ZERO sectors
    //  from track 0 of Choplifter, Karateka and Lode Runner, all three of which
    //  the controller ROM boots perfectly. It abandons a track at the first
    //  sector whose data field it cannot locate, and on a protection that
    //  rewrites data prologues the first header the scan meets from bit zero is
    //  usually one of those -- so the whole revolution is spent and the track
    //  comes back empty. Ask a protected disk the question below instead.
    static void  AssertTheDriveCanReadTheBootSector (const DiskImage  & image,
                                                     const wchar_t    * what);

    //  As much of the same question as a COPY-PROTECTED disk can be asked: the
    //  drive is holding at least as many written tracks as the caller is about
    //  to require the head to visit, and track 0 -- where the ROM starts -- has
    //  address fields on it at all.
    //
    //  Weaker than its neighbor on purpose. It is answered off the loaded bit
    //  streams and the per-track outcome rather than off recovered sectors,
    //  which is the only layer a protected disk and this decoder agree on.
    static void  AssertTheDriveHoldsWrittenTracks (const DiskImage  & image,
                                                   int                leastTracks,
                                                   const wchar_t    * what);

    //  Every non-blank row showing now, appended to whatever is already there.
    static void  CollectRows (EmulatorCore & core, std::vector<std::string> & outRows);

    //  True when the bottom-most non-blank row is a bare BASIC prompt.
    //
    //  THE PROMPT GLYPH IS THE LOAD-BEARING PART. The catalog pager also parks
    //  the machine on a one-glyph row -- the cursor by itself -- with the drive
    //  stopped and the screen still, which is everything MachineIdle looks at.
    static bool  IsAtBarePrompt (EmulatorCore & core);

    //  Presses Return until the guest is back at a bare prompt, collecting
    //  every row it displayed on the way. Rows repeat across screenfuls, which
    //  is why callers ask what a matching row SAYS rather than how many.
    static bool  TryPageToPrompt (EmulatorCore & core, std::vector<std::string> & outRows);

    //  Types a line and hands back everything the guest printed in answer.
    static std::vector<std::string>  TypeAndCollect (EmulatorCore & core, const std::string & line);

    static bool  AnyRowIs       (const std::vector<std::string> & rows, const std::string & wanted);
    static bool  AnyRowContains (const std::vector<std::string> & rows, const std::string & needle);

    //  Every row mentioning `needle` must be `expected`, and there must be one.
    //
    //  ASKING ONLY WHETHER THE SCREEN CONTAINS THE NAME IS NOT ENOUGH. A bare
    //  search is satisfied by the echo of the command that produced the row, by
    //  a neighboring file whose name merely begins the same way, and -- the
    //  case these gates exist for -- by a line carrying the wrong type or the
    //  wrong size.
    static void  AssertTheOnlyRowsMentioning (const std::vector<std::string>  & rows,
                                              const std::string               & needle,
                                              const std::string               & expected);

    //  What the guest holds at an address, for comparing against what a
    //  placement said would be loaded there.
    static std::vector<Byte>  GuestBytesAt (EmulatorCore & core, Word address, size_t count);

    static std::string  TrimTrailingBlanks (const std::string & row);

    static constexpr size_t  kImageBytes = 143360;

    //  Ceilings, not targets. Both are spent through MachineIdle, which stops
    //  as soon as the guest settles; they bound how long a machine reading an
    //  image it cannot make sense of is allowed to spin.
    static constexpr uint64_t  kBootCycles = 40'000'000ULL;
    static constexpr uint64_t  kLineCycles = 20'000'000ULL;

private:
    //  Where the stock masters live on a developer machine. They are fetched by
    //  the GUI rather than committed, so these are cache paths, not fixtures.
    //  The cache names are the ones the emulator saves each image under.
    static constexpr const wchar_t *  kpszMasterCacheName = L"DOS 3.3 System Master.dsk";
    static constexpr const char *     kpszMasterRepoPath  = "Disks/Apple/dos33-master.dsk";
    static constexpr const wchar_t *  kpszProDosCacheName = L"ProDOS Users Disk.dsk";
    static constexpr const char *     kpszProDosRepoPath  = "Disks/Apple/prodos-users.dsk";

    //  How far up from the working directory the repo-path search climbs. The
    //  test host's directory is not fixed, so the walk covers a binary run from
    //  the tree, from an output directory under it, or from a worktree.
    static constexpr int  kMaxAncestorWalk = 10;

    //  How many screenfuls a command is allowed to print before this gives up.
    //  DOS 3.3's catalog pager needs one press on the master; the ProDOS
    //  fixture's startup program is a five-page slideshow.
    static constexpr int  kMaxPages = 12;

    //  A bare prompt row is the prompt glyph plus, on the frames it is lit, the
    //  cursor.
    static constexpr size_t  kPromptRowLength = 2;
    static constexpr char    kPromptGlyph     = ']';

    //  What the controller ROM reads, in the decoded buffer's own terms.
    static constexpr int  kBootTrack  = 0;
    static constexpr int  kBootSector = 0;

    static constexpr Word  kBootRomEntry = 0xC600;
    static constexpr Word  kIntCxRomOff  = 0xC006;

    static constexpr int  kSlot6  = 6;
    static constexpr int  kDrive1 = 0;

    static std::vector<Byte>  ReadFileOrEmpty (const std::filesystem::path & full);

    //  The two places a stock image can be, tried in order. Empty when neither
    //  has it; the Require* callers turn that into a FAILED test.
    static std::vector<Byte>  FindStockImage  (const char    * repoPath,
                                               const wchar_t * cacheName);
};
