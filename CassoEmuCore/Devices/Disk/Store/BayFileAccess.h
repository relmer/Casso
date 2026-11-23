#pragma once

#include "Pch.h"

#include "Devices/Disk/ImageIdentity.h"

struct DiskBay;
struct DiskStoreHooks;





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess
//
//  Every read, stat and write the store makes of a file, through the hooks
//  when a test has installed them and through the filesystem when it has
//  not.
//
////////////////////////////////////////////////////////////////////////////////

class BayFileAccess
{
public:
    explicit BayFileAccess (const DiskStoreHooks & hooks) : m_hooks (hooks) {}

    HRESULT        ReadImageFile         (const string & path, vector<Byte> & bytes) const;
    ImageIdentity  ReadIdentity          (const string & path) const;
    bool           DoesPathExist         (const string & path) const;

    //  Writes a whole image, through the flush sink when one is installed.
    HRESULT        WritePreserved        (const string & path, const vector<Byte> & bytes) const;

    //  Writes the mounted image to `outPath`, choosing a free preserved-copy
    //  path first when `outPath` is empty. On failure `outPath` is the path
    //  that failed.
    HRESULT        SaveLoadedImage       (DiskBay & bay, string & outPath) const;
    HRESULT        FindFreePreservedPath (const string & imagePath, string & outPath) const;

    //  Writes a lossless WOZ copy beside an image that could not be
    //  serialized to its source format.
    HRESULT        TryWriteRecoveryImage (DiskBay & bay, string & outPath) const;
    static string  MakeRecoveryPath      (const string & imagePath, int attempt);

private:
    static constexpr int  kMaxRecoveryNameAttempts = 64;

    const DiskStoreHooks  & m_hooks;
};
