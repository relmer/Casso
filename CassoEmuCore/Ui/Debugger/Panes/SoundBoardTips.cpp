#include "Pch.h"

#include "Ui/Debugger/Panes/SoundBoardTips.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/SoundBoardLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetChannelName
//
////////////////////////////////////////////////////////////////////////////////

wchar_t SoundBoardTips::GetChannelName (int channel)
{
    return (wchar_t) (L'A' + channel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetHex (int value, int digits)
{
    return (digits == 4) ? std::format (L"${:04X}", value) : std::format (L"${:02X}", value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetNote (int channel, const DiagnosticsSoundChannel & state)
{
    return std::format (L"Channel {}'s note: {}, {}, with the cents above or below the nearest note.\n"
                        L"The tone period is {} (R{} and R{}), and the frequency is the AY's clock divided by 16 times the period.",
                        GetChannelName (channel), SoundBoardLayout::GetNoteText (state.frequencyHz), SoundBoardLayout::GetFrequencyText (state.frequencyHz),
                        GetHex (state.period, 4), channel * 2, channel * 2 + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetSource
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetSource (int channel, bool isNoise, const DiagnosticsSoundChannel & state, Byte mixer)
{
    int  bit = channel + (isNoise ? 3 : 0);



    if (isNoise)
    {
        return std::format (L"Noise: the noise generator's random bits are {} for channel {}.\n"
                            L"A clear bit {} of the mixer, R7 ({}), turns them on.",
                            state.isNoiseOn ? L"on" : L"off", GetChannelName (channel), bit, GetHex (mixer, 2));
    }

    return std::format (L"Tone: channel {}'s square wave is {}.\n"
                        L"A clear bit {} of the mixer, R7 ({}), turns it on. With noise on too, the noise gates the tone.",
                        GetChannelName (channel), state.isToneOn ? L"on" : L"off", bit, GetHex (mixer, 2));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetLevel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetLevel (int channel, const DiagnosticsSoundChannel & state, Byte amplitude)
{
    return std::format (L"Channel {}'s amplitude: {} of 15, from R{} ({}).\n"
                        L"With bit 4 of R{} set, the level is the envelope's, marked env, and it moves along the envelope's picture. "
                        L"Each step is about 3 dB; the faint segment is the peak a moment ago.",
                        GetChannelName (channel), state.level, 8 + channel, GetHex (amplitude, 2), 8 + channel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetScope
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetScope (int channel)
{
    return std::format (L"Channel {}'s output over the last few milliseconds, lined up on a rising edge.\n"
                        L"A tone is always a square wave, as tall as the amplitude; noise is random steps. The AY doesn't make any other waveform.",
                        GetChannelName (channel));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetEnvelopePicture
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetEnvelopePicture (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"The envelope's pattern for R13 = {}: the level from 0 to 15 over four ramps, from the write of R13 at the left.\n"
                        L"The line and the dot are at the level now, {}{}. A channel follows the envelope while bit 4 of its amplitude register is set.",
                        GetHex (psg.envelopeRegister, 2), psg.envelopeLevel, psg.isEnvelopeHeld ? L", stopped at the end of the pattern" : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetRamp
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetRamp (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"Ramp: {:.1f} ms, the time one ramp of the envelope takes, from 0 to 15 or 15 to 0.\n"
                        L"It's 256 times the envelope period, {} (R11 and R12), divided by the AY's clock.",
                        psg.envelopeRampMs, GetHex (psg.envelopePeriod, 4));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetShape
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetShape (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"R13, the envelope's pattern: {} ({}). A write of R13 starts the envelope over at its first ramp.\n"
                        L"Its four bits are CONT, ATT, ALT and HOLD, lit beside it where set.",
                        GetHex (psg.envelopeRegister, 2), psg.envelopeRegister);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetNoise
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetNoise (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"Noise: {}, how often the noise generator picks a new random bit.\n"
                        L"It's the AY's clock divided by 16 times the noise period, {} (R6). One generator serves all three channels.",
                        SoundBoardLayout::GetFrequencyText (psg.noiseHz), GetHex (psg.noisePeriod, 2));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetShapeBit
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetShapeBit (int bit, Byte shape)
{
    static constexpr const wchar_t * kText[] =
    {
        L"CONT, continue (R13 bit 3): with it clear, the level drops to 0 after the first ramp and stays there, whatever ALT and HOLD are.",
        L"ATT, attack (R13 bit 2): with it set, the first ramp rises from 0 to 15; with it clear, it falls from 15 to 0.",
        L"ALT, alternate (R13 bit 1): with it set, each ramp runs the other way from the last, a triangle; with it clear, each starts over, a sawtooth.",
        L"HOLD (R13 bit 0): with it set, the level stays where the first ramp ends, or at the other end with ALT set.",
    };
    bool  isSet = (shape & (0x08 >> bit)) != 0;



    if (bit < 0 || bit >= (int) std::size (kText))
    {
        return std::wstring();
    }

    return std::format (L"{}\nIt is {} now.", kText[bit], isSet ? L"set" : L"clear");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetRegister
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetRegister (int reg, Byte value)
{
    static constexpr const wchar_t * kText[kRegisterCount] =
    {
        L"channel A's tone period, its low 8 bits",
        L"channel A's tone period, its high 4 bits",
        L"channel B's tone period, its low 8 bits",
        L"channel B's tone period, its high 4 bits",
        L"channel C's tone period, its low 8 bits",
        L"channel C's tone period, its high 4 bits",
        L"the noise period, 5 bits",
        L"the mixer: bits 0 to 2 turn the tone off for A, B and C, bits 3 to 5 the noise, and bits 6 and 7 set the I/O ports' direction",
        L"channel A's amplitude: bits 0 to 3 the level, and bit 4 set to follow the envelope",
        L"channel B's amplitude: bits 0 to 3 the level, and bit 4 set to follow the envelope",
        L"channel C's amplitude: bits 0 to 3 the level, and bit 4 set to follow the envelope",
        L"the envelope period, its low 8 bits",
        L"the envelope period, its high 8 bits",
        L"the envelope's pattern: CONT, ATT, ALT and HOLD; a write starts the envelope over",
        L"I/O port A, which the Mockingboard doesn't use",
        L"I/O port B, which the Mockingboard doesn't use",
    };



    if (reg < 0 || reg >= kRegisterCount)
    {
        return std::wstring();
    }

    return std::format (L"R{} = {} ({}): {}.", reg, GetHex (value, 2), value, kText[reg]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetVia
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetVia (const std::wstring & label, const DiagnosticsSoundHalf & half)
{
    const DiagnosticsSoundVia  & via = half.via;



    if (label == L"Bus")
    {
        return std::format (L"Bus: {}, the signal ORB's lines put on the AY's BC1 and BDIR now.\n"
                            L"Latch address: the byte on port A selects a register. Write: the byte goes into it. Read: the AY puts it on port A. Inactive: none of them.",
                            TextEncoding::NarrowToWide (half.busState));
    }

    if (label == L"Writes")
    {
        return std::format (L"Writes: {}, the bytes written into this AY's registers since the machine started.", half.psg.writeCount);
    }

    if (label == L"Last")
    {
        return L"Last: the register written last and the byte written, as R7=$38 for $38 into R7.";
    }

    if (label == L"ORA")
    {
        return std::format (L"ORA, output register A: {}, the byte the 6522 puts on the AY's data lines, a register number or a value for one.", GetHex (via.ora, 2));
    }

    if (label == L"ORB")
    {
        return std::format (L"ORB, output register B: {}. Bits 0 to 2 drive the AY's BC1, BDIR and RESET lines, which select inactive, latch address, write or read.", GetHex (via.orb, 2));
    }

    if (label == L"DDRA" || label == L"DDRB")
    {
        return std::format (L"{}, data direction register {}: {}. Each set bit makes that line of the port an output; $FF makes them all outputs, as the AY's lines need.",
                            label, label.back(), GetHex (label == L"DDRA" ? via.ddra : via.ddrb, 2));
    }

    if (label == L"ACR")
    {
        return std::format (L"ACR, auxiliary control: {}. Bits 6 and 7 set timer 1's mode; $40 makes it free-running, so it reloads from its latch and "
                            L"interrupts at a steady rate, the tick a music player counts time in.", GetHex (via.acr, 2));
    }

    if (label == L"PCR")
    {
        return std::format (L"PCR, peripheral control: {}, the mode of the 6522's CA and CB handshake lines. Mockingboard programs set it to $00 or don't write it.", GetHex (via.pcr, 2));
    }

    if (label == L"IFR")
    {
        return std::format (L"IFR, interrupt flags: {}. Bit 6 is set when timer 1 runs out and bit 5 when timer 2 does; bit 7 is set while a flag IER enables is set. "
                            L"A read of T1's low byte clears bit 6.", GetHex (via.ifr, 2));
    }

    if (label == L"IER")
    {
        return std::format (L"IER, interrupt enable: {}. Each set bit lets that flag interrupt the 6502; $C0 enables timer 1.", GetHex (via.ier, 2));
    }

    if (label == L"T1")
    {
        return std::format (L"T1: {}, timer 1's count, one lower each cycle.", GetHex (via.t1Count, 4));
    }

    if (label == L"Latch")
    {
        return std::format (L"Latch: {}, what timer 1 reloads from when it runs out while free-running, so it runs out every latch plus 2 cycles.", GetHex (via.t1Latch, 4));
    }

    if (label == L"T2")
    {
        return std::format (L"T2: {}, timer 2's count, a one-shot timer, one lower each cycle.", GetHex (via.t2Count, 4));
    }

    if (label == L"Rate")
    {
        return L"Rate: how often timer 1 runs out while free-running, the clock divided by the latch plus 2; a music player's ticks a second.";
    }

    if (label == L"Run")
    {
        return L"Run: timer 1 is counting.";
    }

    if (label == L"Fired")
    {
        return L"Fired: timer 1 has run out since the program last cleared its flag.";
    }

    if (label == L"T1 IRQ")
    {
        return L"T1 IRQ: timer 1's flag is set and enabled, so the 6522 is pulling the 6502's IRQ line low.";
    }

    if (label == L"IRQ")
    {
        return L"IRQ: this 6522 is pulling the 6502's IRQ line low, for any of its enabled flags.";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetVoice
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetVoice (const std::wstring & label, const DiagnosticsSoundVoice & voice)
{
    if (label == L"Phoneme")
    {
        return std::format (L"The phoneme playing: {}, code {}, the low 6 bits of register 0.\n"
                            L"The chip speaks one phoneme at a time, and the program writes the next one when the chip requests it.",
                            TextEncoding::NarrowToWide (voice.phoneme), GetHex (voice.phonemeCode, 2));
    }

    if (label == L"Level")
    {
        return L"The speech chip's output level now.";
    }

    if (label == L"Waveform")
    {
        return L"The speech chip's output over the last few milliseconds. A voiced sound repeats at the pitch; a hiss is noise.";
    }

    if (label == L"Formants")
    {
        return std::format (L"Formants: {:.0f}, {:.0f} and {:.0f} Hz, F1, F2 and F3, the resonances of the vocal tract that make this phoneme sound like itself.\n"
                            L"In the picture they're peaks on a logarithmic frequency axis, over the comb of the pitch's harmonics. F1 and F2 differ from vowel to vowel.",
                            voice.formantsHz[0], voice.formantsHz[1], voice.formantsHz[2]);
    }

    if (label == L"Mode")
    {
        return std::format (L"Mode: {}, how the chip moves from phoneme to phoneme, latched from register 0's duration bits when CTL in register 3 is cleared.\n"
                            L"Phoneme transitioned: the formants glide into the next phoneme over the articulation time, as speech normally runs. "
                            L"Phoneme immediate: each starts at once. Frame immediate: each frame starts at once. A/R disabled: the chip doesn't request the next phoneme.",
                            TextEncoding::NarrowToWide (voice.mode));
    }

    if (label == L"Duration")
    {
        return std::format (L"Duration: {:.1f} ms, how long the phoneme lasts: one to four frames, from register 0's top two bits ({}).",
                            voice.durationMs, voice.durationSelect);
    }

    if (label == L"Frame")
    {
        return std::format (L"Frame: {:.1f} ms, the length of one frame, from the rate in register 2's top four bits ({}). A duration is counted in frames.",
                            voice.frameMs, voice.rateSelect);
    }

    if (label == L"Pitch")
    {
        return std::format (L"Pitch: {}, the voice's fundamental frequency, from the 12-bit inflection, {} (registers 1 and 2).",
                            SoundBoardLayout::GetFrequencyText (voice.pitchHz), GetHex (voice.inflection, 4));
    }

    if (label == L"Articulation")
    {
        return std::format (L"Articulation: {}, how fast the formants move toward the next phoneme's, from register 3's bits 4 to 6.", GetHex (voice.articulation, 2));
    }

    if (label == L"Amplitude")
    {
        return std::format (L"Amplitude: {}, the loudness from 0 to 15, register 3's low 4 bits.", GetHex (voice.amplitude, 2));
    }

    if (label == L"Filter")
    {
        return std::format (L"Filter: {}, the frequency the formant filters are tuned by, from register 4 ({}).",
                            SoundBoardLayout::GetFrequencyText (voice.filterHz), GetHex (voice.filterRegister, 2));
    }

    if (label == L"Power")
    {
        return L"Power: the chip is on. A program powers it down, with CTL in register 3 set, between phrases.";
    }

    if (label == L"Sounding")
    {
        return L"Sounding: a phoneme is playing.";
    }

    if (label == L"Request")
    {
        return L"Request: the chip requests the next phoneme, which interrupts the 6502 where the program enabled it.";
    }

    if (label == L"Recent phonemes")
    {
        return L"Recent phonemes, oldest at the left. Each block is as wide as its phoneme lasted, so a short consonant is a thin block, and a block wide enough "
               L"has its label. The row under the blocks lists the same phonemes by name, with an arrow under the one playing. Hover a block or a name for its phoneme.";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetPhoneme
//
//  A recent phoneme's name is its symbol and example word, as "PA (pause)".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetPhoneme (const DiagnosticsSoundVoice::Phoneme & phoneme)
{
    size_t        space = phoneme.name.find (' ');
    std::wstring  name  = TextEncoding::NarrowToWide (phoneme.name.substr (0, space));
    std::wstring  word  = (space == std::string::npos) ? std::wstring() : TextEncoding::NarrowToWide (phoneme.name.substr (space + 1));



    if (word.size() >= 2 && word.front() == L'(' && word.back() == L')')
    {
        word = word.substr (1, word.size() - 2);
    }

    return word.empty() ? std::format (L"{}, {:.0f} ms", name, phoneme.seconds * 1000.0f)
                        : std::format (L"{} as in {}, {:.0f} ms", name, word, phoneme.seconds * 1000.0f);
}
