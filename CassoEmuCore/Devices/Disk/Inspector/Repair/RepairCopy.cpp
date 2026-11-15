#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RepairCopy::GetCopyPath
//
//  "Name (repaired).dsk", then "Name (repaired 2).dsk" and up.
//
////////////////////////////////////////////////////////////////////////////////

std::string RepairCopy::GetCopyPath (IDiskFileIo & io, const std::string & imagePath)
{
    fs::path     image  = fs::path (TextEncoding::Utf8ToWide (imagePath));
    std::wstring stem   = image.stem().wstring();
    std::wstring ext    = image.extension().wstring();
    std::string  path   = TextEncoding::WideToUtf8 ((image.parent_path() / (stem + L" (repaired)" + ext)).wstring());
    int          number = 2;



    while (io.Exists (path))
    {
        path = TextEncoding::WideToUtf8 ((image.parent_path() / std::format (L"{} (repaired {}){}", stem, number, ext)).wstring());
        number++;
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairCopy::PlanFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RepairCopy::PlanFile (
    IDiskFileIo            & io,
    const std::string      & imagePath,
    const RepairOptions    & options,
    RepairPlan             & outPlan,
    std::wstring           & outReason)
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;
    DiskAnalysis  analysis;



    outPlan = RepairPlan();

    hr = io.ReadAllBytes (imagePath, bytes);
    CHRF (hr, outReason = L"The image couldn't be read.");

    hr = FileSystemCheck::Analyze (bytes, imagePath, analysis, outReason);
    CHR (hr);

    outPlan = DiskRepair::Plan (analysis, options);

Error:
    if (FAILED (hr))
    {
        outPlan.reason = outReason;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairCopy::MakeCopyBytes
//
//  The image loaded as its sectors, the plan written over them, and the
//  sectors saved back into the image's format. The tracks of a bit-stream
//  image that the repair doesn't change are copied as they are.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RepairCopy::MakeCopyBytes (
    const vector<Byte>   & imageBytes,
    const std::string    & imagePath,
    const RepairPlan     & plan,
    vector<Byte>         & outBytes,
    std::wstring         & outReason)
{
    static constexpr size_t  kVolumeBytes = 143360;



    HRESULT             hr       = S_OK;
    vector<Byte>        sectors;
    SectorDecodeReport  report;
    std::string         refusal;
    size_t              size     = 0;



    outBytes.clear();

    hr = VolumeImage::Load (imageBytes, imagePath, sectors, report);
    CHRF (hr, outReason = L"The image's sectors couldn't be read.");

    size = sectors.size();
    CBRF (size == kVolumeBytes, outReason = L"Repair is for 140 KB disks.");

    DiskRepair::Apply (plan, sectors);

    hr = VolumeImage::Save (imageBytes, imagePath, sectors, outBytes, refusal);
    CHRF (hr, outReason = TextEncoding::NarrowToWide (refusal));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RepairCopy::Write
//
//  A copy path that is taken fails rather than replacing the file. The path
//  requested is copied first, as outCopyPath may be the same string.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RepairCopy::Write (
    IDiskFileIo          & io,
    const std::string    & imagePath,
    const std::string    & copyPath,
    const RepairPlan     & plan,
    std::string          & outCopyPath,
    std::wstring         & outReason)
{
    HRESULT       hr       = S_OK;
    std::string   target   = copyPath;
    vector<Byte>  image;
    vector<Byte>  copy;
    bool          isTaken  = false;
    bool          canWrite = plan.canRepair;



    outCopyPath.clear();
    CBRF (canWrite, outReason = plan.reason);

    hr = io.ReadAllBytes (imagePath, image);
    CHRF (hr, outReason = L"The image couldn't be read.");

    hr = MakeCopyBytes (image, imagePath, plan, copy, outReason);
    CHR (hr);

    outCopyPath = target.empty() ? GetCopyPath (io, imagePath) : target;
    isTaken     = !target.empty() && io.Exists (target);
    CBRF (!isTaken, outReason = std::format (L"{} already exists.", fs::path (TextEncoding::Utf8ToWide (target)).filename().wstring()));

    hr = io.WriteAllBytes (outCopyPath, copy);
    CHRF (hr, outReason = L"The repaired copy couldn't be written.");

Error:
    if (FAILED (hr))
    {
        outCopyPath.clear();
    }

    return hr;
}
