#include "Pch.h"

#include "DiskCheck/BootSounds.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootSounds::MakeBooted
//
//  C, E and G quickly, then the C above them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> BootSounds::MakeBooted()
{
    static constexpr Note  s_kNotes[] =
    {
        {  523.25,  523.25, 0.09, 0.6, true, 0.0 },
        {  659.25,  659.25, 0.09, 0.6, true, 0.0 },
        {  783.99,  783.99, 0.09, 0.6, true, 0.0 },
        { 1046.50, 1046.50, 0.40, 0.6, true, 6.0 },
    };
    std::vector<int16_t>   samples;



    for (const Note & note : s_kNotes)
    {
        AddNote (note, samples);
    }

    return MakeWav (samples);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootSounds::MakeDidNotBoot
//
//  Four notes a half step apart, each sliding down a little, and the last
//  one long and sliding further.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> BootSounds::MakeDidNotBoot()
{
    static constexpr Note  s_kNotes[] =
    {
        { 233.08, 226.0, 0.38, 1.0, false, 0.0 },
        { 220.00, 213.0, 0.38, 1.0, false, 0.0 },
        { 207.65, 201.0, 0.38, 1.0, false, 0.0 },
        { 196.00, 170.0, 1.20, 1.0, false, 5.0 },
    };
    std::vector<int16_t>   samples;



    for (const Note & note : s_kNotes)
    {
        AddNote (note, samples);
    }

    return MakeWav (samples);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootSounds::AddNote
//
//  A sum of harmonics, each at one over its number, with the higher ones
//  fading over the note, as a mute closing over a horn's bell.
//
////////////////////////////////////////////////////////////////////////////////

void BootSounds::AddNote (const Note & note, std::vector<int16_t> & ioSamples)
{
    static constexpr double  kTwoPi = 6.283185307179586;
    int                      count  = (int) (note.seconds * kSampleRate);
    double                   phase  = 0.0;



    for (int i = 0; i < count; i++)
    {
        double  seconds = (double) i / kSampleRate;
        double  along   = (double) i / count;
        double  vibrato = 1.0 + s_kVibratoDeep * along * sin (kTwoPi * note.vibratoHz * seconds);
        double  hertz   = (note.startHz + (note.endHz - note.startHz) * along) * vibrato;
        double  attack  = (std::min) (1.0, seconds / s_kAttack);
        double  release = (std::min) (1.0, (note.seconds - seconds) / s_kRelease);
        double  bright  = note.brightness * (1.0 - 0.75 * along);
        double  value   = 0.0;

        phase += kTwoPi * hertz / kSampleRate;

        for (int harmonic = 1; harmonic <= s_kHarmonics; harmonic += note.oddOnly ? 2 : 1)
        {
            value += sin (phase * harmonic) / harmonic * (harmonic == 1 ? 1.0 : bright);
        }

        ioSamples.push_back ((int16_t) (value * attack * release * s_kLevel * 32767.0));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootSounds::MakeWav
//
//  One channel of 16-bit samples behind the 44 bytes of a WAV file's header.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> BootSounds::MakeWav (const std::vector<int16_t> & samples)
{
    static constexpr uint32_t  kHeaderBytes = 44;
    uint32_t                   dataBytes    = (uint32_t) (samples.size() * sizeof (int16_t));
    std::vector<Byte>          wav;
    auto                       text         = [&wav] (const char * four) { wav.insert (wav.end(), four, four + 4); };
    auto                       number       = [&wav] (uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; i++)
        {
            wav.push_back ((Byte) (value >> (8 * i)));
        }
    };



    wav.reserve (kHeaderBytes + dataBytes);

    text ("RIFF");
    number (kHeaderBytes - 8 + dataBytes, 4);
    text ("WAVE");
    text ("fmt ");
    number (16, 4);
    number (1, 2);                                  // PCM
    number (1, 2);                                  // one channel
    number ((uint32_t) kSampleRate, 4);
    number ((uint32_t) kSampleRate * 2, 4);         // bytes a second
    number (2, 2);                                  // bytes a sample
    number (16, 2);                                 // bits a sample
    text ("data");
    number (dataBytes, 4);

    for (int16_t sample : samples)
    {
        number ((uint16_t) sample, 2);
    }

    return wav;
}
