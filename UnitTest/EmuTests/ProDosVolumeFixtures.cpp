#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MakeVolume
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ProDosVolumeFixtures::MakeVolume (
    const std::string  & name)
{
    vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0xEE);



    AssertSucceeded (ProDosSkeleton::Write (buffer, name));

    return buffer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::At
//
////////////////////////////////////////////////////////////////////////////////

Byte ProDosVolumeFixtures::At (
    const vector<Byte>  & buffer,
    int                   block,
    size_t                offset)
{
    return buffer[ProDosSkeleton::GetBlockByteOffset (block, offset)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::WordAt
//
////////////////////////////////////////////////////////////////////////////////

Word ProDosVolumeFixtures::WordAt (
    const vector<Byte>  & buffer,
    int                   block,
    size_t                offset)
{
    return (Word) (At (buffer, block, offset) | (At (buffer, block, offset + 1) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MakePattern
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ProDosVolumeFixtures::MakePattern (
    size_t  count)
{
    vector<Byte>  bytes (count);
    size_t        i = 0;



    for (i = 0; i < count; i++)
    {
        bytes[i] = (Byte) ((i * 7 + 3) & 0xFF);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::BlockStamp
//
//  A byte that names the data BLOCK it belongs to, so a mis-ordered index
//  shows up as wrong content rather than merely a wrong length. Deliberately
//  not linear in the block number: any linear stamp repeats every 256
//  blocks, which is exactly where a tree's second index block begins.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProDosVolumeFixtures::BlockStamp (
    size_t  blockIndex)
{
    return (Byte) ((blockIndex * 31 + (blockIndex >> 8) * 101 + 7) & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MakeBinaryPayload
//
////////////////////////////////////////////////////////////////////////////////

FilePayload ProDosVolumeFixtures::MakeBinaryPayload (
    size_t  length,
    Word    loadAddress)
{
    FilePayload  payload;
    size_t       i = 0;



    payload.type           = ProDosVolume::kTypeBinary;
    payload.loadAddress    = loadAddress;
    payload.hasLoadAddress = true;

    payload.bytes.resize (length);

    for (i = 0; i < length; i++)
    {
        payload.bytes[i] = BlockStamp (i / kBlockBytes);
    }

    return payload;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::EntryOffset
//
////////////////////////////////////////////////////////////////////////////////

size_t ProDosVolumeFixtures::EntryOffset (
    int  slot)
{
    return kKeyBlockEntry + (size_t) slot * kEntryLength;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::KeyPointerOf
//
////////////////////////////////////////////////////////////////////////////////

Word ProDosVolumeFixtures::KeyPointerOf (
    const vector<Byte>  & vol,
    int                   dirBlock,
    int                   slot)
{
    return WordAt (vol, dirBlock, EntryOffset (slot) + kEntOffKeyPointer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::IndexPointerAt
//
//  One pointer out of an index block: 256 low bytes then the 256 matching
//  high bytes, which is the layout a pair-per-slot reader gets wrong only
//  for blocks numbered above 255.
//
////////////////////////////////////////////////////////////////////////////////

Word ProDosVolumeFixtures::IndexPointerAt (
    const vector<Byte>  & vol,
    int                   indexBlock,
    size_t                slot)
{
    return (Word) (At (vol, indexBlock, slot)
                | (At (vol, indexBlock, slot + kPointersPerIndex) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent
//
//  What a computed result must not disagree about.
//
//  IsClean is deliberately NOT the assertion even though a healthy ProDOS
//  volume happens to satisfy it -- the skeleton claims its own boot,
//  directory and bitmap blocks under a reserved owner, where the DOS 3.3
//  side leaves them allocated and unclaimed. Relying on that would still be
//  wrong here: a CORRECT delete that leaks reports blocks allocated with
//  nothing owning them, which is precisely the outcome the leak rule
//  requires, so a whole-volume yes/no would refuse it. Compare the result
//  against the input on the specific sets instead.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeFixtures::AssertEditLeftTheVolumeConsistent (
    const vector<Byte>  & before,
    const vector<Byte>  & after)
{
    ProDosVolume           wasVolume (before);
    ProDosVolume           isVolume (after);
    VolumeIntegrityReport  was;
    VolumeIntegrityReport  is;



    AssertSucceeded (wasVolume.BuildIntegrityReport (was));
    AssertSucceeded (isVolume.BuildIntegrityReport (is));

    Assert::AreEqual (size_t (0), is.GetCrossLinked().size(),
        L"the edit must not leave a block claimed by two entries");
    Assert::AreEqual (size_t (0), is.GetClaimedButFree().size(),
        L"the edit must not leave a referenced block marked free");
    Assert::AreEqual (was.GetUnfollowableChains().size(), is.GetUnfollowableChains().size(),
        L"the edit must not break a structure that was whole");
    Assert::AreEqual (was.GetAllocatedButUnclaimed().size(), is.GetAllocatedButUnclaimed().size(),
        L"the edit must not leak space, nor reclaim space it did not own");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MakeVolumeWithCrossLinkedFiles
//
//  Two files that both claim one data block, made by pointing the first
//  file's second index slot at the second file's second data block. The
//  block the first file used to own is left allocated and unclaimed, which
//  is what a cross-link costs even before anything is deleted.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ProDosVolumeFixtures::MakeVolumeWithCrossLinkedFiles (
    vector<Byte>  & outSurvivorBytes,
    Word          & outSharedBlock)
{
    vector<Byte>  vol   = MakeVolume();
    vector<Byte>  a     = MakePattern (600);
    vector<Byte>  b     = MakePattern (600);
    Word          keyA  = 0;
    Word          keyB  = 0;
    size_t        i     = 0;



    for (i = 0; i < b.size(); i++) { b[i] = (Byte) ~b[i]; }

    AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FILEA", 0x06, 0x2000, a));
    AssertSucceeded (ProDosFileWriter::WriteFile (vol, "FILEB", 0x06, 0x2000, b));

    keyA = KeyPointerOf (vol, 2, 1);
    keyB = KeyPointerOf (vol, 2, 2);

    outSharedBlock = IndexPointerAt (vol, keyB, 1);

    Assert::IsTrue (outSharedBlock != 0, L"the fixture must have a second data block to share");

    vol[ProDosSkeleton::GetBlockByteOffset (keyA, 1)] = (Byte) (outSharedBlock & 0xFF);
    vol[ProDosSkeleton::GetBlockByteOffset (keyA, 1 + kPointersPerIndex)] =
        (Byte) (outSharedBlock >> 8);

    outSurvivorBytes = b;

    return vol;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MarkFree
//
//  Volume bitmap: MSB of byte 0 is block 0, a SET bit means free.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosVolumeFixtures::MarkFree (
    vector<Byte>  & vol,
    uint32_t        block,
    bool            isFree)
{
    size_t  at   = ProDosSkeleton::GetBlockByteOffset (6, (size_t) (block / 8));
    Byte    mask = (Byte) (0x80 >> (block % 8));



    vol[at] = isFree ? (Byte) (vol[at] | mask) : (Byte) (vol[at] & ~mask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeFixtures::MakeVolumeWithSubdirectory
//
//  A blank volume with the directory SUB, and the file SUB/FILE inside it.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> ProDosVolumeFixtures::MakeVolumeWithSubdirectory()
{
    vector<Byte>  vol = MakeVolume();
    vector<Byte>  withDirectory;
    vector<Byte>  withFile;



    {
        ProDosVolume  volume (vol);

        AssertSucceeded (volume.CreateDirectory (FilePath::Parse ("SUB"), withDirectory));
    }

    {
        ProDosVolume  volume (withDirectory);

        AssertSucceeded (volume.Write (FilePath::Parse ("SUB/FILE"), MakeBinaryPayload (600, 0x2000), withFile));
    }

    return withFile;
}




