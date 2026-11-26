#include "Pch.h"

#include "CassoLink/Session/CaptureStamp.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureStamp::AddToWoz
//
//  Loads the WOZ, builds its stamp with the standard decode settings and the
//  capture's facts, and puts the stamp in the WOZ's bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CaptureStamp::AddToWoz (
    const std::vector<CassoLinkCapture::Position>  &  positions,
    const CassoLinkCapture::Result                &  result,
    const std::string                             &  written,
    std::vector<Byte>                             &  ioWoz)
{
    HRESULT                          hr     = S_OK;
    DiskImage                        image;
    std::shared_ptr<const DiskCopy>  copy;
    CaptureReport                    report;
    FormatStamp                      stamp;



    hr = WozLoader::Load (ioWoz, image);
    CHRA (hr);

    copy = DiskCopy::MakeFromImage (image, 0, std::string(), ioWoz.size(), false);

    for (const CassoLinkCapture::Position & position : positions)
    {
        report.facts.reads += (int) position.reads.size();
    }

    report.facts.hasRevolution    = result.revolutionCycles > 0.0;
    report.facts.revolutionCycles = result.revolutionCycles;
    report.facts.isSynchronized   = result.synchronized;
    report.facts.betweenRead      = result.betweenRead;
    report.facts.betweenTotal     = result.betweenTotal;
    report.weak                   = result.weak;
    report.unstable               = result.unstable;
    report.streams                = result.streams;
    report.replaced               = result.replaced;

    FormatStampBuilder::Build (*copy, DecodeSettings::MakeStandard(), StampSource::Capture, &report, written, stamp);

    hr = FormatStampChunk::SetStampInFile (ioWoz, stamp);
    CHRA (hr);

Error:
    return hr;
}
