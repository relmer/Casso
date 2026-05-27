#include "Pch.h"

#include "AutoMountResolver.h"

#include "Config/IFileSystem.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Resolve
//
////////////////////////////////////////////////////////////////////////////////

AutoMountResolver::Decision AutoMountResolver::Resolve (
    const std::wstring & rememberedPath,
    IFileSystem        & fs)
{
    Decision  decision = { Action::LeaveEmpty, std::wstring() };



    if (rememberedPath.empty())
    {
        return decision;
    }

    if (fs.Exists (rememberedPath))
    {
        decision.action = Action::Mount;
        decision.path   = rememberedPath;
    }
    else
    {
        decision.action = Action::ClearStaleEntry;
        decision.path   = rememberedPath;
    }

    return decision;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotForPersistence
//
//  Pass-through helper that exists so tests can verify the round-trip
//  persistence payload shape independently of shell plumbing.
//
////////////////////////////////////////////////////////////////////////////////

std::array<std::wstring, 2> AutoMountResolver::SnapshotForPersistence (
    const std::wstring & drive0Path,
    const std::wstring & drive1Path)
{
    std::array<std::wstring, 2>  out;



    out[0] = drive0Path;
    out[1] = drive1Path;

    return out;
}
