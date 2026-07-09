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


    // A DOS-cadence write routine. X = $60 selects slot 6, so the indexed
    // soft switches resolve to $C0E9/$C0EC/$C0ED/$C0EE/$C0EF. Each loop body
    // is padded with NOPs to the exact bit-cell budget the LSS expects: 40
    // cycles/nibble for self-sync $FF (10 bit cells) and 32 cycles/nibble
    // for the payload (8 bit cells). PLEN is the payload length; the test
    // asserts it stays in step with s_kPayload before running the routine.
    constexpr char  kWriteSource[] = R"(
                    .org $6000
        MOTOR = $C089
        Q6L   = $C08C
        Q6H   = $C08D
        Q7L   = $C08E
        Q7H   = $C08F
        PTAB  = $7000
        PLEN  = 26                  ; == s_kPayload.size()
        start:
                    ldx #$60
                    lda MOTOR,x         ; motor on
                    lda Q7L,x           ; Q7 off (read)
                    lda Q6L,x           ; Q6 off
                    lda Q7H,x           ; Q7 on (write armed)

                    ; leading self-sync: 40 x $FF at 40 cycles/nibble
                    ldy #40
        sync1:
                    lda #$FF            ; 2
                    sta Q6H,x           ; 5  load
                    lda Q6L,x           ; 4  shift
                    dey                 ; 2
                    nop                 ; 12 nops = 24  -> 40 total
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    bne sync1           ; 3

                    ; payload: PLEN nibbles at 32 cycles/nibble
                    ldy #0
        data1:
                    lda PTAB,y          ; 4
                    sta Q6H,x           ; 5  load
                    lda Q6L,x           ; 4  shift
                    iny                 ; 2
                    cpy #PLEN           ; 2
                    nop                 ; 6 nops = 12  -> 32 total
                    nop
                    nop
                    nop
                    nop
                    nop
                    bne data1           ; 3

                    ; trailing self-sync so the payload is bracketed by gaps
                    ldy #20
        sync2:
                    lda #$FF
                    sta Q6H,x
                    lda Q6L,x
                    dey
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    nop
                    bne sync2

                    lda Q7L,x           ; Q7 off
        halt:
                    jmp halt
    )";


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

        // Assemble + load the write routine. PLEN in kWriteSource is
        // hardcoded to the payload length; keep the two in step.
        Assert::AreEqual (size_t (26), s_kPayload.size (),
            L"kWriteSource hardcodes PLEN = 26; update both together");

        Cpu             asmCpu;
        Assembler       assembler (asmCpu.GetInstructionSet ());
        AssemblyResult  r = assembler.Assemble (kWriteSource);

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
