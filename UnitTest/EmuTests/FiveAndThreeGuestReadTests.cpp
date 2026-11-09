#include "Pch.h"

#include "Assembler.h"
#include "AssemblerTypes.h"
#include "ComparisonTestImages.h"
#include "../EhmTestHelper.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/SectorFieldWriter.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FiveAndThreeGuestReadTests
//
//  An edit to a sector of a made-up 13-sector disk, read back by a 6502
//  5-and-3 read routine on a test machine (SC-018). The routine is written
//  for the test from the published DOS 3.2 format: address prologue D5 AA
//  B5, 4-and-4 address values, data prologue D5 AA AD, 410 nibbles from the
//  32-entry nibble list, each value exclusive-ORed with the one before it,
//  and a checksum nibble for the last value. The secondary buffer comes
//  first, from its last value down; the primary buffer follows. No code in
//  it comes from Casso's encoder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FiveAndThreeGuestReadTests)
{
public:

    static constexpr int       kSlot6       = 6;
    static constexpr int       kDrive1      = 0;
    static constexpr Byte      kSector      = 5;
    static constexpr Word      kCodeOrg     = 0x6000;
    static constexpr Word      kOutAddr     = 0x7900;
    static constexpr Word      kDenibAddr   = 0x7A00;
    static constexpr Word      kGoodAddr    = 0x7F00;
    static constexpr Word      kDoneAddr    = 0x7F01;
    static constexpr Word      kTargetAddr  = 0x7F02;
    static constexpr uint64_t  kStepCycles  = 100'000ULL;
    static constexpr int       kMaxSteps    = 200;

    //  The 5-and-3 nibbles, in value order.
    static constexpr std::array<Byte, 32>  kNibbles =
    {
        0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
        0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
        0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
        0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF,
    };



    //  Reads the sector at TARGET on the track under the head, decodes it
    //  into OUT, and sets GOOD when the checksum matches and DONE at the end.
    static constexpr char  kReadSource[] = R"(
                    .org $6000
        MOTOR  = $C089
        DRV1   = $C08A
        Q6L    = $C08C
        Q7L    = $C08E
        RAWS   = $7400
        RAWP   = $7500
        CHK    = $7600
        SEC    = $7700
        PRI    = $7800
        OUT    = $7900
        DENIB  = $7A00
        GOOD   = $7F00
        DONE   = $7F01
        TARGET = $7F02
        PREV   = $06
        AUX    = $07
        F4     = $08
        F5     = $09
        ODD    = $0A
        start:
                    ldx #$60
                    lda MOTOR,x
                    lda DRV1,x
                    lda Q7L,x
                    lda Q6L,x
        addr:
                    lda Q6L,x
                    bpl addr
        addr1:
                    cmp #$D5
                    bne addr
        addr2:
                    lda Q6L,x
                    bpl addr2
                    cmp #$AA
                    bne addr1
        addr3:
                    lda Q6L,x
                    bpl addr3
                    cmp #$B5
                    bne addr1
                    ldy #4
        skip:
                    lda Q6L,x
                    bpl skip
                    dey
                    bne skip
        secodd:
                    lda Q6L,x
                    bpl secodd
                    sec
                    rol a
                    sta ODD
        seceven:
                    lda Q6L,x
                    bpl seceven
                    and ODD
                    cmp TARGET
                    bne addr
        data:
                    lda Q6L,x
                    bpl data
        data1:
                    cmp #$D5
                    bne data
        data2:
                    lda Q6L,x
                    bpl data2
                    cmp #$AA
                    bne data1
        data3:
                    lda Q6L,x
                    bpl data3
                    cmp #$AD
                    bne data1
                    ldy #0
        raws:
                    lda Q6L,x
                    bpl raws
                    sta RAWS,y
                    iny
                    cpy #154
                    bne raws
                    ldy #0
        rawp:
                    lda Q6L,x
                    bpl rawp
                    sta RAWP,y
                    iny
                    bne rawp
        rawc:
                    lda Q6L,x
                    bpl rawc
                    sta CHK

                    ; undo the chain: secondary values go to SEC from its end
                    lda #0
                    sta PREV
                    ldy #0
        decs:
                    ldx RAWS,y
                    lda DENIB,x
                    eor PREV
                    sta PREV
                    pha
                    tya
                    eor #$FF
                    clc
                    adc #154
                    tax
                    pla
                    sta SEC,x
                    iny
                    cpy #154
                    bne decs
                    ldy #0
        decp:
                    ldx RAWP,y
                    lda DENIB,x
                    eor PREV
                    sta PREV
                    sta PRI,y
                    iny
                    bne decp
                    ldx CHK
                    lda DENIB,x
                    cmp PREV
                    bne post
                    lda #1
                    sta GOOD

                    ; 51 groups of five bytes, group i at index j = 50 - i
        post:
                    ldx #50
                    ldy #0
        group:
                    lda #0
                    sta F4
                    sta F5

                    lda SEC,x
                    sta AUX
                    lsr a
                    lsr a
                    sta ODD
                    lda PRI,x
                    asl a
                    asl a
                    asl a
                    ora ODD
                    sta OUT,y
                    lda AUX
                    lsr a
                    rol F5
                    lsr a
                    rol F4

                    lda SEC+51,x
                    sta AUX
                    lsr a
                    lsr a
                    sta ODD
                    lda PRI+51,x
                    asl a
                    asl a
                    asl a
                    ora ODD
                    sta OUT+1,y
                    lda AUX
                    lsr a
                    rol F5
                    lsr a
                    rol F4

                    lda SEC+102,x
                    sta AUX
                    lsr a
                    lsr a
                    sta ODD
                    lda PRI+102,x
                    asl a
                    asl a
                    asl a
                    ora ODD
                    sta OUT+2,y
                    lda AUX
                    lsr a
                    rol F5
                    lsr a
                    rol F4

                    lda PRI+153,x
                    asl a
                    asl a
                    asl a
                    ora F4
                    sta OUT+3,y
                    lda PRI+204,x
                    asl a
                    asl a
                    asl a
                    ora F5
                    sta OUT+4,y

                    tya
                    clc
                    adc #5
                    tay
                    dex
                    bpl group

                    lda PRI+255
                    asl a
                    asl a
                    asl a
                    ora SEC+153
                    sta OUT+255
                    lda #1
                    sta DONE
        halt:
                    jmp halt
    )";



    static std::array<Byte, DiskFieldFormat::kSectorBytes> MakeEditedBytes()
    {
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes = {};



        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> ((i * 13 + 0x5A) & 0xFF);
        }

        return bytes;
    }



    //  Every track 13-sector, as DOS 3.2 formats a disk.
    static std::vector<Byte> BuildThirteenSectorWoz()
    {
        std::vector<WozSyntheticTrack>  tracks;
        std::vector<Byte>               woz;
        ComparisonTestImages::TrackSpec spec;



        spec.isThirteen = true;

        for (int t = 0; t < ComparisonTestImages::kTracks; t++)
        {
            tracks.push_back (ComparisonTestImages::MakeTrack (t, spec));
            tracks.back().quarterTracks = { 4 * t, 4 * t + 1 };

            if (t > 0)
            {
                tracks.back().quarterTracks.push_back (4 * t - 1);
            }
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));

        return woz;
    }



    //  The edit for track 0's sector kSector, found as the inspector's
    //  editor finds the field it draws.
    static SectorWrite MakeWrite (const DiskImage & image)
    {
        SectorWrite   write;
        DiskAnalysis  analysis;



        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "thirteen.woz", 0, false), DecodeSettings::MakeStandard(), analysis);

        const TrackAnalysis &  track = *analysis.tracks[analysis.entries[0].slot];

        for (const AnalyzedSector & sector : track.sectors)
        {
            if (sector.sector == kSector && sector.dataField >= 0)
            {
                write.slot          = track.slot;
                write.dataFieldCell = static_cast<int> (track.fields[sector.dataField].startCell);
                write.sector        = sector.sector;
            }
        }

        write.bytes = MakeEditedBytes();

        return write;
    }



    TEST_METHOD (A5And3ReadRoutineReadsAnEditedSector)
    {
        TestMachine                                        machine ("Apple2e");
        Cpu                                                asmCpu;
        Assembler                                          assembler (asmCpu.GetInstructionSet());
        AssemblyResult                                     assembled;
        DiskImage                                        * image     = nullptr;
        SectorWrite                                        write;
        vector<SectorWriteError>                           errors;
        std::array<Byte, DiskFieldFormat::kSectorBytes>    edited    = MakeEditedBytes();
        HRESULT                                            hr        = S_OK;
        int                                                step      = 0;



        machine.PowerCycle();

        hr = machine.GetDiskStore().MountFromBytes (kSlot6, kDrive1, "thirteen.woz", DiskFormat::Woz, BuildThirteenSectorWoz());
        AssertSucceeded (hr, L"the 13-sector disk mounts");

        image = machine.GetDiskStore().GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (image);

        write = MakeWrite (*image);
        Assert::IsTrue (write.slot >= 0, L"track 0 has the sector");

        hr = SectorFieldWriter::Write (*image, std::span<const SectorWrite> (&write, 1), SectorWritePolicy::Editor, errors);
        AssertSucceeded (hr, L"the shared sector writer writes the 5-and-3 field");
        Assert::IsTrue (errors.empty(), L"with no errors");

        for (size_t i = 0; i < kNibbles.size(); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kDenibAddr + kNibbles[i]), static_cast<Byte> (i));
        }

        machine.GetMemoryBus().WriteByte (kGoodAddr,   0);
        machine.GetMemoryBus().WriteByte (kDoneAddr,   0);
        machine.GetMemoryBus().WriteByte (kTargetAddr, kSector);

        assembled = assembler.Assemble (kReadSource);
        Assert::IsTrue (assembled.success, assembled.errors.empty() ? L"the read routine assembles" : TextEncoding::NarrowToWide (assembled.errors[0].message).c_str());
        Assert::AreEqual (kCodeOrg, assembled.startAddress);

        for (size_t i = 0; i < assembled.bytes.size(); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (kCodeOrg + i), assembled.bytes[i]);
        }

        machine.GetCpu()->SetPC (kCodeOrg);

        for (step = 0; step < kMaxSteps && machine.GetMemoryBus().ReadByte (kDoneAddr) == 0; step++)
        {
            (void) machine.RunCycles (kStepCycles);
        }

        Assert::AreEqual (1, static_cast<int> (machine.GetMemoryBus().ReadByte (kDoneAddr)), L"the routine finds and reads the sector");
        Assert::AreEqual (1, static_cast<int> (machine.GetMemoryBus().ReadByte (kGoodAddr)), L"with a good checksum");

        for (size_t i = 0; i < edited.size(); i++)
        {
            Assert::AreEqual (static_cast<int> (edited[i]), static_cast<int> (machine.GetMemoryBus().ReadByte (static_cast<Word> (kOutAddr + i))), std::format (L"byte ${:02X}", i).c_str());
        }
    }
};
