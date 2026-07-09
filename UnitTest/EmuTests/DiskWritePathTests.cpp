#include "Pch.h"
#include "Assembler.h"
#include "AssemblerTypes.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Disk/Disk2NibbleEngine.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskWritePathTests
//
//  Hermetic gate for the Disk II LSS *write* path (GH #89). A real 6502
//  runs a DOS-cadence write loop (self-sync nibbles at 40 cyc, data
//  nibbles at 32 cyc) that streams a known nibble sequence onto a mounted
//  synthetic track. The test then frames the track's raw bit stream back
//  into nibbles and searches for the payload signature -- i.e. it asserts
//  that bytes written through the LSS read back as themselves.
//
//  This closes the fidelity gap #67 shipped without: the existing LSS
//  write tests only assert the image dirtied + the cursor advanced, never
//  that written data survives a round trip (Disk2Tests.cpp ~L419).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int    kSlot6   = 6;
    static constexpr int    kDrive1  = 0;
    static constexpr Word   kCodeOrg = 0x6000;
    static constexpr Word   kPayloadAddr = 0x7000;

    // Distinct, valid 6-and-2 nibbles (all MSB-set, no illegal double-zero
    // bit runs) forming a signature unlikely to occur in the surrounding
    // synthesized zero-data track.
    static const std::vector<Byte>  s_kPayload =
    {
        0xD5, 0xAA, 0xAD,                    // data-field prologue
        0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F,
        0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
        0xDE, 0xAA, 0xEB                     // data-field epilogue
    };


    // Assembly for the write routine. X = $60 selects slot 6, so the
    // indexed soft switches resolve to $C0E9/$C0EC/$C0ED/$C0EE/$C0EF.
    std::string  BuildWriteSource (size_t payloadLen)
    {
        std::string  s;
        s += ".org $6000\n";
        s += "MOTOR = $C089\n";
        s += "Q6L   = $C08C\n";
        s += "Q6H   = $C08D\n";
        s += "Q7L   = $C08E\n";
        s += "Q7H   = $C08F\n";
        s += "PTAB  = $7000\n";
        s += "start\n";
        s += "    ldx #$60\n";
        s += "    lda MOTOR,x\n";       // motor on
        s += "    lda Q7L,x\n";         // Q7 off (read)
        s += "    lda Q6L,x\n";         // Q6 off
        s += "    lda Q7H,x\n";         // Q7 on (write armed)

        // Self-sync run: 40 x $FF at 40 cycles/nibble (10 bit cells).
        s += "    ldy #40\n";
        s += "sync1\n";
        s += "    lda #$FF\n";          // 2
        s += "    sta Q6H,x\n";         // 5  load  [load point]
        s += "    lda Q6L,x\n";         // 4  shift
        s += "    dey\n";               // 2
        for (int i = 0; i < 12; i++) { s += "    nop\n"; }   // +24 -> 40
        s += "    bne sync1\n";         // 3

        // Payload: N nibbles at 32 cycles/nibble (8 bit cells).
        s += "    ldy #0\n";
        s += "data1\n";
        s += "    lda PTAB,y\n";        // 4
        s += "    sta Q6H,x\n";         // 5  load  [load point]
        s += "    lda Q6L,x\n";         // 4  shift
        s += "    iny\n";               // 2
        s += "    cpy #" + std::to_string (payloadLen) + "\n";  // 2
        for (int i = 0; i < 6; i++) { s += "    nop\n"; }    // +12 -> 32
        s += "    bne data1\n";         // 3

        // Trailing self-sync so the payload is bracketed by gaps.
        s += "    ldy #20\n";
        s += "sync2\n";
        s += "    lda #$FF\n";
        s += "    sta Q6H,x\n";
        s += "    lda Q6L,x\n";
        s += "    dey\n";
        for (int i = 0; i < 12; i++) { s += "    nop\n"; }
        s += "    bne sync2\n";

        s += "    lda Q7L,x\n";         // Q7 off
        s += "halt\n";
        s += "    jmp halt\n";
        return s;
    }


    // Frame a track's packed bit stream into nibbles exactly as the LSS
    // reader does: accumulate bits MSB-first until the MSB latches.
    std::vector<Byte>  FrameTrack (const DiskImage & img, int slot)
    {
        std::vector<Byte>  out;
        size_t             trackBits = img.GetTrackBitCount (slot);
        size_t             bitPos    = 0;

        if (trackBits == 0) { return out; }

        while (bitPos < trackBits)
        {
            Byte    value = 0;
            size_t  guard = 0;

            while ((value & 0x80) == 0 && bitPos < trackBits && guard < 16)
            {
                value = static_cast<Byte> ((value << 1) |
                        (img.ReadBit (slot, bitPos) & 1));
                bitPos++;
                guard++;
            }
            if (value & 0x80) { out.push_back (value); }
        }
        return out;
    }


    size_t  FindSubsequence (const std::vector<Byte> & hay, const std::vector<Byte> & needle)
    {
        if (needle.empty () || hay.size () < needle.size ()) { return std::string::npos; }

        for (size_t i = 0; i + needle.size () <= hay.size (); i++)
        {
            bool  match = true;
            for (size_t j = 0; j < needle.size (); j++)
            {
                if (hay[i + j] != needle[j]) { match = false; break; }
            }
            if (match) { return i; }
        }
        return std::string::npos;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DiskWritePathTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskWritePathTests)
{
public:

    // Engine-level round trip (no CPU, no controller): write a run of 0xFF
    // through the LSS write path, then read the deposited flux back through
    // the LSS read path and assert the 0xFF nibbles survive. Regression gate
    // for GH #89, where the write sampled the sequencer state bit instead of
    // the shift-register MSB and deposited ~AA garbage where FF sync belongs.
    TEST_METHOD (LssWrite_DirectEngine_FF_RoundTrips)
    {
        DiskImage           img;
        Disk2NibbleEngine   eng;

        img.ResizeTrack (0, 4096);
        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetCurrentTrack (0);
        eng.SetWriteMode (true);

        constexpr int  kSyncBytes = 6;
        for (int n = 0; n < kSyncBytes; n++)
        {
            eng.SetShiftLoadMode (true);                    // Q6 high (load)
            eng.WriteLatch (0xFF);
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);     // 1 cell to load
            eng.SetShiftLoadMode (false);                   // Q6 low (shift)
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 8); // 8 cells shift
        }

        // Rewind and read the written flux back through the LSS reader.
        eng.Reset ();
        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetCurrentTrack (0);

        int  leadingFF = 0;
        bool counting  = true;
        for (int cell = 0; cell < 400 && counting; cell++)
        {
            uint8_t  nib = 0;
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);
            if (eng.ConsumeFreshNibble (nib))
            {
                if (nib == 0xFF) { leadingFF++; }
                else if (leadingFF > 0) { counting = false; }
            }
        }

        Assert::IsTrue (leadingFF >= kSyncBytes,
            L"0xFF written through the LSS must read back as 0xFF (GH #89).");
    }


    TEST_METHOD (LssWrite_KnownNibbles_RoundTripThroughBitstream)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT  hr = host.BuildApple2eWithDisk2 (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle ();

        // Mount a synthetic blank .dsk (nibblizes to a formatted track 0).
        std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);
        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1, "blank.dsk",
                                             DiskFormat::Dsk, blank);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        DiskImage *  img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img);
        core.diskController->SetExternalDisk (kDrive1, img);

        // Poke the payload nibble table into RAM at $7000.
        for (size_t i = 0; i < s_kPayload.size (); i++)
        {
            core.bus->WriteByte (static_cast<Word> (kPayloadAddr + i), s_kPayload[i]);
        }

        // Assemble + load the write routine.
        Cpu             asmCpu;
        Assembler       assembler (asmCpu.GetInstructionSet ());
        AssemblyResult  r = assembler.Assemble (BuildWriteSource (s_kPayload.size ()));

        if (!r.success)
        {
            const char *  e = r.errors.empty () ? "(none)" : r.errors[0].message.c_str ();
            wchar_t  msg[256] = {};
            swprintf_s (msg, L"write routine must assemble. First error: %hs", e);
            Assert::Fail (msg);
        }
        Assert::AreEqual (Word (kCodeOrg), r.startAddress, L"routine must .org $6000");

        for (size_t i = 0; i < r.bytes.size (); i++)
        {
            core.bus->WriteByte (static_cast<Word> (kCodeOrg + i), r.bytes[i]);
        }

        core.cpu->SetPC (kCodeOrg);
        core.RunCycles (200'000ULL);

        // Frame track 0 back into nibbles and search for the payload.
        std::vector<Byte>  nibbles   = FrameTrack (*img, 0);
        size_t             payloadAt = FindSubsequence (nibbles, s_kPayload);

        Assert::IsTrue (payloadAt != std::string::npos,
            L"Nibbles written through the LSS must frame back to the payload (GH #89).");
    }
};
