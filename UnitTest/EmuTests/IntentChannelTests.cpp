#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeIntentChannel.h"
#include "FakeDiskFileIo.h"
#include "Cli/Win32IntentChannel.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/BlankDiskBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IntentChannelTests
//
//  What a writing tool says about its own change, and what a receiver makes of
//  the bytes.
//
//  THE DECODE IS THE HALF WORTH ASSERTING HARDEST. Any process on the desktop
//  can address a WM_COPYDATA at the emulator's window, so every field is
//  attacker-controlled: the length, the intent byte, the path. None of it may be
//  trusted, and none of that judgement may sit inside a window procedure where
//  no test can reach it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IntentChannelTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\work\\Loader.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bin";



    //
    //  ------------------------------------------------------------------
    //  The payload.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (APayloadRoundTripsThroughTheWire)
    {
        const ExternalChangeIntent  intents[] = { ExternalChangeIntent::Unstated,
                                          ExternalChangeIntent::ReloadInPlace,
                                          ExternalChangeIntent::Restart };



        for (ExternalChangeIntent intent : intents)
        {
            std::vector<Byte>            bytes = Win32IntentChannel::Encode (kImagePath, intent);
            Win32IntentChannel::Payload  payload;

            Assert::IsTrue (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
            Assert::IsTrue (payload.intent == intent);
            Assert::AreEqual (std::string (kImagePath), payload.imagePath);
        }
    }



    TEST_METHOD (ATruncatedPayloadIsRefusedRatherThanReadPast)
    {
        std::vector<Byte>            bytes = Win32IntentChannel::Encode (kImagePath,
                                                                         ExternalChangeIntent::Restart);
        Win32IntentChannel::Payload  payload;



        //  Nothing at all.
        Assert::IsFalse (Win32IntentChannel::Decode (nullptr, 0, payload));
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), 0, payload));

        //  An intent and no path, which is a change to nothing.
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), 1, payload));
    }



    TEST_METHOD (AnOversizedPayloadIsRefused)
    {
        std::vector<Byte>            bytes (Win32IntentChannel::kMaxPayloadBytes + 1, 'x');
        Win32IntentChannel::Payload  payload;



        bytes[0] = (Byte) ExternalChangeIntent::Restart;

        //  A length no path could have is a message this channel did not send,
        //  and reading it would mean trusting a stranger's count.
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
    }



    TEST_METHOD (AnIntentValueThisBuildDoesNotKnowIsRefused)
    {
        std::vector<Byte>            bytes = Win32IntentChannel::Encode (kImagePath,
                                                                         ExternalChangeIntent::Restart);
        Win32IntentChannel::Payload  payload;



        bytes[0] = 0x7F;

        //  Reading the rest would be reading a message meant for something
        //  else, or for a later version of this.
        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
    }



    TEST_METHOD (ARefusedPayloadLeavesNothingBehindInTheOutput)
    {
        std::vector<Byte>            bytes = Win32IntentChannel::Encode (kImagePath,
                                                                         ExternalChangeIntent::Restart);
        Win32IntentChannel::Payload  payload;



        //  Read a good one first, so the out-parameter is carrying something.
        Assert::IsTrue (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));

        bytes[0] = 0x7F;

        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));

        //  A caller that ignored the return must not find last message's path
        //  sitting in it.
        Assert::IsTrue (payload.imagePath.empty());
        Assert::IsTrue (payload.intent == ExternalChangeIntent::Unstated);
    }



    TEST_METHOD (APathOfNothingButSpacesIsNotAPath)
    {
        std::vector<Byte>            bytes = Win32IntentChannel::Encode ("   ",
                                                                         ExternalChangeIntent::Restart);
        Win32IntentChannel::Payload  payload;



        Assert::IsFalse (Win32IntentChannel::Decode (bytes.data(), bytes.size(), payload));
    }



    //
    //  ------------------------------------------------------------------
    //  The announcement, from the writing side.
    //  ------------------------------------------------------------------
    //

    static CommandLineOptions  MakePut (ExternalChangeIntent intent)
    {
        CommandLineOptions  options;

        options.subcommand          = CommandLineOptions::Subcommand::Disk;
        options.disk.command        = CommandLineOptions::DiskOptions::Command::Put;
        options.disk.imagePath      = kImagePath;
        options.disk.hostFile       = kHostFile;
        options.disk.path           = "PROG";
        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x6000;
        options.disk.hasLoadAddress = true;
        options.disk.changeIntent   = intent;

        return options;
    }



    static void  SeedForPut (FakeDiskFileIo & io, const std::vector<Byte> & image)
    {
        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };
        io.files[kHostFile]   = std::vector<Byte> (64, 0x42);
        io.stamps[kHostFile]  = FileStamp { 64, 100 };
    }



    //  A blank DOS 3.3 disk, which is enough for `put` to land a file on.
    static std::vector<Byte>  MakeBlankImage()
    {
        std::vector<Byte>  bytes;
        BlankDiskSpec      spec;
        BootPayload        payload;

        spec.format   = DiskFormat::Dsk;
        spec.contents = BlankDiskContents::Dos33;
        spec.bootable = false;

        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, bytes));

        return bytes;
    }



    TEST_METHOD (AStatedIntentIsAnnouncedAfterAWriteSucceeds)
    {
        FakeDiskFileIo     io;
        FakeIntentChannel  channel;
        DiskCommandResult  result;



        SeedForPut (io, MakeBlankImage());

        {
            DiskCommandRunner  runner (io);

            runner.SetIntentChannel (&channel);
            result = runner.Run (MakePut (ExternalChangeIntent::Restart));
        }

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual ((size_t) 1, channel.stated.size());
        Assert::AreEqual (std::string (kImagePath), channel.stated[0].imagePath);
        Assert::IsTrue (channel.stated[0].intent == ExternalChangeIntent::Restart);
    }



    TEST_METHOD (NoStatedIntentAnnouncesNothing)
    {
        FakeDiskFileIo     io;
        FakeIntentChannel  channel;
        DiskCommandResult  result;



        SeedForPut (io, MakeBlankImage());

        {
            DiskCommandRunner  runner (io);

            runner.SetIntentChannel (&channel);
            result = runner.Run (MakePut (ExternalChangeIntent::Unstated));
        }

        //  Every invocation without the flag takes this path, and an emulator
        //  that heard "nothing in particular" would have learned nothing it
        //  did not already get from watching the file.
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual ((size_t) 0, channel.stated.size());
    }



    TEST_METHOD (AFailedWriteAnnouncesNothing)
    {
        FakeDiskFileIo     io;
        FakeIntentChannel  channel;
        DiskCommandResult  result;
        CommandLineOptions options = MakePut (ExternalChangeIntent::Restart);



        SeedForPut (io, MakeBlankImage());

        //  The host file is not there, so nothing is written.
        io.files.erase (kHostFile);

        {
            DiskCommandRunner  runner (io);

            runner.SetIntentChannel (&channel);
            result = runner.Run (options);
        }

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual ((size_t) 0, channel.stated.size(),
                          L"an intent after a write that did not happen would send "
                          L"an emulator to re-read a file nothing touched");
    }



    TEST_METHOD (AReadOnlyCommandAnnouncesNothingEvenWithAnIntent)
    {
        FakeDiskFileIo     io;
        FakeIntentChannel  channel;
        CommandLineOptions options;
        DiskCommandResult  result;



        SeedForPut (io, MakeBlankImage());

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::List;
        options.disk.imagePath    = kImagePath;
        options.disk.changeIntent = ExternalChangeIntent::Restart;

        {
            DiskCommandRunner  runner (io);

            runner.SetIntentChannel (&channel);
            result = runner.Run (options);
        }

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual ((size_t) 0, channel.stated.size(), L"listing changes nothing");
    }



    TEST_METHOD (NoChannelAtAllIsNotAnError)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        SeedForPut (io, MakeBlankImage());

        {
            //  Which is also the case where no emulator is running: the writer
            //  cannot know, and a build script must behave the same either way.
            DiskCommandRunner  runner (io);

            result = runner.Run (MakePut (ExternalChangeIntent::Restart));
        }

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }



    TEST_METHOD (StatingAnIntentChangesNotOneWrittenByte)
    {
        FakeDiskFileIo     withIntent;
        FakeDiskFileIo     without;
        FakeIntentChannel  channel;
        std::vector<Byte>  blank = MakeBlankImage();



        SeedForPut (withIntent, blank);
        SeedForPut (without,    blank);

        {
            DiskCommandRunner  runner (withIntent);

            runner.SetIntentChannel (&channel);
            AssertSucceededStatus (runner.Run (MakePut (ExternalChangeIntent::Restart)));
        }

        {
            DiskCommandRunner  runner (without);

            AssertSucceededStatus (runner.Run (MakePut (ExternalChangeIntent::Unstated)));
        }

        //  The flag says what a change MEANS. It must not be able to change
        //  what the change IS.
        Assert::IsTrue (withIntent.files[kImagePath] == without.files[kImagePath],
                        L"byte for byte, --on-change writes what its absence writes");
    }



    static void  AssertSucceededStatus (const DiskCommandResult & result)
    {
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }
};
