#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/TestMachine.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLoaderTests
//
//  The serial loader a bootstrap types into an Apple, run on a //c's
//  firmware and 65C02. The loader's bytes are put in memory at $3A50 rather
//  than typed in, and the shim transfer is written to the port from this
//  thread, so the machine runs only when the test runs it and nothing waits
//  on the clock.
//
//  With the right XOR the loader starts the shim; with a byte of a page
//  flipped it returns to Applesoft's prompt, where the bootstrap was typed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SerialLoaderTests)
{
public:

    static constexpr uint64_t  kBootCycles     = 2'000'000;
    static constexpr uint64_t  kKeyCycles      = 100'000;
    static constexpr uint64_t  kStepCycles     = 17'030;
    static constexpr uint64_t  kReadyBudget    = 4'000'000;
    static constexpr uint64_t  kTransferCycles = 6'000'000;
    static constexpr int       kPort           = 2;
    static constexpr DWORD     kBootstrapBaud  = 9600;
    static constexpr size_t    kFlipOffset     = 40;     // a byte of the shim's first page
    static constexpr Byte      kFlipMask       = 0x01;
    static constexpr Word      kRomStart       = 0xC000;

    static void  Type (TestMachine & machine, const char * text)
    {
        for (size_t i = 0; text[i] != '\0'; i++)
        {
            machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
            machine.RunCycles (kKeyCycles);
        }
    }

    //  Runs the loader on a //c at the BASIC prompt, with IN#2 and Ctrl-A 14B
    //  typed as for a bootstrap, and sends `transfer` once the loader's ready
    //  byte arrives. Returns where the CPU is after the transfer.
    static Word  RunLoader (const std::vector<Byte> & transfer)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::vector<Byte>                     loader;
        std::string                           errors;
        Byte                                  got[64]   = {};
        size_t                                read      = 0;
        uint64_t                              spent     = 0;
        bool                                  readySeen = false;
        Word                                  address   = LinkProgram::kSerialLoaderAddress;



        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (kBootCycles);

        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (kBootCycles);

        InProcessByteStream::CreatePair (machineEnd, pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (kPort, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        AssertSucceeded (LinkProgram::AssembleSerialLoader (loader, errors));

        for (Byte b : loader)
        {
            machine.GetMemoryBus().WriteByte (address++, b);
        }

        machine.GetCpu()->SetPC (LinkProgram::kSerialLoaderAddress);

        while (!readySeen && spent < kReadyBudget)
        {
            machine.RunCycles (kStepCycles);
            spent += kStepCycles;

            AssertSucceeded (pcEnd->Read (got, sizeof (got), 0, read));
            readySeen = std::find (got, got + read, LinkBootstrap::kLoaderReady) != got + read;
        }

        Assert::IsTrue (readySeen, L"the loader sends its ready byte");

        AssertSucceeded (pcEnd->Write (transfer.data(), transfer.size()));
        machine.RunCycles (kTransferCycles);

        return machine.GetCpu()->GetPC();
    }

    static bool  IsInShim (Word pc, const std::vector<Byte> & transfer)
    {
        uint32_t  shimEnd = (uint32_t) LinkProgram::kShimAddress + (uint32_t) transfer[0] * 256;

        return pc >= LinkProgram::kShimAddress && (uint32_t) pc < shimEnd;
    }



    TEST_METHOD (TheShimTransfer_PassesTheLoadersXor_AndTheShimRuns)
    {
        std::vector<Byte>  transfer;
        Word               pc = 0;



        AssertSucceeded (LinkBootstrap::BuildShimTransfer (kBootstrapBaud, transfer));

        pc = RunLoader (transfer);

        Assert::IsTrue (IsInShim (pc, transfer), std::format (L"the shim runs, PC ${:04X}", pc).c_str());
    }



    //  A byte of the shim's first page flipped on the line: the loader's XOR
    //  fails, the shim never starts, and the Apple is back at Applesoft's
    //  prompt, where the PC's prompt probe finds it.
    TEST_METHOD (AFlippedShimByte_FailsTheLoadersXor_AndTheAppleReturnsToBasic)
    {
        std::vector<Byte>  transfer;
        Word               pc = 0;



        AssertSucceeded (LinkBootstrap::BuildShimTransfer (kBootstrapBaud, transfer));

        transfer[1 + kFlipOffset] ^= kFlipMask;

        pc = RunLoader (transfer);

        Assert::IsFalse (IsInShim (pc, transfer), std::format (L"the shim never starts, PC ${:04X}", pc).c_str());
        Assert::IsTrue  (pc >= kRomStart,         std::format (L"Applesoft's prompt, in ROM, PC ${:04X}", pc).c_str());
    }
};
