#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateFailure
//
//  Why a check or an update did not succeed, for the message the user sees.
//  None on success. RestoreFailed means an install failed AND putting the
//  old copy back failed too, the one outcome that leaves the folder mixed.
//
////////////////////////////////////////////////////////////////////////////////

enum class UpdateFailure
{
    None,
    Network,
    RateLimited,
    BadData,
    NoAsset,
    DigestMismatch,
    NotOfficial,
    FolderNotWritable,
    OtherInstanceRunning,
    InstallFailed,
    RestoreFailed,
};
