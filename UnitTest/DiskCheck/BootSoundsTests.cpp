#include "Pch.h"

#include "DiskCheck/BootSounds.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootSoundsTests
//
//  Each sound is a WAV file of one channel of 16-bit samples.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootSoundsTests)
{
public:

    static uint32_t  Number (const std::vector<Byte> & wav, size_t at, int bytes)
    {
        uint32_t  value = 0;

        for (int i = 0; i < bytes; i++)
        {
            value |= (uint32_t) wav[at + (size_t) i] << (8 * i);
        }

        return value;
    }

    static void  AssertWav (const std::vector<Byte> & wav, double seconds)
    {
        Assert::IsTrue (wav.size() > 44);
        Assert::AreEqual (std::string ("RIFF"), std::string (wav.begin(), wav.begin() + 4));
        Assert::AreEqual (std::string ("WAVE"), std::string (wav.begin() + 8, wav.begin() + 12));
        Assert::AreEqual ((uint32_t) wav.size() - 8, Number (wav, 4, 4));
        Assert::AreEqual ((uint32_t) 1, Number (wav, 22, 2), L"One channel");
        Assert::AreEqual ((uint32_t) BootSounds::kSampleRate, Number (wav, 24, 4));
        Assert::AreEqual ((uint32_t) wav.size() - 44, Number (wav, 40, 4));
        Assert::AreEqual (seconds, (double) (wav.size() - 44) / 2 / BootSounds::kSampleRate, 0.01);
    }

    TEST_METHOD (TheSoundForAnImageThatBooted_IsAShortWav)
    {
        AssertWav (BootSounds::MakeBooted(), 0.67);
    }

    TEST_METHOD (TheSoundForAnImageThatDidNotBoot_IsALongerWav)
    {
        AssertWav (BootSounds::MakeDidNotBoot(), 2.34);
    }
};
