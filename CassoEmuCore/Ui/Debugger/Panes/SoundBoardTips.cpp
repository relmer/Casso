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
//  SoundBoardTips::GetPattern
//
//  What the level does under each value of R13's four bits.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * SoundBoardTips::GetPattern (Byte shape)
{
    static constexpr const wchar_t * kPatterns[] =
    {
        L"falls once, then stays at 0",
        L"falls once, then stays at 0",
        L"falls once, then stays at 0",
        L"falls once, then stays at 0",
        L"rises once, then drops to 0 and stays there",
        L"rises once, then drops to 0 and stays there",
        L"rises once, then drops to 0 and stays there",
        L"rises once, then drops to 0 and stays there",
        L"falls again and again, a sawtooth",
        L"falls once, then stays at 0",
        L"falls and rises again and again, a triangle",
        L"falls once, then jumps to 15 and stays there",
        L"rises again and again, a sawtooth",
        L"rises once, then stays at 15",
        L"rises and falls again and again, a triangle",
        L"rises once, then drops to 0 and stays there",
    };
    constexpr Byte  kShapeMask = 0x0F;



    return kPatterns[shape & kShapeMask];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetRegisterPurpose
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * SoundBoardTips::GetRegisterPurpose (int reg)
{
    static constexpr const wchar_t * kPurposes[kRegisterCount] =
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
        L"the envelope's pattern, from CONT, ATT, ALT and HOLD; a write starts the envelope over",
        L"I/O port A, which the Mockingboard doesn't use",
        L"I/O port B, which the Mockingboard doesn't use",
    };



    return (reg >= 0 && reg < kRegisterCount) ? kPurposes[reg] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetSentence
//
//  Text that opens a tip, with its first letter capitalized.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetSentence (const wchar_t * text)
{
    std::wstring  out = text;



    if (!out.empty() && out[0] >= L'a' && out[0] <= L'z')
    {
        out[0] = (wchar_t) (out[0] - L'a' + L'A');
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetNote (int channel, const DiagnosticsSoundChannel & state)
{
    return std::format (L"The nearest note, with the cents above or below it, and the frequency: the AY's clock divided by 16 times "
                        L"channel {}'s tone period, {} (R{} and R{}).",
                        GetChannelName (channel), GetHex (state.period, 4), channel * 2, channel * 2 + 1);
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
        return std::format (L"The noise generator's random bits are {} for channel {}, since bit {} of the mixer, R7 ({}), is {}.",
                            state.isNoiseOn ? L"on" : L"off", GetChannelName (channel), bit, GetHex (mixer, 2), state.isNoiseOn ? L"clear" : L"set");
    }

    return std::format (L"Channel {}'s square wave is {}, since bit {} of the mixer, R7 ({}), is {}. With noise on too, the noise gates the tone.",
                        GetChannelName (channel), state.isToneOn ? L"on" : L"off", bit, GetHex (mixer, 2), state.isToneOn ? L"clear" : L"set");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetLevel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetLevel (int channel, const DiagnosticsSoundChannel & state, Byte amplitude)
{
    static constexpr const wchar_t * kMeter = L"Each step is about 3 dB, and the faint segment is the peak a moment ago.";



    if (state.usesEnvelope)
    {
        return std::format (L"Channel {}'s level follows the envelope, marked env, since bit 4 of R{} ({}) is set, and moves along the envelope's picture. {}",
                            GetChannelName (channel), 8 + channel, GetHex (amplitude, 2), kMeter);
    }

    return std::format (L"Channel {}'s level, from 0 to 15, is the low 4 bits of R{} ({}). With bit 4 set, the level follows the envelope instead. {}",
                        GetChannelName (channel), 8 + channel, GetHex (amplitude, 2), kMeter);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetScope
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetScope (int channel)
{
    return std::format (L"Channel {}'s output over the last few milliseconds, lined up on a rising edge. A tone is always a square wave, as tall as the "
                        L"level, and noise is random steps; the AY doesn't make any other waveform.",
                        GetChannelName (channel));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetEnvelopePicture
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetEnvelopePicture (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"The envelope's level from 0 to 15 over four ramps, from the write of R13 at the left: it {}. The line and the dot are at the "
                        L"level now, {}{}. A channel follows the envelope while bit 4 of its amplitude register is set.",
                        GetPattern (psg.envelopeRegister), psg.envelopeLevel, psg.isEnvelopeHeld ? L", where the pattern stopped" : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetRamp
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetRamp (const DiagnosticsSoundPsg & psg)
{
    constexpr int  kFine   = 11;
    constexpr int  kCoarse = 12;
    int            period  = (psg.registers[kCoarse] << 8) | psg.registers[kFine];



    return std::format (L"The time one ramp of the envelope takes, from 0 to 15 or 15 to 0: 256 times the envelope period, {} (R11 and R12), "
                        L"divided by the AY's clock.{}",
                        GetHex (period, 4), (period == 0) ? L" A period of 0 counts as 1, the fastest ramp." : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetShape
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetShape (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"The envelope's pattern: the level {}. A write of R13 starts the envelope over at its first ramp. The pattern's four bits "
                        L"are lit beside it where set.",
                        GetPattern (psg.envelopeRegister));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetNoise
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetNoise (const DiagnosticsSoundPsg & psg)
{
    return std::format (L"How often the noise generator steps to a new random bit: the AY's clock divided by 16 times the noise period, {} (R6). "
                        L"One generator serves all three channels.",
                        GetHex (psg.noisePeriod, 2));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetShapeBit
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetShapeBit (int bit, Byte shape)
{
    struct Text
    {
        const wchar_t  * name;
        const wchar_t  * set;
        const wchar_t  * clear;
    };

    static constexpr Text  kText[] =
    {
        { L"R13's bit 3, continue",  L"the envelope goes on past its first ramp",
                                     L"the level drops to 0 after the first ramp and stays there, whatever ALT and HOLD are" },
        { L"R13's bit 2, attack",    L"the first ramp rises from 0 to 15",
                                     L"the first ramp falls from 15 to 0" },
        { L"R13's bit 1, alternate", L"each ramp runs the other way from the last, a triangle, and with HOLD set the level jumps to the other end",
                                     L"each ramp starts over, a sawtooth" },
        { L"R13's bit 0, hold",      L"the level stays where the first ramp ends, or at the other end with ALT set",
                                     L"the ramps repeat while CONT is set" },
    };
    constexpr Byte  kTopBit = 0x08;
    bool            isSet   = false;



    if (bit < 0 || bit >= (int) std::size (kText))
    {
        return std::wstring();
    }

    isSet = (shape & (kTopBit >> bit)) != 0;

    return std::format (L"{}. It's {}, so {}.", kText[bit].name, isSet ? L"set" : L"clear", isSet ? kText[bit].set : kText[bit].clear);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetRegister
//
//  What a register is for, and for the mixer, an amplitude and the pattern,
//  what its value means.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetRegister (int reg, Byte value)
{
    constexpr int   kMixer      = 7;
    constexpr int   kFirstAmp   = 8;
    constexpr int   kLastAmp    = 10;
    constexpr int   kShape      = 13;
    constexpr Byte  kEnvelope   = 0x10;
    constexpr Byte  kLevelMask  = 0x0F;
    std::wstring    out         = GetSentence (GetRegisterPurpose (reg));
    std::wstring    tones;
    std::wstring    noises;



    if (out.empty())
    {
        return out;
    }

    out += L".";

    if (reg == kMixer)
    {
        for (int c = 0; c < DiagnosticsSoundPsg::kChannelCount; c++)
        {
            if ((value & (1 << c)) == 0)
            {
                tones += tones.empty() ? std::wstring (1, GetChannelName (c)) : std::format (L", {}", GetChannelName (c));
            }

            if ((value & (1 << (c + 3))) == 0)
            {
                noises += noises.empty() ? std::wstring (1, GetChannelName (c)) : std::format (L", {}", GetChannelName (c));
            }
        }

        out += std::format (L" Tone is on for {} and noise for {}.", tones.empty() ? L"none" : tones, noises.empty() ? L"none" : noises);
    }
    else if (reg >= kFirstAmp && reg <= kLastAmp)
    {
        out += ((value & kEnvelope) != 0) ? std::wstring (L" The level follows the envelope.") : std::format (L" The level is {}.", value & kLevelMask);
    }
    else if (reg == kShape)
    {
        out += std::format (L" The level {}.", GetPattern (value));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetViaBits
//
//  The timers whose bits are set in an IFR or IER value, as "timer 1" or
//  "timer 1 and timer 2", or an empty string for neither.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetViaBits (Byte value)
{
    constexpr Byte  kTimer1 = 0x40;
    constexpr Byte  kTimer2 = 0x20;
    bool            one     = (value & kTimer1) != 0;
    bool            two     = (value & kTimer2) != 0;



    if (one && two)
    {
        return L"timer 1 and timer 2";
    }

    return one ? L"timer 1" : (two ? L"timer 2" : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetVia
//
//  The tip doesn't begin with the label beside the value or repeat the
//  value. Where a value means more than itself, the tip has what it means.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetVia (const std::wstring & label, const DiagnosticsSoundHalf & half)
{
    static constexpr const wchar_t * kT1Modes[] =
    {
        L"Timer 1 is one-shot: it interrupts once when it runs out.",
        L"Timer 1 is free-running: it reloads from its latch and interrupts at a steady rate, the tick a music player counts time in.",
        L"Timer 1 is one-shot, with a pulse on PB7.",
        L"Timer 1 is free-running, with a square wave on PB7.",
    };
    constexpr Byte                    kResetLine = 0x04;
    constexpr int                     kModeShift = 6;
    constexpr Byte                    kModeMask  = 0x03;
    const DiagnosticsSoundVia       & via        = half.via;
    const DiagnosticsSoundPsg       & psg        = half.psg;



    if (label == L"Bus")
    {
        return L"The AY's bus mode, from its BC1 and BDIR lines, which ORB's bits 0 and 1 drive. In latch address, the byte on port A selects a register; "
               L"in write, the byte goes into that register; in read, the AY puts the register's byte on port A; and in inactive, the AY is off the bus.";
    }

    if (label == L"Writes")
    {
        return L"Bytes written into this AY's registers since the machine restarted.";
    }

    if (label == L"Last")
    {
        return psg.hasLastWrite ? std::format (L"The register written last and the byte written into it. R{} is {}.", psg.lastWriteReg, GetRegisterPurpose (psg.lastWriteReg))
                                : std::wstring (L"The register written last and the byte written into it. None has been written since the machine restarted.");
    }

    if (label == L"ORA")
    {
        return L"Output register A: the byte the 6522 puts on the AY's data lines, a register number or a value for one.";
    }

    if (label == L"ORB")
    {
        return std::format (L"Output register B. Bits 0 to 2 drive the AY's BC1, BDIR and RESET lines, which select inactive, latch address, write or read. "
                            L"RESET is {}.", ((via.orb & kResetLine) != 0) ? L"high, so the AY runs" : L"low, so the AY is in reset");
    }

    if (label == L"DDRA" || label == L"DDRB")
    {
        Byte  direction = (label == L"DDRA") ? via.ddra : via.ddrb;
        int   outputs   = (int) std::bitset<8> (direction).count();

        return std::format (L"Data direction register {}. Each set bit makes that line of port {} an output, and a Mockingboard program sets all 8, $FF. {}",
                            label.back(), label.back(),
                            (outputs == 8) ? std::wstring (L"All 8 are outputs.") : std::format (L"{} of the 8 are outputs.", outputs));
    }

    if (label == L"ACR")
    {
        return std::format (L"Auxiliary control. Bits 6 and 7 set timer 1's mode. {}", kT1Modes[(via.acr >> kModeShift) & kModeMask]);
    }

    if (label == L"PCR")
    {
        return L"Peripheral control: the mode of the 6522's CA and CB handshake lines. Mockingboard programs set it to $00 or don't write it.";
    }

    if (label == L"IFR")
    {
        std::wstring  set = GetViaBits (via.ifr);

        return std::format (L"Interrupt flags. Bit 6 is set when timer 1 runs out and bit 5 when timer 2 does, and bit 7 while a flag IER enables is set; "
                            L"a read of T1's low byte clears bit 6. {}",
                            set.empty() ? std::wstring (L"No timer's flag is set.") : std::format (L"The flag of {} is set.", set));
    }

    if (label == L"IER")
    {
        std::wstring  enabled = GetViaBits (via.ier);

        return std::format (L"Interrupt enable. Each set bit lets that flag interrupt the 6502. {}",
                            enabled.empty() ? std::wstring (L"No timer can interrupt.") : GetSentence (std::format (L"{} can interrupt.", enabled).c_str()));
    }

    if (label == L"T1")
    {
        return L"Timer 1's count, one lower each cycle.";
    }

    if (label == L"Latch")
    {
        return std::format (L"What timer 1 reloads from when it runs out while free-running, so it runs out every {} cycles, the latch plus 2.", via.t1Latch + 2);
    }

    if (label == L"T2")
    {
        return L"Timer 2's count, one lower each cycle. Timer 2 is one-shot.";
    }

    if (label == L"Rate")
    {
        return L"How often timer 1 runs out while free-running: the clock divided by the latch plus 2, which is a music player's ticks a second.";
    }

    if (label == L"Run")
    {
        return L"Lit while timer 1 counts.";
    }

    if (label == L"Fired")
    {
        return L"Lit once timer 1 has run out, until the program clears its flag.";
    }

    if (label == L"T1 IRQ")
    {
        return L"Lit while timer 1's flag is set and enabled, so the 6522 pulls the 6502's IRQ line low.";
    }

    if (label == L"IRQ")
    {
        return L"Lit while this 6522 pulls the 6502's IRQ line low, for any of its enabled flags.";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardTips::GetMode
//
//  What the speech chip's mode means now. A/R is the chip's acknowledge and
//  request line, which interrupts the 6502 for the next phoneme.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SoundBoardTips::GetMode (const std::string & mode)
{
    static constexpr const wchar_t * kRequest = L"The chip requests the next phoneme on A/R, its acknowledge/request line, as each ends.";



    if (mode == "Phoneme transitioned")
    {
        return std::format (L"The formants glide into each phoneme over the articulation time, as speech normally runs. {}", kRequest);
    }

    if (mode == "Phoneme immediate")
    {
        return std::format (L"Each phoneme starts at once, without a glide. {}", kRequest);
    }

    if (mode == "Frame immediate")
    {
        return std::format (L"Each frame starts at once. {}", kRequest);
    }

    return L"A/R, the chip's acknowledge/request line, is off, so the chip doesn't request the next phoneme, and the program times each one.";
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
        return L"The phoneme playing, by its symbol and an example word, and its code, the low 6 bits of register 0. The chip sounds one phoneme at a "
               L"time, and the program writes the next one when the chip requests it.";
    }

    if (label == L"Level")
    {
        return L"The speech chip's output level now.";
    }

    if (label == L"Waveform")
    {
        return L"The speech chip's output over the last few milliseconds. A voiced sound repeats at the pitch, and a hiss is noise.";
    }

    if (label == L"Formants")
    {
        return L"F1, F2 and F3 in Hz, the resonances of the vocal tract that make each phoneme sound like itself; F1 and F2 differ from vowel to vowel. "
               L"In the picture they're peaks on a logarithmic frequency axis, over the comb of the pitch's harmonics.";
    }

    if (label == L"Mode")
    {
        return std::format (L"How the chip moves from phoneme to phoneme, latched from register 0's top two bits when CTL in register 3 is cleared. {}",
                            GetMode (voice.mode));
    }

    if (label == L"Duration")
    {
        return std::format (L"How long the phoneme lasts: one to four frames, from register 0's top two bits ({}).", voice.durationSelect);
    }

    if (label == L"Frame")
    {
        return std::format (L"The length of one frame, from the rate in register 2's top four bits ({}). A duration is counted in frames.", voice.rateSelect);
    }

    if (label == L"Pitch")
    {
        return std::format (L"The voice's fundamental frequency, from the 12-bit inflection, {} (registers 1 and 2).", GetHex (voice.inflection, 4));
    }

    if (label == L"Articulation")
    {
        return L"How fast the formants move toward the next phoneme's, from register 3's bits 4 to 6.";
    }

    if (label == L"Amplitude")
    {
        return L"The loudness from 0 to 15, register 3's low 4 bits.";
    }

    if (label == L"Filter")
    {
        return std::format (L"The frequency the formant filters are tuned by, from register 4 ({}).", GetHex (voice.filterRegister, 2));
    }

    if (label == L"Power")
    {
        return L"Lit while the chip is on. A program powers it down, with CTL in register 3 set, between phrases.";
    }

    if (label == L"Sounding")
    {
        return L"Lit while the chip is producing a phoneme.";
    }

    if (label == L"Request")
    {
        return L"Lit while the chip requests the next phoneme, which interrupts the 6502 where the program enabled it.";
    }

    if (label == L"Recent phonemes")
    {
        return L"The phonemes before the one playing, oldest at the left. Each block is as wide as its phoneme lasted, on one scale, and no narrower "
               L"than the widest label, so a short consonant has its label too. The row under the blocks lists the phonemes by name, with an arrow "
               L"under the one playing, and a line is a pause. The wheel scrolls back through the last 256.";
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
