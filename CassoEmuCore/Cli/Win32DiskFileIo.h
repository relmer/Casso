#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo
//
//  The platform half of the disk-command seam: syscalls and nothing else.
//
//  Every decision the disk commands make lives above this class, in
//  DiskCommandRunner, which the test assembly links and exercises. This one is
//  UNTESTABLE BY CONSTRUCTION -- the test project does not reference the console
//  executable -- so anything here that could be gotten wrong in an interesting
//  way is in the wrong place. What remains is opening, reading, writing and
//  replacing files.
//
//  With one exception that could not be moved: the translation mode of the
//  process's own output. See WritePayloadToStandardOutput.
//
////////////////////////////////////////////////////////////////////////////////

class Win32DiskFileIo : public IDiskFileIo
{
public:
    HRESULT  ReadAllBytes  (const std::string & path, std::vector<Byte> & outBytes) override;
    HRESULT  WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes) override;
    HRESULT  Stat          (const std::string & path, FileStamp & outStamp) override;

    bool     Exists        (const std::string & path) override;
    HRESULT  Remove        (const std::string & path) override;

    HRESULT  ReplaceAtomically (const std::string & tempPath,
                                const std::string & targetPath) override;

    bool     IsHeldByAnotherProcess (const std::string & path) override;

    HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> & bytes) override;

    //  THE INTERRUPTION SWITCH FOR THE COMMIT PATH, AND IT IS NOT IN THIS
    //  BINARY.
    //
    //  Arming it takes a define that no project configuration sets, so a stock
    //  build -- Debug as much as Release -- holds none of the code below and
    //  none of the strings that name it:
    //
    //      $env:CL = '/DCASSO_DIAG_DISK_ABORT'
    //      scripts\Build.ps1 -Configuration Debug -Target Rebuild
    //      $env:CL = $null
    //
    //  The environment variable of the same name then aims an armed binary at
    //  one of the two stages. It cannot arm anything by itself: with no define
    //  there is no code to read it. That asymmetry is the point -- an armed
    //  state that survives in a shell profile or a stray setx is one that
    //  eventually fires when nobody meant it to, and a guard that only kept the
    //  switch out of shipping builds did nothing about that.
    //
    //  _DEBUG stays in the condition as well, so the define arriving from an
    //  environment nobody audited still cannot reach a shipping binary.
    //
    //  See the note above the definitions for what the switch is for and why
    //  the substitute file interface does not cover the same ground.

#if defined(_DEBUG) && defined(CASSO_DIAG_DISK_ABORT)
private:

    //  True when the environment names this interruption point.
    static bool  IsAbortStageRequested (const wchar_t * stage);

    //  Leaves a genuinely short temporary on the filesystem, then stops.
    static void  AbortWithPartialFileIfRequested (std::ofstream            & file,
                                                  const std::vector<Byte>  & bytes);

    //  Stops with the temporary complete and the target not yet touched.
    static void  AbortBeforeReplaceIfRequested();

    //  No unwinding, no destructors, no cleanup -- which is what a crash is.
    static void  AbortProcessNow (const wchar_t * stage);

    static constexpr const wchar_t *  kpszAbortVariable    = L"CASSO_DIAG_DISK_ABORT";
    static constexpr const wchar_t *  kpszStageDuringWrite = L"during-write";
    static constexpr const wchar_t *  kpszStageBeforeSwap  = L"before-replace";
#endif
};
