#include "Pch.h"
#include "Assembler.h"
#include "AssemblerTypes.h"
#include "HeadlessHost.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Disk/Disk2NibbleEngine.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"

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

TEST_CLASS (DiskWritePathTests)
{
public:

    static constexpr int   kSlot6       = 6;
    static constexpr int   kDrive1      = 0;
    static constexpr Word  kCodeOrg     = 0x6000;
    static constexpr Word  kPayloadAddr = 0x7000;

    // Distinct, valid 6-and-2 nibbles (all MSB-set, no illegal double-zero
    // bit runs) forming a signature unlikely to occur in the surrounding
    // synthesized zero-data track.
    static inline const std::vector<Byte>  s_kPayload =
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
    static constexpr char  kWriteSource[] = R"(
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

        // An unformatted track has no bits to frame; the loop condition already
        // says so, so the guard is only documentation.
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
        size_t  at = std::string::npos;

        // An empty needle finds nothing rather than matching at 0: callers use
        // the result to assert a pattern IS present, so a vacuous hit would
        // pass a test that proved nothing.
        if (!needle.empty() && hay.size() >= needle.size())
        {
            for (size_t i = 0; at == std::string::npos && i + needle.size() <= hay.size(); i++)
            {
                bool  match = true;

                for (size_t j = 0; match && j < needle.size(); j++)
                {
                    match = (hay[i + j] == needle[j]);
                }

                if (match) { at = i; }
            }
        }

        return at;
    }

    // Engine-level round trip (no CPU, no controller): write a run of 0xFF
    // through the LSS write path, then read the deposited flux back through
    // the LSS read path and assert the 0xFF nibbles survive. Regression gate
    // for GH #89, where the write sampled the sequencer state bit instead of
    // the shift-register MSB and deposited ~AA garbage where FF sync belongs.
    TEST_METHOD (LssWrite_DirectEngine_FF_RoundTrips)
    {
        DiskImage          img;
        Disk2NibbleEngine  eng;
        constexpr int      kSyncBytes = 6;
        int                leadingFF  = 0;
        bool               counting   = false;

        img.ResizeTrack (0, 4096);
        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetCurrentTrack (0);
        eng.SetWriteMode (true);

        for (int n = 0; n < kSyncBytes; n++)
        {
            eng.SetShiftLoadMode (true);                    // Q6 high (load)
            eng.WriteLatch (0xFF);
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);     // 1 cell to load
            eng.SetShiftLoadMode (false);                   // Q6 low (shift)
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 8); // 8 cells shift
        }

        // Rewind and read the written flux back through the LSS reader.
        eng.Reset();
        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetCurrentTrack (0);

        counting = true;
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
        HeadlessHost         host;
        EmulatorCore         core;
        DiskImage          * img       = nullptr;
        Cpu                  asmCpu;
        std::vector<Byte>    nibbles;
        size_t               payloadAt = 0;
        AssemblyResult       r;

        HRESULT  hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        // Mount a synthetic blank .dsk (nibblizes to a formatted track 0).
        std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);
        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1, "blank.dsk",
                                             DiskFormat::Dsk, blank);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img);
        core.diskController->SetExternalDisk (kDrive1, img);

        // Poke the payload nibble table into RAM at $7000.
        for (size_t i = 0; i < s_kPayload.size(); i++)
        {
            core.bus->WriteByte (static_cast<Word> (kPayloadAddr + i), s_kPayload[i]);
        }

        // Assemble + load the write routine. PLEN in kWriteSource is
        // hardcoded to the payload length; keep the two in step.
        Assert::AreEqual (size_t (26), s_kPayload.size(),
            L"kWriteSource hardcodes PLEN = 26; update both together");

        Assembler       assembler (asmCpu.GetInstructionSet());
        r = assembler.Assemble (kWriteSource);

        if (!r.success)
        {
            wchar_t  msg[256] = {};

            const char *  e = r.errors.empty() ? "(none)" : r.errors[0].message.c_str();
            swprintf_s (msg, L"write routine must assemble. First error: %hs", e);
            Assert::Fail (msg);
        }

        Assert::AreEqual (Word (kCodeOrg), r.startAddress, L"routine must .org $6000");

        for (size_t i = 0; i < r.bytes.size(); i++)
        {
            core.bus->WriteByte (static_cast<Word> (kCodeOrg + i), r.bytes[i]);
        }

        core.cpu->SetPC (kCodeOrg);
        core.RunCycles (200'000ULL);

        // Frame track 0 back into nibbles and search for the payload.
        nibbles = FrameTrack (*img, 0);
        payloadAt = FindSubsequence (nibbles, s_kPayload);

        Assert::IsTrue (payloadAt != std::string::npos,
            L"Nibbles written through the LSS must frame back to the payload (GH #89).");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  WriteProtectToggleTests
//
//  Store-level contract behind the Disk-menu write-protect toggle: the WOZ
//  image flag round-trips through a forced flush, the flush-before-protect
//  ordering loses no dirty sectors, and the regular flush gate (which the
//  ordering exists to dodge) really does drop dirty content once the image
//  is protected.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WriteProtectToggleTests)
{
public:

    static vector<Byte> BuildBlankWoz()
    {
        BlankDiskSpec  spec;
        vector<Byte>   woz;

        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload{}, woz));

        return woz;
    }


    // Casso's writer always lays INFO down first, so its payload starts at
    // offset 20 and the write-protect flag sits two bytes into it. The tests
    // below assert that layout before relying on the offset.
    static constexpr size_t  kInfoPayloadOffset = 20;
    static constexpr size_t  kWpFlagOffset      = kInfoPayloadOffset + 2;
    static constexpr size_t  kHeaderCrcOffset   = 8;
    static constexpr size_t  kHeaderCrcSize     = 4;

    void AssertInfoIsFirstChunk (const vector<Byte> & woz)
    {
        Assert::IsTrue (woz.size() > kInfoPayloadOffset, L"too small to hold an INFO chunk");
        Assert::AreEqual (string ("INFO"),
            string (reinterpret_cast<const char *> (woz.data() + 12), 4),
            L"precondition: INFO is the first chunk, so its flag byte is at a known offset");
    }


    TEST_METHOD (WozWpFlag_RoundTripsThroughTheFileItPatches)
    {
        DiskImageStore  store;
        DiskImage     * img  = nullptr;
        DiskImage       reloaded;
        vector<Byte>    file = BuildBlankWoz();



        // Paired read and write seams: one in-memory buffer standing in for
        // the backing file, so the patch is a real read-modify-write.
        store.SetImageReader ([&file] (const string &, vector<Byte> & out)
        {
            out = file;
            return S_OK;
        });

        store.SetFlushSink ([&file] (const string &, const vector<Byte> & bytes)
        {
            file = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "t.woz", DiskFormat::Woz, file));

        img = store.GetImage (6, 0);
        Assert::IsNotNull (img);

        AssertSucceeded (store.SetImageWriteProtect (6, 0, true));

        AssertSucceeded (WozLoader::Load (file, reloaded));
        Assert::IsTrue (reloaded.GetWriteProtectInfo().imageFlag,
            L"the flag must land in the file's INFO chunk");
        Assert::IsTrue (img->GetWriteProtectInfo().imageFlag,
            L"and the live image must agree with the file");

        AssertSucceeded (store.SetImageWriteProtect (6, 0, false));

        AssertSucceeded (WozLoader::Load (file, reloaded));
        Assert::IsFalse (reloaded.GetWriteProtectInfo().imageFlag,
            L"clearing the flag must round-trip too -- un-protecting is what a "
            L"user does to a preservation dump before writing to it");
        Assert::IsFalse (img->GetWriteProtectInfo().imageFlag);
    }


    TEST_METHOD (WriteProtectToggle_TouchesOnlyTheFlagByteAndTheChecksum)
    {
        // The whole point of the operation. Flipping this flag must not be an
        // excuse to rewrite the file: a preservation dump carries chunks and
        // INFO fields Casso cannot reproduce, and a click on a menu item is
        // not a reason to put any of them at risk.
        DiskImageStore  store;
        vector<Byte>    file     = BuildBlankWoz();
        vector<Byte>    original;
        size_t          i        = 0;
        size_t          diffs    = 0;
        bool            crcMoved = false;



        AssertInfoIsFirstChunk (file);
        original = file;

        store.SetImageReader ([&file] (const string &, vector<Byte> & out)
        {
            out = file;
            return S_OK;
        });

        store.SetFlushSink ([&file] (const string &, const vector<Byte> & bytes)
        {
            file = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "t.woz", DiskFormat::Woz, file));
        AssertSucceeded (store.SetImageWriteProtect (6, 0, true));

        Assert::AreEqual (original.size(), file.size(),
            L"patching one flag must not resize the file");

        for (i = 0; i < original.size(); i++)
        {
            if (original[i] == file[i])
            {
                continue;
            }

            diffs++;

            if (i >= kHeaderCrcOffset && i < kHeaderCrcOffset + kHeaderCrcSize)
            {
                crcMoved = true;
                continue;
            }

            Assert::AreEqual (kWpFlagOffset, i,
                L"the only byte outside the header checksum that may change is "
                L"the write-protect flag itself");
        }

        Assert::IsTrue (crcMoved,
            L"the stored checksum must be recomputed, or the file now fails its own check");
        Assert::AreEqual (Byte (1), file[kWpFlagOffset], L"the flag must actually be set");
        Assert::IsTrue (diffs >= 2 && diffs <= 1 + kHeaderCrcSize,
            L"exactly the flag byte plus some or all of the four checksum bytes");
    }


    TEST_METHOD (WriteProtectToggle_PersistsPendingGuestWritesFirst)
    {
        // Protecting a disk closes the gate that guest writes go through, so
        // anything still dirty has to be written before the flag is set. The
        // ordering used to live in the menu handler; it now lives inside the
        // operation, which is why this test no longer performs it by hand.
        DiskImageStore  store;
        DiskImage     * img  = nullptr;
        DiskImage       reloaded;
        vector<Byte>    file = BuildBlankWoz();
        uint8_t         bit0 = 0;



        store.SetImageReader ([&file] (const string &, vector<Byte> & out)
        {
            out = file;
            return S_OK;
        });

        store.SetFlushSink ([&file] (const string &, const vector<Byte> & bytes)
        {
            file = bytes;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "t.woz", DiskFormat::Woz, file));

        img = store.GetImage (6, 0);
        Assert::IsNotNull (img);

        bit0 = img->ReadBit (0, 0);
        img->WriteBit (0, 0, (uint8_t) (bit0 ^ 1));
        Assert::IsTrue (img->IsDirty(), L"precondition: there is a pending guest write");

        AssertSucceeded (store.SetImageWriteProtect (6, 0, true));

        AssertSucceeded (WozLoader::Load (file, reloaded));

        Assert::IsTrue (reloaded.GetWriteProtectInfo().imageFlag,
            L"the flag must be set in the final file");
        Assert::AreEqual ((int) (bit0 ^ 1), (int) reloaded.ReadBit (0, 0),
            L"and the guest write must survive -- the caller no longer has to "
            L"remember to flush before protecting");
    }


    TEST_METHOD (RegularFlush_DropsDirtyOnceProtected)
    {
        DiskImageStore  store;
        DiskImage     * img        = nullptr;
        int             sinkCalls  = 0;
        uint8_t         bit0       = 0;



        store.SetFlushSink ([&sinkCalls] (const string &, const vector<Byte> &)
        {
            sinkCalls++;
            return S_OK;
        });

        AssertSucceeded (store.MountFromBytes (6, 0, "t.woz", DiskFormat::Woz, BuildBlankWoz()));

        img = store.GetImage (6, 0);
        Assert::IsNotNull (img);

        bit0 = img->ReadBit (0, 0);
        img->WriteBit (0, 0, (uint8_t) (bit0 ^ 1));
        img->SetImageWriteProtected (true);

        // The gate the toggle's ordering exists for: a protected image's
        // regular flush drops the dirty content without writing.
        AssertSucceeded (store.Flush (6, 0));
        Assert::AreEqual (0, sinkCalls, L"no write may happen through the gate");
        Assert::IsFalse (img->IsDirty(), L"the dirty bit is consumed");
    }
};

