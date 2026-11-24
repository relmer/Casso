#include "Pch.h"

#include "CassoLink/CassoLinkSosDisk.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosStartup.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSosDisk::Make
//
//  A new ProDOS volume, the user's SOS loader and system files on it, then
//  SOS.INTERP. SOS reads a ProDOS volume, and the loader goes in block 0, as
//  the new volume has no Apple II loader there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSosDisk::Make (
    const std::vector<Byte>  & usersDisk,
    const std::string        & usersPath,
    DWORD                      baud,
    std::vector<Byte>        & outImage,
    std::string              & outError)
{
    HRESULT             hr      = S_OK;
    std::vector<Byte>   sectors;
    std::vector<Byte>   image (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>   written;
    FilePayload         interpreter;
    std::string         errors;
    SectorDecodeReport  report;
    Byte                control = 0;
    bool                known   = LinkProgram::GetSpeedControl (baud, control);
    bool                sized   = false;



    outImage.clear();
    outError.clear();

    CBREx (known, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hr = LinkProgram::BuildSosInterpreter (control, interpreter.bytes, errors);
    CHR (hr);

    hr = VolumeImage::Load (usersDisk, usersPath, sectors, report);
    CHR (hr);

    sized = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ProDosSkeleton::Write (image, kVolumeName);
    CHR (hr);

    hr = ProDosStartup::InstallSosKernel (image, sectors, outError);
    CHR (hr);

    interpreter.type = kSosSystemType;

    hr = ProDosVolume (image).Write (FilePath::Parse (kInterpreterName), interpreter, written);
    CHR (hr);

    outImage = std::move (written);

Error:
    return hr;
}
