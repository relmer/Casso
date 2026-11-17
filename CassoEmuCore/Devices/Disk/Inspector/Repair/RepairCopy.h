#pragma once

#include "Pch.h"

class IDiskFileIo;
struct DiskAnalysis;
struct RepairOptions;
struct RepairPlan;





////////////////////////////////////////////////////////////////////////////////
//
//  RepairCopy
//
//  A repaired copy of a disk image beside it: " (repaired)" before the
//  extension, and a number after it when a file of that name exists, in
//  the image's format, with the plan's sectors written over the image's.
//  The image itself isn't written.
//
////////////////////////////////////////////////////////////////////////////////

class RepairCopy
{
public:
    static std::string  GetCopyPath (IDiskFileIo & io, const std::string & imagePath);

    //  The image file analyzed as saved, which a drive's copy with changes
    //  not yet saved may differ from; a file that can't be read or analyzed
    //  fails with the reason.
    static HRESULT  AnalyzeFile (IDiskFileIo          & io,
                                 const std::string    & imagePath,
                                 DiskAnalysis         & outAnalysis,
                                 std::wstring         & outReason);

    //  The plan for the image file as saved.
    static HRESULT  PlanFile (IDiskFileIo            & io,
                              const std::string      & imagePath,
                              const RepairOptions    & options,
                              RepairPlan             & outPlan,
                              std::wstring           & outReason);

    static HRESULT  MakeCopyBytes (const vector<Byte>   & imageBytes,
                                   const std::string    & imagePath,
                                   const RepairPlan     & plan,
                                   vector<Byte>         & outBytes,
                                   std::wstring         & outReason);

    //  The copy written to copyPath, or beside the image when copyPath is
    //  empty; outCopyPath is the path written.
    static HRESULT  Write (IDiskFileIo          & io,
                           const std::string    & imagePath,
                           const std::string    & copyPath,
                           const RepairPlan     & plan,
                           std::string          & outCopyPath,
                           std::wstring         & outReason);
};
