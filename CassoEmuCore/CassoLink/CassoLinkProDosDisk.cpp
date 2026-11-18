#include "Pch.h"

#include "CassoLink/CassoLinkProDosDisk.h"
#include "CassoLink/LinkProgram.h"
#include "ApplesoftTokenizer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDisk::Make
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkProDosDisk::Make (const std::vector<Byte> & usersDisk, const std::string & usersPath, DWORD baud, std::vector<Byte> & outImage)
{
    HRESULT                hr      = S_OK;
    std::vector<Byte>      sectors;
    std::vector<Byte>      image (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>      installer;
    std::vector<Byte>      startup;
    std::string            errors;
    SectorDecodeReport     report;
    ApplesoftListingError  listingError;
    Byte                   control = 0;
    bool                   known   = LinkProgram::GetSpeedControl (baud, control);
    bool                   sized   = false;



    outImage.clear();
    CBREx (known, E_INVALIDARG);

    hr = VolumeImage::Load (usersDisk, usersPath, sectors, report);
    CHR (hr);

    sized = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ProDosSkeleton::Write (image, kVolumeName);
    CHR (hr);

    hr = ProDosSkeleton::InstallBoot (image, sectors);
    CHR (hr);

    hr = LinkProgram::BuildVirtualDrive (control, installer, errors);
    CHR (hr);

    hr = ProDosFileWriter::WriteFile (image, kInstallerName, kBinaryType, LinkProgram::kInstallerAddress, installer);
    CHR (hr);

    hr = ApplesoftTokenizer::Tokenize (std::format ("10 PRINT CHR$(4);\"BRUN {}\"\n", kInstallerName), startup, listingError);
    CHR (hr);

    hr = ProDosFileWriter::WriteFile (image, "STARTUP", kBasicType, ApplesoftTokenizer::kProgramBase, startup);
    CHR (hr);

    outImage = std::move (image);

Error:
    return hr;
}
