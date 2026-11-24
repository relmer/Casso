#include "Pch.h"

#include "../UnitTest/EhmTestHelper.h"
#include "FixtureProvider.h"
#include "RealDiskFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures::SeedRealDisk
//
//  Seeds the fake platform with a real disk, so the command path is
//  exercised against material nobody here authored.
//
////////////////////////////////////////////////////////////////////////////////

void RealDiskFixtures::SeedRealDisk (
    FakeDiskFileIo  & io,
    const char      * fixture,
    const char      * asPath)
{
    FixtureProvider  fixtures;
    vector<Byte>     bytes;



    AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

    io.files[asPath]  = bytes;
    io.stamps[asPath] = FileStamp { bytes.size(), 100 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures::SeedProDosDisk
//
//  A ProDOS image with room to make directories on.
//
////////////////////////////////////////////////////////////////////////////////

void RealDiskFixtures::SeedProDosDisk (
    FakeDiskFileIo  & io)
{
    SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk");
}





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures::ExpectedMakeDumpPayload
//
//  The independently extracted copy, minus the four-byte DOS 3.3 header the
//  reader consumes. Comparing against these bytes is strictly stronger than
//  comparing a length: a length check passes under any corruption that
//  happens to preserve size, and the oracle is already sitting there.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> RealDiskFixtures::ExpectedMakeDumpPayload()
{
    FixtureProvider  fixtures;
    vector<Byte>     stored;



    AssertSucceeded (fixtures.OpenFixture ("Merlin/MAKE DUMP", stored));
    Assert::AreEqual (size_t (593), stored.size(), L"the stored file carries its header");

    return vector<Byte> (stored.begin() + 4, stored.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures::AssertIsMakeDumpPayload
//
////////////////////////////////////////////////////////////////////////////////

void RealDiskFixtures::AssertIsMakeDumpPayload (
    const vector<Byte>  & actual)
{
    vector<Byte>  expected = ExpectedMakeDumpPayload();
    size_t        i        = 0;



    // 589 is the length the header declares. Naming it here makes the
    // failure legible: 618 would mean 29 line-feed bytes were expanded on
    // the way out, which is the text-mode corruption this file's payload is
    // uniquely suited to expose.
    Assert::AreEqual (size_t (589), expected.size());
    Assert::AreEqual (expected.size(), actual.size());

    for (i = 0; i < expected.size(); i++)
    {
        if (actual[i] != expected[i])
        {
            Assert::Fail (L"payload differs from the independently extracted copy");
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures::OriginalImageBytes
//
//  The image exactly as it was seeded, kept for comparison. Read from the
//  fixture rather than from the fake, so a commit that corrupted the fake's
//  copy cannot also corrupt the oracle.
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> RealDiskFixtures::OriginalImageBytes()
{
    FixtureProvider  fixtures;
    vector<Byte>     bytes;



    AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", bytes));
    Assert::IsTrue (bytes.size() > 0, L"the oracle must actually have been read");

    return bytes;
}





