#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleProgramTests
//
//  A module loaded into the program on an emulated //c: its request runs,
//  a disk operation overwrites it and EnsureModule loads it again, and a
//  module with a byte changed fails its check.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ModuleProgramTests)
{
public:

    static constexpr Byte  kEcho    = 0xF0;
    static constexpr Byte  kId      = 7;
    static constexpr Byte  kVersion = 3;

    //  One request, which acknowledges the first two bytes of its payload.
    static std::vector<Byte>  BuildEcho()
    {
        static constexpr const char *  kSource =
            "        .org $5000\n"
            "        .byte 'C', 'M', 7, 3\n"
            "        .word PROGVER\n"
            "        .word modend - $5000\n"
            "        .word 0\n"
            "        .byte 1\n"
            "        .byte $F0\n"
            "        .word echo\n"
            "echo:   lda #p_ACK\n"
            "        ldx p_PKTBUF\n"
            "        ldy p_PKTBUF+1\n"
            "        jsr p_sendack\n"
            "        jmp p_main\n"
            "modend:\n";

        std::vector<Byte>  module;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleModule (kSource, module, errors);



        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());

        return module;
    }

    static void  AssertEchoes (ProgramRig & rig)
    {
        const Byte       payload[] = { 0x12, 0x34 };
        CassoLinkPacket  reply;



        AssertSucceeded (rig.host->RequestModule (kEcho, payload, reply));

        Assert::AreEqual (CassoLinkCodec::kAcknowledge, reply.command);
        Assert::AreEqual ((size_t) 3, reply.payload.size());
        Assert::AreEqual ((Byte) 0x12, reply.payload[1]);
        Assert::AreEqual ((Byte) 0x34, reply.payload[2]);
    }


    TEST_METHOD (LoadedModule_RunsItsRequest)
    {
        ProgramRig         rig (std::vector<Byte> (NibblizationLayer::kImageByteSize, 0));
        std::vector<Byte>  module  = BuildEcho();
        Byte               id      = 0xFF;
        Byte               version = 0xFF;



        AssertSucceeded (rig.host->QueryModule (id, version));
        Assert::AreEqual ((Byte) 0, id, L"No module at first");

        AssertSucceeded (rig.host->LoadModule (module));
        AssertSucceeded (rig.host->QueryModule (id, version));
        Assert::AreEqual (kId,      id);
        Assert::AreEqual (kVersion, version);

        AssertEchoes (rig);
    }


    TEST_METHOD (DiskOperation_OverwritesTheModule_AndEnsureLoadsItAgain)
    {
        ProgramRig            rig (std::vector<Byte> (NibblizationLayer::kImageByteSize, 0));
        std::vector<Byte>  module    = BuildEcho();
        std::vector<Byte>  data;
        const Byte         sectors[] = { 0 };
        CassoLinkStatus    status;
        Byte               id        = 0xFF;
        Byte               version   = 0xFF;
        HRESULT            hrRead    = S_OK;



        AssertSucceeded (rig.host->EnsureModule (module));

        //  A read of a blank disk fails to find a sector, and uses the buffers.
        hrRead = rig.host->ReadSectors (1, 0, sectors, data, status);
        IGNORE_RETURN_VALUE (hrRead, S_OK);

        AssertSucceeded (rig.host->QueryModule (id, version));
        Assert::AreEqual ((Byte) 0, id, L"The read overwrote it");

        AssertSucceeded (rig.host->EnsureModule (module));
        AssertEchoes (rig);
    }


    TEST_METHOD (ChangedModule_FailsItsCheck)
    {
        ProgramRig         rig (std::vector<Byte> (NibblizationLayer::kImageByteSize, 0));
        std::vector<Byte>  module = BuildEcho();
        HRESULT            hr     = S_OK;



        module.back() ^= 0x01;

        hr = rig.host->LoadModule (module);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), hr);
    }
};
