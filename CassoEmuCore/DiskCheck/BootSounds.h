#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootSounds
//
//  The two sounds for an image's boot result, each computed as a WAV file
//  in memory: a short rising fanfare for an image that booted, and four
//  falling notes of a muted trombone for one that did not.
//
////////////////////////////////////////////////////////////////////////////////

class BootSounds
{
public:
    static constexpr int  kSampleRate = 22050;

    static std::vector<Byte>  MakeBooted     ();
    static std::vector<Byte>  MakeDidNotBoot ();

private:
    //  A note: its pitch at its start and end, its length, and how much of
    //  each harmonic it has at its start and at its end.
    struct Note
    {
        double  startHz    = 0.0;
        double  endHz      = 0.0;
        double  seconds    = 0.0;
        double  brightness = 1.0;      // the harmonics' share at the note's start; they fade toward its end
        bool    oddOnly    = false;    // a hollow tone, of the odd harmonics alone
        double  vibratoHz  = 0.0;
    };

    static constexpr int     s_kHarmonics   = 8;
    static constexpr double  s_kLevel       = 0.22;
    static constexpr double  s_kAttack      = 0.012;     // seconds
    static constexpr double  s_kRelease     = 0.04;
    static constexpr double  s_kVibratoDeep = 0.012;     // of the pitch

    static void               AddNote (const Note & note, std::vector<int16_t> & ioSamples);
    static std::vector<Byte>  MakeWav (const std::vector<int16_t> & samples);
};
