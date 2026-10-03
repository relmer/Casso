#include "Pch.h"

#include "HResultAssert.h"
#include "Core/IMachineState.h"
#include "Core/StateWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint32_t  s_kReuseTag        = IMachineState::MakeTag ('R', 'E', 'U', 'S');
static constexpr size_t    s_kReuseStale      = 4096;
static constexpr Byte      s_kReuseStaleByte  = 0xEE;





////////////////////////////////////////////////////////////////////////////////
//
//  StateWriterReuseTests
//
//  A writer given a used buffer writes over it rather than clearing it first,
//  so the stream must come out the same as from a fresh writer whatever the
//  buffer held.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StateWriterReuseTests)
{
public:

    TEST_METHOD (AReusedBufferGivesTheSameStreamAsAFreshWriter)
    {
        StateWriter        fresh;
        StateWriter        reused;
        std::vector<Byte>  taken;



        WriteSample (fresh);

        reused.Reuse (std::vector<Byte> (s_kReuseStale, s_kReuseStaleByte));
        WriteSample (reused);

        Assert::IsTrue (reused.GetBytes() == fresh.GetBytes(), L"nothing of the old buffer shows in the stream");

        taken = reused.TakeBytes();

        Assert::IsTrue (taken == fresh.GetBytes(), L"and the buffer handed out is cut to the stream");
        Assert::IsTrue (reused.GetBytes().empty(), L"the writer starts over empty after handing it out");

        reused.Reuse (std::move (taken));
        WriteSample (reused);
        WriteSample (fresh);

        Assert::IsTrue (reused.GetBytes() != fresh.GetBytes(), L"a reused writer starts over rather than appending");

        reused.Reuse (std::vector<Byte>());
        WriteSample (reused);
        WriteSample (reused);

        Assert::IsTrue (reused.GetBytes() == fresh.GetBytes(), L"a stream longer than its buffer grows it");
    }


private:

    static void WriteSample (StateWriter & writer)
    {
        HRESULT  hr = S_OK;



        writer.BeginSection (s_kReuseTag, 1);
        writer.WriteByte    (0x12);
        writer.WriteBool    (true);
        writer.WriteWord    (0x3456);
        writer.WriteUInt32  (0x789ABCDE);
        writer.WriteUInt64  (0x0102030405060708ULL);

        hr = writer.EndSection();
        AssertSucceeded (hr, L"EndSection");
    }
};
