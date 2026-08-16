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
};
