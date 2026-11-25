#pragma once

#include "Pch.h"

class MountDiagnosis;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMessages
//
//  The text of every notice about a mount or a save that did not happen.
//  Pure: each one is built from a path and a reason.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreMessages
{
public:
    static wstring  FormatMountFailureMessage    (const string         & path,
                                                  const MountDiagnosis & diagnosis);
    static wstring  FormatDamagedImageMessage    (const string & path, bool hasDamagedTracks);
    static wstring  FormatFlushLossMessage       (const string & path,
                                                  HRESULT        reason,
                                                  const string & recoveryPath = string());
    static wstring  FormatDiscardedWritesMessage (const string & path,
                                                  const string & attemptedPath,
                                                  HRESULT        reason);
    static wstring  FormatSalvageFailedMessage   (const string & path);
    static wstring  FormatExternalChangeMessage  (const string & path);
};
