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
    bool         failNextMetadata    = false;
    bool         failNextFlush       = false;
    bool         failNextRename      = false;
    bool         reportHeldByOther   = false;

    //  WHICH failure the replace reports, because the reason is not
    //  interchangeable. A read-only target denies access and a caller must be
    //  told the image is write-protected; anything else is a different problem
    //  with a different fix, and a fake that answered one code for all of them
    //  would let a message naming the wrong cause pass.
    HRESULT      nextReplaceError    = E_FAIL;

    //  WHICH failure the write reports, for the same reason.
    HRESULT      nextWriteError      = E_FAIL;

    //  Every write to a path that contains this fails with nextWriteError,
    //  for as long as it is set: a folder that does not accept one kind of
    //  file, write after write.
    std::string  failWritesContaining;

    //  Set to make Stat report something different from what was recorded at
    //  read time, which is how the staleness check is exercised without a
    //  second process.
    bool         mutateStampOnNextStat = false;

    int          writeCount    = 0;
    int          replaceCount  = 0;
    int          removeCount   = 0;
    int          metadataCount = 0;
    int          flushCount    = 0;
    int          renameCount   = 0;

    //  The paths flushed, in order, so a test can see the temporary was made
    //  durable before it became the target.
    vector<std::string>  flushedPaths;

    //  What is on the storage, as opposed to in the cache. A write sets the
    //  file's size there with zeros for its bytes, as NTFS logs a file's size
    //  before its data; a flush copies the bytes there. SimulatePowerLoss puts the files
    //  back to it.
    std::unordered_map<std::string, vector<Byte>>  durable;

    //  A file's attributes and permissions. A write starts a file with none;
    //  CopyFileMetadata copies them; a replace or rename moves them.
    struct Metadata
    {
        uint32_t     attributes = 0;
        std::string  permissions;

        bool  operator== (const Metadata &) const = default;
    };

    std::unordered_map<std::string, Metadata>  metadata;

    //  Every write, metadata copy, flush, replace and rename, in order.
    vector<std::string>  operations;

    //  The power goes off: every file is what the storage has.
    void  SimulatePowerLoss ()
    {
        files = durable;
    }

    //  A file as it is after a flush, so it survives a power loss.
    void  SeedDurable (const std::string & path, const vector<Byte> & bytes)
    {
        files[path]   = bytes;
        durable[path] = bytes;
        stamps[path]  = FileStamp { bytes.size(), 1 };
    }

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
        operations.push_back ("Write");
        metadata[path] = Metadata();

        if (failNextWrite || (!failWritesContaining.empty() && path.find (failWritesContaining) != std::string::npos))
        {
            failNextWrite = false;

            files[path]   = vector<Byte> (bytes.begin(), bytes.begin() + (bytes.size() / 2));
            stamps[path]  = FileStamp { files[path].size(), 1 };
            durable[path] = vector<Byte> (files[path].size(), 0);

            return nextWriteError;
        }

        files[path]   = bytes;
        stamps[path]  = FileStamp { bytes.size(), 1 };
        durable[path] = vector<Byte> (bytes.size(), 0);

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
        durable.erase (path);
        metadata.erase (path);

        return S_OK;
    }

    HRESULT  ReplaceAtomically (const std::string & tempPath, const std::string & targetPath) override
    {
        auto  found = files.find (tempPath);

        replaceCount++;
        operations.push_back ("Replace");

        if (failNextReplace)
        {
            failNextReplace = false;
            return nextReplaceError;
        }

        if (found == files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        Move (tempPath, targetPath);

        return S_OK;
    }

    HRESULT  FlushToStorage (const std::string & path) override
    {
        flushCount++;
        flushedPaths.push_back (path);
        operations.push_back ("Flush");

        if (failNextFlush)
        {
            failNextFlush = false;
            return HRESULT_FROM_WIN32 (ERROR_IO_DEVICE);
        }

        if (!Exists (path))
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        durable[path] = files[path];

        return S_OK;
    }

    HRESULT  CopyFileMetadata (const std::string & fromPath, const std::string & toPath) override
    {
        metadataCount++;
        operations.push_back ("CopyMetadata");

        if (failNextMetadata)
        {
            failNextMetadata = false;
            return E_ACCESSDENIED;
        }

        if (!Exists (fromPath) || !Exists (toPath))
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        metadata[toPath] = metadata[fromPath];

        return S_OK;
    }

    HRESULT  RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath) override
    {
        auto  found = files.find (tempPath);

        renameCount++;
        operations.push_back ("Rename");

        if (failNextRename)
        {
            failNextRename = false;
            return HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);
        }

        if (Exists (targetPath))
        {
            return HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS);
        }

        if (found == files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        Move (tempPath, targetPath);

        return S_OK;
    }

    //  A replace or rename: the file's bytes, its storage copy and its metadata
    //  move to the target name together.
    void  Move (const std::string & tempPath, const std::string & targetPath)
    {
        files[targetPath]    = files[tempPath];
        stamps[targetPath]   = FileStamp { files[tempPath].size(), 2 };
        durable[targetPath]  = durable[tempPath];
        metadata[targetPath] = metadata[tempPath];

        files.erase (tempPath);
        stamps.erase (tempPath);
        durable.erase (tempPath);
        metadata.erase (tempPath);
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
