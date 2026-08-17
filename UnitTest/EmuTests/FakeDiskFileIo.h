#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeDiskFileIo
//
//  An in-memory stand-in for the platform, so the whole disk-command path can
//  be exercised without touching a real file. Test isolation is
//  non-negotiable here, and the commit path's guarantees -- image unchanged
//  after every documented failure, no temporary left behind -- are only
//  assertable against a file table something can inspect.
//
//  Every failure the real platform can produce is inducible on demand, because
//  a refusal path that is never exercised is a refusal path nobody has checked.
//
//  What it CANNOT model, and must not be trusted for: the platform's own
//  translation of binary output. That happens below this interface, so the
//  bytes recorded here are always the bytes handed in. The reason binary
//  payload is routed through the seam at all is to put the mode decision in one
//  real implementation -- not because a fake could catch getting it wrong.
//
////////////////////////////////////////////////////////////////////////////////

class FakeDiskFileIo : public IDiskFileIo
{
public:
    //  The pretend filesystem. Public so a test can seed it and inspect it
    //  without a wrapper that would only restate what a map already says.
    std::unordered_map<std::string, vector<Byte>>  files;
    std::unordered_map<std::string, FileStamp>     stamps;

    //  What was written to the process's output, in order.
    vector<Byte>  standardOutput;

    //  Every path WriteAllBytes was asked to write, in order, whether or not
    //  the write was then made to fail. It is how a test learns the name a
    //  temporary was given without having to restate the derivation, which
    //  would make the test agree with the code by construction.
    vector<std::string>  writtenPaths;

    //  Induced failures. Each is off by default so a test opts into exactly the
    //  one it means to exercise.
    bool         failNextWrite       = false;
    bool         failNextReplace     = false;
    bool         reportHeldByOther   = false;

    //  WHICH failure the replace reports, because the reason is not
    //  interchangeable. A read-only target denies access and a caller must be
    //  told the image is write-protected; anything else is a different problem
    //  with a different fix, and a fake that answered one code for all of them
    //  would let a message naming the wrong cause pass.
    HRESULT      nextReplaceError    = E_FAIL;

    //  Set to make Stat report something different from what was recorded at
    //  read time, which is how the staleness check is exercised without a
    //  second process.
    bool         mutateStampOnNextStat = false;

    int          writeCount   = 0;
    int          replaceCount = 0;
    int          removeCount  = 0;

    HRESULT  ReadAllBytes (const std::string & path, vector<Byte> & outBytes) override
    {
        auto  found = files.find (path);

        if (found == files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outBytes = found->second;

        return S_OK;
    }

    //  A FAILED WRITE LEAVES A PARTIAL FILE BEHIND, deliberately, because that
    //  is what the platform does. A stream that fails partway has already
    //  created the file and put some of the bytes in it, so a substitute that
    //  fails cleanly would let a commit path that never cleans up after a
    //  failed write pass every test written against it -- the one shape those
    //  tests exist to catch.
    HRESULT  WriteAllBytes (const std::string & path, const vector<Byte> & bytes) override
    {
        writeCount++;
        writtenPaths.push_back (path);

        if (failNextWrite)
        {
            failNextWrite = false;

            files[path]  = vector<Byte> (bytes.begin(), bytes.begin() + (bytes.size() / 2));
            stamps[path] = FileStamp { files[path].size(), 1 };

            return E_FAIL;
        }

        files[path]  = bytes;
        stamps[path] = FileStamp { bytes.size(), 1 };

        return S_OK;
    }

    HRESULT  Stat (const std::string & path, FileStamp & outStamp) override
    {
        auto  found = stamps.find (path);

        if (found == stamps.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outStamp = found->second;

        if (mutateStampOnNextStat)
        {
            mutateStampOnNextStat = false;
            outStamp.modifiedUnix++;
        }

        return S_OK;
    }

    bool  Exists (const std::string & path) override
    {
        return files.find (path) != files.end();
    }

    HRESULT  Remove (const std::string & path) override
    {
        removeCount++;
        files.erase (path);
        stamps.erase (path);

        return S_OK;
    }

    HRESULT  ReplaceAtomically (const std::string & tempPath, const std::string & targetPath) override
    {
        auto  found = files.find (tempPath);

        replaceCount++;

        if (failNextReplace)
        {
            failNextReplace = false;
            return nextReplaceError;
        }

        if (found == files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        files[targetPath]  = found->second;
        stamps[targetPath] = FileStamp { found->second.size(), 2 };

        files.erase (tempPath);
        stamps.erase (tempPath);

        return S_OK;
    }

    bool  IsHeldByAnotherProcess (const std::string & path) override
    {
        UNREFERENCED_PARAMETER (path);

        return reportHeldByOther;
    }

    HRESULT  WritePayloadToStandardOutput (const vector<Byte> & bytes) override
    {
        standardOutput.insert (standardOutput.end(), bytes.begin(), bytes.end());

        return S_OK;
    }

    //  Convenience for the assertion that matters most on a failed write: the
    //  target must be byte-for-byte what it was, and nothing may be left over.
    bool  HasNoTemporaryFiles () const
    {
        for (const auto & entry : files)
        {
            if (entry.first.find (".tmp") != std::string::npos)
            {
                return false;
            }
        }

        return true;
    }
};
