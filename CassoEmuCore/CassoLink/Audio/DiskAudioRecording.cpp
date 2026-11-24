#include "Pch.h"

#include "CassoLink/Audio/DiskAudioRecording.h"
#include "CassoLink/Audio/CassetteByteStream.h"
#include "CassoLink/Audio/CassetteCodec.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioRecording::Build
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskAudioRecording::Build (const std::vector<Byte> & sectors, uint32_t sampleRate, std::vector<float> & outSamples, std::string & outMonitorCommand)
{
    constexpr size_t     kTracks   = 35;
    HRESULT              hr        = S_OK;
    std::vector<double>  record;
    std::vector<float>   samples;
    std::vector<Byte>    module;
    std::vector<Byte>    packet;
    std::vector<Byte>    stream;
    DiskImage            image;
    bool                 thirteen  = sectors.size() == ThirteenSectorCodec::kImageByteSize;
    bool                 sixteen   = sectors.size() == NibblizationLayer::kImageByteSize;
    const Byte           check[]   = { 0xFF, 0xFF };



    outSamples.clear();
    CBREx (thirteen || sixteen, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    hr = thirteen ? ThirteenSectorCodec::Nibblize (sectors, CassoLinkThirteenSector::kVolume, image)
                  : NibblizationLayer::Nibblize (sectors, DiskFormat::Dsk, image);
    CHR (hr);

    hr = LinkAudioBootstrap::BuildCassetteLinkRecord (record, outMonitorCommand);
    CHR (hr);

    hr = LinkProgram::BuildThirteenSectorModule (module, LinkChip::Cassette);
    CHR (hr);

    CassetteCodec::Render (record, sampleRate, CassetteByteStream::kAmplitude, samples);
    outSamples.insert (outSamples.end(), samples.begin(), samples.end());
    AppendSilence (kAfterProgramMs, sampleRate, outSamples);

    for (size_t at = 0; at < module.size(); at += CassoLinkCodec::kModuleChunk)
    {
        size_t             count = std::min (CassoLinkCodec::kModuleChunk, module.size() - at);
        std::vector<Byte>  chunk = { (Byte) (at & 0xFF), (Byte) (at >> 8) };

        chunk.insert (chunk.end(), module.begin() + (ptrdiff_t) at, module.begin() + (ptrdiff_t) (at + count));

        packet.clear();

        hr = CassoLinkCodec::Encode (CassoLinkCodec::kLoadModule, chunk, packet);
        CHR (hr);

        AppendBurst (packet, kRequestToneMs, sampleRate, outSamples);
        AppendSilence (kAfterRequestMs, sampleRate, outSamples);
    }

    packet.clear();

    hr = CassoLinkCodec::Encode (CassoLinkCodec::kLoadModule, check, packet);
    CHR (hr);

    AppendBurst (packet, kRequestToneMs, sampleRate, outSamples);
    AppendSilence (kAfterRequestMs, sampleRate, outSamples);

    for (size_t track = 0; track < kTracks; track++)
    {
        std::vector<Byte>  request;
        Word               crc     = 0;

        CassoLinkThirteenSector::MakeStream (image, image.ResolveWholeTrack ((int) track), stream);

        request = { kDrive, (Byte) track, (Byte) (stream.size() & 0xFF), (Byte) (stream.size() >> 8) };

        packet.clear();

        hr = CassoLinkCodec::Encode (CassoLinkCodec::kWriteNibbleTrack, request, packet);
        CHR (hr);

        AppendBurst (packet, kRequestToneMs, sampleRate, outSamples);

        crc = CassoLinkCodec::ComputeCrc (stream);
        stream.push_back ((Byte) (crc & 0xFF));
        stream.push_back ((Byte) (crc >> 8));

        packet.clear();

        hr = CassoLinkCodec::Encode (CassoLinkCodec::kNibbleData, stream, packet);
        CHR (hr);

        //  A stream longer than a burst goes in pieces, each after
        //  CassetteByteStream's gap and tone, while the Apple waits for the
        //  rest.
        for (size_t piece = 0; piece < packet.size(); piece += CassetteCodec::kMaxBurst)
        {
            size_t  count = std::min (CassetteCodec::kMaxBurst, packet.size() - piece);

            if (piece > 0)
            {
                AppendSilence (CassetteByteStream::kBurstGapMs, sampleRate, outSamples);
            }

            AppendBurst (std::span<const Byte> (packet).subspan (piece, count),
                         piece > 0 ? CassetteByteStream::kNextToneHalves * CassetteCodec::kToneHalfUs / 1000.0 : track == 0 ? kFirstStreamToneMs : kStreamToneMs,
                         sampleRate, outSamples);
        }

        AppendSilence (kAfterTrackMs, sampleRate, outSamples);
    }

    AppendSilence (kTailMs, sampleRate, outSamples);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioRecording::AppendBurst
//
////////////////////////////////////////////////////////////////////////////////

void DiskAudioRecording::AppendBurst (std::span<const Byte> bytes, double toneMs, uint32_t sampleRate, std::vector<float> & inOutSamples)
{
    constexpr double     kUsPerMs = 1000.0;
    std::vector<double>  halves;
    std::vector<float>   samples;



    CassetteCodec::AppendBurst (bytes, halves, (int) (toneMs * kUsPerMs / CassetteCodec::kToneHalfUs));
    CassetteCodec::Render (halves, sampleRate, CassetteByteStream::kAmplitude, samples);

    inOutSamples.insert (inOutSamples.end(), samples.begin(), samples.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioRecording::AppendSilence
//
////////////////////////////////////////////////////////////////////////////////

void DiskAudioRecording::AppendSilence (double ms, uint32_t sampleRate, std::vector<float> & inOutSamples)
{
    constexpr double  kMsPerSecond = 1000.0;



    inOutSamples.insert (inOutSamples.end(), (size_t) (ms * sampleRate / kMsPerSecond), 0.0f);
}
