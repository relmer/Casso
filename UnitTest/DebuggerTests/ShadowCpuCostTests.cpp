#include "Pch.h"

#include "Cpu6502.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpuCostTests
//
//  What a scratch Cpu6502 costs to construct (T184). The code pane predicts
//  an instruction's result by executing it on a CPU of its own, and the
//  choice that decision turns on is whether each prediction can afford a
//  FRESH instance.
//
//  A fresh one cannot inherit anything from the prediction before it. Reusing
//  one means re-seeding it, and re-seeding is only as correct as a list of
//  every member that must be cleared -- a list that rots silently. So the
//  fresh instance is worth paying for if the price is small against the
//  handful of predictions one stop asks for.
//
//  Two costs are separated, because they have different answers: the whole
//  construction, and the 64 KB of memory it zero-fills, which a shim that
//  intercepts every access never reads.
//
//  Release only. A Debug build measures the iterator checks.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG

namespace ShadowCpuCostTests
{
    static constexpr int  kRounds  = 2000;
    static constexpr int  kPerStop = 40;      // an annotated pane's visible lines



    static double Now()
    {
        LARGE_INTEGER  counter = {};
        LARGE_INTEGER  freq    = {};



        QueryPerformanceCounter   (&counter);
        QueryPerformanceFrequency (&freq);

        return (double) counter.QuadPart * 1000.0 / (double) freq.QuadPart;
    }



    TEST_CLASS (ShadowCpuCostTests)
    {
    public:

        TEST_METHOD (AScratchCpuIsCheapEnoughToBuildPerPrediction)
        {
            std::vector<double>  samples;
            wchar_t              line[256] = {};
            double               each      = 0.0;



            for (int round = 0; round < 5; round++)
            {
                double  start = Now();

                for (int i = 0; i < kRounds; i++)
                {
                    Cpu6502  scratch;

                    //  Touched so the construction cannot be optimized away.
                    scratch.PokeByte (0x0300, (Byte) i);
                }

                samples.push_back (Now() - start);
            }

            std::sort (samples.begin(), samples.end());
            each = samples[samples.size() / 2] * 1000.0 / kRounds;

            swprintf_s (line, L"Cpu6502 construction: %.1f us each, %.2f ms for %d predictions (one stop's worth)",
                        each, each * kPerStop / 1000.0, kPerStop);
            Logger::WriteMessage (line);

            //  A stop's worth of predictions must not be felt by someone
            //  stepping. Well clear of a frame at 60 Hz.
            Assert::IsTrue (each * kPerStop / 1000.0 < 8.0, line);
        }
    };
}

#endif
