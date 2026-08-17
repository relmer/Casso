#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





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
    //  Where the stock master lives on a developer machine. It is fetched by
    //  the GUI rather than committed, so this is a cache path, not a fixture.
    static constexpr const wchar_t *  kpszMasterCacheName = L"DOS 3.3 System Master.dsk";
    static constexpr const char *     kpszMasterRepoPath  = "Disks/Apple/dos33-master.dsk";

    //  How many screenfuls a command is allowed to print before this gives up.
    //  DOS 3.3's catalog pager needs one press on the master; the ProDOS
    //  fixture's startup program is a five-page slideshow.
    static constexpr int  kMaxPages = 12;

    //  A bare prompt row is the prompt glyph plus, on the frames it is lit, the
    //  cursor.
    static constexpr size_t  kPromptRowLength = 2;
    static constexpr char    kPromptGlyph     = ']';

    static constexpr Word  kBootRomEntry = 0xC600;
    static constexpr Word  kIntCxRomOff  = 0xC006;

    static constexpr int  kSlot6  = 6;
    static constexpr int  kDrive1 = 0;

    static std::vector<Byte>  ReadFileOrEmpty (const std::filesystem::path & full);
};
