#include "Pch.h"

#include "Devices/Tape/OggVorbisCodec.h"

#define OV_EXCLUDE_STATIC_CALLBACKS

#include "vorbis/vorbisenc.h"
#include "vorbis/vorbisfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::IsOgg
//
////////////////////////////////////////////////////////////////////////////////

bool OggVorbisCodec::IsOgg (std::span<const Byte> bytes)
{
    return bytes.size() >= kCapturePatternLength && memcmp (bytes.data(), "OggS", kCapturePatternLength) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::ReadCallback
//
////////////////////////////////////////////////////////////////////////////////

size_t OggVorbisCodec::ReadCallback (void * buffer, size_t size, size_t count, void * source)
{
    Reader  * reader    = static_cast<Reader *> (source);
    size_t    wanted    = size * count;
    size_t    available = reader->bytes.size() - reader->at;
    size_t    copied    = std::min (wanted, available);



    memcpy (buffer, reader->bytes.data() + reader->at, copied);
    reader->at += copied;

    return (size == 0) ? 0 : copied / size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::SeekCallback
//
////////////////////////////////////////////////////////////////////////////////

int OggVorbisCodec::SeekCallback (void * source, int64_t offset, int whence)
{
    Reader   * reader = static_cast<Reader *> (source);
    int64_t    base   = (whence == SEEK_CUR) ? (int64_t) reader->at : (whence == SEEK_END) ? (int64_t) reader->bytes.size() : 0;
    int64_t    target = base + offset;
    bool       inside = target >= 0 && target <= (int64_t) reader->bytes.size();



    if (inside)
    {
        reader->at = (size_t) target;
    }

    return inside ? 0 : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::TellCallback
//
////////////////////////////////////////////////////////////////////////////////

long OggVorbisCodec::TellCallback (void * source)
{
    return (long) static_cast<Reader *> (source)->at;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::Decode
//
//  Every channel is averaged into one, at the recording's sample rate.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OggVorbisCodec::Decode (std::span<const Byte> bytes, TapeAudio & audio, std::string & error)
{
    HRESULT          hr        = S_OK;
    Reader           reader    = { bytes, 0 };
    OggVorbis_File   file      = {};
    ov_callbacks     callbacks = { &ReadCallback, &SeekCallback, nullptr, &TellCallback };
    bool             isOpen    = false;
    vorbis_info    * info      = nullptr;
    float         ** pcm       = nullptr;
    int              section   = 0;
    long             frames    = 0;
    bool             inRange   = false;



    audio = TapeAudio();

    isOpen = ov_open_callbacks (&reader, &file, nullptr, 0, callbacks) == 0;
    CBRFEx (isOpen, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The recording isn't an Ogg Vorbis stream.");

    info    = ov_info (&file, -1);
    inRange = info != nullptr && info->channels >= 1 && info->rate >= (long) TapeAudio::kMinSampleRate && info->rate <= (long) TapeAudio::kMaxSampleRate;
    CBRFEx (inRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The recording's sample rate or channels can't be read as a tape.");

    audio.sampleRate = (uint32_t) info->rate;

    do
    {
        frames = ov_read_float (&file, &pcm, kDecodeFrames, &section);

        for (long frame = 0; frame < frames; frame++)
        {
            float  sum = 0.0f;

            for (int channel = 0; channel < info->channels; channel++)
            {
                sum += pcm[channel][frame];
            }

            audio.samples.push_back (sum / (float) info->channels);
        }
    }
    while (frames > 0);

    CBRFEx (frames == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), error = "The recording is damaged.");

Error:
    if (isOpen)
    {
        ov_clear (&file);
    }

    if (FAILED (hr))
    {
        audio = TapeAudio();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OggVorbisCodec::Encode
//
//  Mono, variable bit rate at kQuality, at the recording's sample rate.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OggVorbisCodec::Encode (const TapeAudio & audio, std::vector<Byte> & bytes, std::string & error)
{
    HRESULT           hr        = S_OK;
    vorbis_info       info      = {};
    vorbis_comment    comment   = {};
    vorbis_dsp_state  dsp       = {};
    vorbis_block      block     = {};
    ogg_stream_state  stream    = {};
    ogg_packet        identity  = {};
    ogg_packet        comments  = {};
    ogg_packet        codebooks = {};
    ogg_packet        packet    = {};
    ogg_page          page      = {};
    bool              hasDsp    = false;
    bool              isSetUp   = false;
    size_t            at        = 0;
    bool              done      = false;



    bytes.clear();

    vorbis_info_init    (&info);
    vorbis_comment_init (&comment);

    isSetUp = vorbis_encode_init_vbr (&info, 1, (long) audio.sampleRate, kQuality) == 0;
    CBRF (isSetUp, error = "The Ogg Vorbis encoder doesn't take this sample rate.");

    vorbis_comment_add_tag (&comment, "ENCODER", "Casso");
    vorbis_analysis_init   (&dsp, &info);
    vorbis_block_init      (&dsp, &block);
    ogg_stream_init        (&stream, kStreamSerial);
    hasDsp = true;

    vorbis_analysis_headerout (&dsp, &comment, &identity, &comments, &codebooks);
    ogg_stream_packetin (&stream, &identity);
    ogg_stream_packetin (&stream, &comments);
    ogg_stream_packetin (&stream, &codebooks);

    while (ogg_stream_flush (&stream, &page) != 0)
    {
        bytes.insert (bytes.end(), page.header, page.header + page.header_len);
        bytes.insert (bytes.end(), page.body,   page.body   + page.body_len);
    }

    //  Each pass passes the next frames to libvorbis, or none to end the
    //  stream, then writes every page that completes.
    while (!done)
    {
        size_t  count = std::min ((size_t) kEncodeFrames, audio.samples.size() - at);

        if (count > 0)
        {
            float  ** buffer = vorbis_analysis_buffer (&dsp, (int) count);

            memcpy (buffer[0], audio.samples.data() + at, count * sizeof (float));
            at += count;
        }

        vorbis_analysis_wrote (&dsp, (int) count);

        while (vorbis_analysis_blockout (&dsp, &block) == 1)
        {
            vorbis_analysis       (&block, nullptr);
            vorbis_bitrate_addblock (&block);

            while (vorbis_bitrate_flushpacket (&dsp, &packet) != 0)
            {
                ogg_stream_packetin (&stream, &packet);

                while (ogg_stream_pageout (&stream, &page) != 0)
                {
                    bytes.insert (bytes.end(), page.header, page.header + page.header_len);
                    bytes.insert (bytes.end(), page.body,   page.body   + page.body_len);

                    done = done || ogg_page_eos (&page) != 0;
                }
            }
        }

        done = done || count == 0;
    }

    while (ogg_stream_flush (&stream, &page) != 0)
    {
        bytes.insert (bytes.end(), page.header, page.header + page.header_len);
        bytes.insert (bytes.end(), page.body,   page.body   + page.body_len);
    }

Error:
    if (hasDsp)
    {
        ogg_stream_clear  (&stream);
        vorbis_block_clear (&block);
        vorbis_dsp_clear  (&dsp);
    }

    vorbis_comment_clear (&comment);
    vorbis_info_clear    (&info);

    if (FAILED (hr))
    {
        bytes.clear();
    }

    return hr;
}
