#include "Pch.h"

#include "EmuTests/FakeDiskFileIo.h"
#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"
#include "Cli/AssemblerMode.h"
#include "Cli/ImageArtifactSink.h"
#include "Devices/Disk/AssembledFilePlacement.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/ProDosSkeleton.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk/VolumeImage.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AssemblerToDiskTests
{
    static constexpr const char *  kImagePath        = "work.dsk";
    static constexpr Byte          kBlankVolumeNum   = 254;
    static constexpr const char *  kProDosVolumeName = "WORK";



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  An assembly, a disk that exists only in memory, and the sink between
    //  them. Nothing here touches a real file: the platform is behind the
    //  file-I/O seam and the volume is a byte buffer.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblyResult Assemble (const std::string & source,
                                        DialectId           dialect = DialectId::Merlin)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  options = {};

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }



        static vector<Byte> MakeDos33Image()
        {
            vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

            AssertSucceeded (Dos33Skeleton::Write (buffer, kBlankVolumeNum));

            return buffer;
        }



        static vector<Byte> MakeProDosImage()
        {
            vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

            AssertSucceeded (ProDosSkeleton::Write (buffer, kProDosVolumeName));

            return buffer;
        }



        static void SeedImage (FakeDiskFileIo & io, const vector<Byte> & bytes)
        {
            io.files[kImagePath]  = bytes;
            io.stamps[kImagePath] = FileStamp { bytes.size(), 100 };
        }



        //  The options an assembly targeting the image would arrive with.
        static CommandLineOptions ImageOptions (const char * onDiskName,
                                                const char * typeName = "")
        {
            CommandLineOptions  options;

            options.imagePath     = kImagePath;
            options.onDiskName    = onDiskName;
            options.imageTypeName = typeName;

            return options;
        }



        //  One catalog entry off the committed image, so a test asserts what
        //  the volume records rather than what the sink believed it wrote.
        static bool FindEntry (const vector<Byte> & imageBytes,
                               const std::string  & name,
                               FileEntry          & outEntry)
        {
            Dos33Volume    volume (imageBytes);
            VolumeListing  listing;
            bool           found = false;

            AssertSucceeded (volume.Enumerate (listing));

            for (const FileEntry & entry : listing.entries)
            {
                if (entry.name == name)
                {
                    outEntry = entry;
                    found    = true;
                }
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AssembleOntoDiskTests
    //
    //  The whole feature in one action: an object lands on a volume, named and
    //  typed, at the address its source declared.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AssembleOntoDiskTests)
    {
    public:

        TEST_METHOD (ObjectLandsOnTheVolumeUnderTheGivenName)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            FileEntry           entry;

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            Assert::IsTrue (Fixture::FindEntry (io.files[kImagePath], "PROG", entry),
                            L"the object is on the volume under the name it was given");
        }



        //  The correctness half of the feature. No option supplied an address;
        //  the only place $6000 can have come from is the source.
        TEST_METHOD (LoadAddressComesFromTheSourceOrigin)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            FilePayload         readBack;
            vector<Byte>        expect  = { 0xA9, 0x11, 0x60 };

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            //  Read back rather than enumerated: DOS 3.3 keeps the load address
            //  in the file's own header, so reading the file is what reports it
            //  and what a guest loading it would see.
            {
                Dos33Volume  committed (io.files[kImagePath]);

                AssertSucceeded (committed.Read (FilePath::Parse ("PROG"), readBack));
            }

            Assert::IsTrue (readBack.hasLoadAddress, L"the file records a load address");
            Assert::AreEqual ((int) 0x6000, (int) readBack.loadAddress,
                              L"and it is the origin the source stated");
            Assert::IsTrue (expect == readBack.bytes, L"the bytes are the assembled ones");
        }



        //  A missing image is refused rather than created. Creating one means
        //  choosing a container, a filesystem and a volume name, which the
        //  disk-creation command already owns.
        TEST_METHOD (MissingImage_IsRefusedAndNothingIsCreated)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            HRESULT             written = S_OK;

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"an image that is not there is refused");
            Assert::AreEqual ((size_t) 0, io.files.size(),
                              L"and no disk was brought into existence");
            Assert::IsTrue (sink.GetDiagnostics().size() > 0, L"the refusal says something");
        }



        //  An image held by another program is refused, and the bytes it holds
        //  are left exactly as they were.
        TEST_METHOD (ImageHeldByAnotherProgram_IsRefusedAndUnchanged)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            vector<Byte>        before  = Fixture::MakeDos33Image();
            HRESULT             written = S_OK;

            Fixture::SeedImage (io, before);
            io.reportHeldByOther = true;

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"a held image is refused");
            Assert::IsTrue (before == io.files[kImagePath], L"and is byte for byte as it was");
        }



        //  A source that emitted nothing has nothing to place. Writing an empty
        //  file would replace a real one with nothing under the replace rule.
        TEST_METHOD (AssemblyEmittingNothing_IsRefusedAndImageUnchanged)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble ("* nothing but a comment\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            vector<Byte>        before  = Fixture::MakeDos33Image();
            HRESULT             written = S_OK;

            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"an assembly with no bytes has nothing to write");
            Assert::IsTrue (before == io.files[kImagePath], L"and the image is untouched");
        }



        //  An unrecognized type is refused by name rather than approximated. A
        //  guessed type surfaces much later as a program that will not load.
        TEST_METHOD (UnrecognizedType_IsRefusedNamingTheValue)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG", "ZZZ");
            vector<Byte>        before  = Fixture::MakeDos33Image();
            HRESULT             written = S_OK;

            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"an unknown type is refused");
            Assert::IsTrue (sink.GetDiagnostics().find ("ZZZ") != std::string::npos,
                            L"and the refusal names the value that was not recognized");
            Assert::IsTrue (before == io.files[kImagePath], L"the image is unchanged");
        }



        //  The type a build loop produces, when nothing named one.
        TEST_METHOD (NoTypeNamed_FilesAsBinary)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $6000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            FileEntry           entry;

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));
            Assert::IsTrue (Fixture::FindEntry (io.files[kImagePath], "PROG", entry), L"expected the file");

            Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) entry.type,
                              L"an assembly with no stated type is a binary");
        }



        //  A ProDOS volume takes the same path, since the capability belongs to
        //  the assembler rather than to one filesystem.
        TEST_METHOD (ProDosVolume_TakesTheObjectToo)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $2000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            ProDosVolume        volume (io.files[kImagePath]);
            VolumeListing       listing;
            bool                found   = false;

            Fixture::SeedImage (io, Fixture::MakeProDosImage());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            {
                ProDosVolume  committed (io.files[kImagePath]);

                AssertSucceeded (committed.Enumerate (listing));
            }

            for (const FileEntry & entry : listing.entries)
            {
                if (entry.name == "PROG")
                {
                    found = true;
                    Assert::AreEqual ((int) 0x2000, (int) entry.auxType,
                                      L"ProDOS records the origin as the auxiliary type");
                }
            }

            Assert::IsTrue (found, L"the object is on the ProDOS volume");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceStatedTypeTests
    //
    //  The file-type directive, which states a ProDOS type byte and now has
    //  somewhere for it to land.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceStatedTypeTests)
    {
    public:

        //  Which type byte the volume ended up recording, for a source that
        //  stated one.
        static Byte TypeAfterWriting (const char * source, bool proDos, HRESULT & outResult,
                                      std::string & outDiagnostics, const char * flagType = "")
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (source);
            CommandLineOptions  options = Fixture::ImageOptions ("PROG", flagType);
            FileEntry           entry;
            Byte                type    = 0;

            Fixture::SeedImage (io, proDos ? Fixture::MakeProDosImage() : Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            outResult      = sink.WriteBinary (result, options);
            outDiagnostics = sink.GetDiagnostics();

            if (SUCCEEDED (outResult))
            {
                if (proDos)
                {
                    ProDosVolume   volume (io.files[kImagePath]);
                    VolumeListing  listing;

                    AssertSucceeded (volume.Enumerate (listing));

                    for (const FileEntry & found : listing.entries)
                    {
                        if (found.name == "PROG")
                        {
                            type = found.type;
                        }
                    }
                }
                else if (Fixture::FindEntry (io.files[kImagePath], "PROG", entry))
                {
                    type = entry.type;
                }
            }

            return type;
        }



        TEST_METHOD (BinaryTypeMapsToBothFilesystems)
        {
            HRESULT      hr    = S_OK;
            std::string  diag;
            Byte         dos   = TypeAfterWriting (" ORG $300\n TYP $06\n LDA #$11\n RTS\n", false, hr, diag);
            Byte         pro   = TypeAfterWriting (" ORG $300\n TYP $06\n LDA #$11\n RTS\n", true,  hr, diag);

            Assert::AreEqual ((int) Dos33Volume::kTypeBinary,  (int) dos, L"DOS 3.3 binary");
            Assert::AreEqual ((int) ProDosVolume::kTypeBinary, (int) pro, L"ProDOS binary");
        }



        TEST_METHOD (TextTypeMapsToBothFilesystems)
        {
            HRESULT      hr  = S_OK;
            std::string  diag;
            Byte         dos = TypeAfterWriting (" ORG $300\n TYP $04\n LDA #$11\n RTS\n", false, hr, diag);
            Byte         pro = TypeAfterWriting (" ORG $300\n TYP $04\n LDA #$11\n RTS\n", true,  hr, diag);

            Assert::AreEqual ((int) Dos33Volume::kTypeText,  (int) dos, L"DOS 3.3 text");
            Assert::AreEqual ((int) ProDosVolume::kTypeText, (int) pro, L"ProDOS text");
        }



        TEST_METHOD (ApplesoftTypeMapsToBothFilesystems)
        {
            HRESULT      hr  = S_OK;
            std::string  diag;
            Byte         dos = TypeAfterWriting (" ORG $300\n TYP $FC\n LDA #$11\n RTS\n", false, hr, diag);
            Byte         pro = TypeAfterWriting (" ORG $300\n TYP $FC\n LDA #$11\n RTS\n", true,  hr, diag);

            Assert::AreEqual ((int) Dos33Volume::kTypeApplesoft, (int) dos, L"DOS 3.3 Applesoft");
            Assert::AreEqual ((int) ProDosVolume::kTypeBasic,    (int) pro, L"ProDOS Applesoft");
        }



        //  The refusal that must not become an approximation. Nothing in
        //  DOS 3.3 means what a system file means, so any answer is a guess.
        TEST_METHOD (SystemTypeIsRefusedOnDos33NamingTypeAndFilesystem)
        {
            HRESULT      hr = S_OK;
            std::string  diag;

            TypeAfterWriting (" ORG $300\n TYP $FF\n LDA #$11\n RTS\n", false, hr, diag);

            Assert::IsTrue (FAILED (hr), L"a system file has no DOS 3.3 equivalent");
            Assert::IsTrue (diag.find ("$FF") != std::string::npos, L"the refusal names the type");
            Assert::IsTrue (diag.find ("DOS 3.3") != std::string::npos, L"and the filesystem");
        }



        TEST_METHOD (SystemTypeIsAcceptedOnProDos)
        {
            HRESULT      hr  = S_OK;
            std::string  diag;
            Byte         pro = TypeAfterWriting (" ORG $300\n TYP $FF\n LDA #$11\n RTS\n", true, hr, diag);

            Assert::IsTrue (SUCCEEDED (hr), L"ProDOS has system files");
            Assert::AreEqual ((int) ProDosVolume::kTypeSystem, (int) pro, L"filed as a system program");
        }



        TEST_METHOD (UnrecognizedTypeByteIsRefusedNamingTheByte)
        {
            HRESULT      hr = S_OK;
            std::string  diag;

            TypeAfterWriting (" ORG $300\n TYP $99\n LDA #$11\n RTS\n", true, hr, diag);

            Assert::IsTrue (FAILED (hr), L"a type outside the recognized set is refused");
            Assert::IsTrue (diag.find ("$99") != std::string::npos, L"and the refusal names the value");
        }



        //  The precedence the tool already applies to the object's name,
        //  settled by the layer that sees both.
        //  A binary named as a DOS 3.3 greeting leaves the disk booting and the
        //  program never running, because the command DOS issues at boot is RUN.
        //  The same rule the boot command applies, not a second copy of it.
        TEST_METHOD (StartupProgramIsRefusedWhenDos33WouldNotRunIt)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            vector<Byte>        before  = Fixture::MakeDos33Image();
            HRESULT             written = S_OK;

            options.setStartupProgram = true;

            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"a volume that cannot run the file is refused");
            Assert::IsTrue (sink.GetDiagnostics().size() > 0, L"and says so");
            Assert::IsTrue (before == io.files[kImagePath],
                            L"refused before anything was written, so the image is untouched");
        }



        //  The rule itself, asserted where it lives. This is the one both
        //  routes to a startup program call, so a test on the rule covers both
        //  and a change to it cannot make them disagree.
        TEST_METHOD (TheGreetingRuleAcceptsRunnableTypesAndRefusesABinary)
        {
            VolumeListing  listing;
            FileEntry      binary;
            FileEntry      applesoft;
            FileEntry      integer;

            binary.name    = "PROG";
            binary.type    = Dos33Volume::kTypeBinary;
            applesoft.name = "HELLO";
            applesoft.type = Dos33Volume::kTypeApplesoft;
            integer.name   = "INTHELLO";
            integer.type   = Dos33Volume::kTypeInteger;

            listing.entries = { binary, applesoft, integer };

            Assert::IsFalse (Dos33Volume::IsRunnableAsGreeting (listing, "PROG"),
                             L"a booting DOS 3.3 RUNs its greeting, and RUN cannot start a binary");
            Assert::IsTrue (Dos33Volume::IsRunnableAsGreeting (listing, "HELLO"),
                            L"Applesoft is what RUN understands");
            Assert::IsTrue (Dos33Volume::IsRunnableAsGreeting (listing, "INTHELLO"),
                            L"and Integer BASIC likewise");

            //  A name not on the volume answers true, because the refusal for
            //  that belongs to whichever layer looked it up.
            Assert::IsTrue (Dos33Volume::IsRunnableAsGreeting (listing, "ABSENT"),
                            L"one refusal per problem");
        }



        //  ProDOS boots by finding a system-typed entry, so a system file named
        //  as the startup program is exactly what it wants.
        TEST_METHOD (StartupProgramIsSetOnProDos)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $2000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG.SYSTEM", "SYS");
            vector<Byte>        before  = Fixture::MakeProDosImage();

            options.setStartupProgram = true;

            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            Assert::IsFalse (before == io.files[kImagePath], L"the volume was written");
        }



        TEST_METHOD (CommandLineTypeBeatsTheSourceDirective)
        {
            HRESULT      hr  = S_OK;
            std::string  diag;
            Byte         dos = TypeAfterWriting (" ORG $300\n TYP $04\n LDA #$11\n RTS\n", false, hr, diag, "B");

            Assert::IsTrue (SUCCEEDED (hr), L"the flag is honored");
            Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) dos,
                              L"the command line wins over the source's text type");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AllOrNothingTests
    //
    //  What the image holds after a write that did not finish.
    //
    //  THE GUARANTEE IS STRUCTURAL, WHICH IS EXACTLY WHY IT NEEDS ASSERTING.
    //  The volume layer never mutates in place and the sink commits once at the
    //  end, so nothing has to remember to undo itself -- and a reading of the
    //  code was all the evidence there was. A commit moved inside the loop would
    //  pass every other test in this file.
    //
    //  The failure is provoked with a name DOS 3.3 cannot store: it begins with
    //  a digit, which the catalog encoding refuses. The assembly itself
    //  succeeds, so the WRITE is what fails, which is the case the guarantee is
    //  about. An assembly that failed would never reach the sink at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AllOrNothingTests)
    {
    public:

        static constexpr int     kVtocTrack          = 17;
        static constexpr int     kCatalogFirstSector = 15;
        static constexpr size_t  kEntryBase          = 0x0B;
        static constexpr size_t  kEntOffType         = 0x02;
        static constexpr Byte    kLockedBit          = 0x80;

        //  An image already carrying one file, so a test can tell "nothing was
        //  written" apart from "the volume was blank anyway".
        static vector<Byte> ImageHoldingOneFile (const char * name, const std::string & source)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (source);
            CommandLineOptions  options = Fixture::ImageOptions (name);

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            return io.files[kImagePath];
        }



        //  The bytes of one file as a guest loading it would read them.
        static vector<Byte> ReadBack (const vector<Byte> & imageBytes, const char * name)
        {
            Dos33Volume  volume (imageBytes);
            FilePayload  payload;

            AssertSucceeded (volume.Read (FilePath::Parse (name), payload));

            return payload.bytes;
        }



        //  Sets the lock bit on the first catalog entry, which is where a file
        //  placed onto a freshly formatted volume lands.
        static void LockFirstCatalogEntry (vector<Byte> & buffer)
        {
            size_t  at = Dos33Skeleton::SectorOffset (kVtocTrack, kCatalogFirstSector)
                       + kEntryBase + kEntOffType;

            buffer[at] = (Byte) (buffer[at] | kLockedBit);
        }



        //  Replacement is the common path in a build loop, so it is worth
        //  asserting that the second write leaves the second program and not
        //  some mixture of the two.
        TEST_METHOD (WritingOverAFileLeavesOnlyTheNewBytes)
        {
            FakeDiskFileIo      io;
            AssemblyResult      second  = Fixture::Assemble (" ORG $300\n LDA #$22\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            vector<Byte>        expect  = { 0xA9, 0x22, 0x60 };

            Fixture::SeedImage (io, ImageHoldingOneFile ("PROG", " ORG $300\n LDA #$11\n RTS\n"));

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (second, options));

            Assert::IsTrue (expect == ReadBack (io.files[kImagePath], "PROG"),
                            L"the file holds the second program, whole");
        }



        //  THE CENTRAL PROMISE. The first output is perfectly writable and the
        //  second is not, so a sink that committed as it went would leave GOOD
        //  on the volume and report failure.
        TEST_METHOD (AFailureOnALaterOutputLeavesTheImageExactlyAsItWas)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $300\n LDA #$11\n RTS\n SAV GOOD\n"
                                                             " LDA #$22\n RTS\n SAV 9BAD\n");
            CommandLineOptions  options = Fixture::ImageOptions ("");
            vector<Byte>        before  = ImageHoldingOneFile ("PROG", " ORG $300\n LDA #$11\n RTS\n");
            HRESULT             written = S_OK;
            FileEntry           entry;

            Assert::AreEqual ((size_t) 2, result.savePoints.size(), L"two outputs, or this proves nothing");

            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"a name the volume cannot store is refused");
            Assert::IsTrue (before == io.files[kImagePath],
                            L"and the image is byte for byte as it was");
            Assert::IsFalse (Fixture::FindEntry (io.files[kImagePath], "GOOD", entry),
                             L"the output that WOULD have written is not there either");
        }



        //  The half of the all-or-nothing rule that a replacement puts at risk:
        //  the first output overwrites a file that already exists, and the
        //  second output fails. The old file must come through untouched.
        TEST_METHOD (AReplacementAbandonedPartWayLeavesTheOldFileIntact)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result   = Fixture::Assemble (" ORG $300\n LDA #$33\n RTS\n SAV PROG\n"
                                                              " LDA #$44\n RTS\n SAV 9BAD\n");
            CommandLineOptions  options  = Fixture::ImageOptions ("");
            vector<Byte>        original = { 0xA9, 0x11, 0x60 };
            HRESULT             written  = S_OK;

            Fixture::SeedImage (io, ImageHoldingOneFile ("PROG", " ORG $300\n LDA #$11\n RTS\n"));

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"the second output cannot be written");
            Assert::IsTrue (original == ReadBack (io.files[kImagePath], "PROG"),
                            L"and the file that was there still holds what it held");
        }



        //  The filesystem's own protection, surfaced rather than overridden.
        //  The refusal lives in the volume layer; what this asserts is that the
        //  assembler path takes it rather than writing anyway.
        TEST_METHOD (ALockedFileIsRefusedAndKeepsItsBytes)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (" ORG $300\n LDA #$22\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            vector<Byte>        before  = ImageHoldingOneFile ("PROG", " ORG $300\n LDA #$11\n RTS\n");
            HRESULT             written = S_OK;

            LockFirstCatalogEntry (before);
            Fixture::SeedImage (io, before);

            ImageArtifactSink  sink (io);

            written = sink.WriteBinary (result, options);

            Assert::IsTrue (FAILED (written), L"a locked file is not replaced");
            Assert::IsTrue (before == io.files[kImagePath], L"and the image is unchanged");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectParityTests
    //
    //  The same program, written in either dialect, placed identically.
    //
    //  THE CAPABILITY BELONGS TO THE ASSEMBLER AND THE DIRECTIVES ONLY FEED IT.
    //  A dialect must not have to own directives to reach a disk: AS65 has no
    //  equivalent of DSK, TYP or SAV and still writes onto a volume through the
    //  same flags. Nothing else in this file would catch that guarantee decaying
    //  into a Merlin-only path.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectParityTests)
    {
    public:

        //  One image, one program, under whichever dialect was named.
        static vector<Byte> PlaceUnder (DialectId dialect, const std::string & source)
        {
            FakeDiskFileIo      io;
            AssemblyResult      result  = Fixture::Assemble (source, dialect);
            CommandLineOptions  options = Fixture::ImageOptions ("PROG", "B");

            Assert::IsTrue (result.success, L"the source must assemble under the dialect it was written for");

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            AssertSucceeded (sink.WriteBinary (result, options));

            return io.files[kImagePath];
        }



        TEST_METHOD (TheSameProgramPlacesIdenticallyUnderEitherDialect)
        {
            vector<Byte>  viaMerlin = PlaceUnder (DialectId::Merlin, " ORG $6000\n LDA #$11\n RTS\n");
            vector<Byte>  viaAs65   = PlaceUnder (DialectId::As65,   "        .org $6000\n"
                                                                     "        lda #$11\n"
                                                                     "        rts\n");

            Assert::IsTrue (viaMerlin == viaAs65,
                            L"the image is the same whichever dialect assembled the program");
        }
    };


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RefusalsReachTheReaderTests
    //
    //  That a refusal on the disk path is SAID, not merely returned.
    //
    //  THIS IS THE GAP EVERY OTHER TEST IN THIS FILE LEFT OPEN. They assert
    //  that the sink PRODUCES its diagnostics, which it always did; nothing
    //  asserted that anything reads them back. For one release nothing did, and
    //  every refusal here -- no image, a type the filesystem has no equivalent
    //  for, a volume full, a locked file, an illegal name, an image held by
    //  another program -- exited non-zero and printed absolutely nothing.
    //
    //  Only running the tool found it, which is why the assertion is made
    //  against what a reader would see rather than against the sink.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RefusalsReachTheReaderTests)
    {
    public:

        //  Standard error, captured while one assembly runs.
        class CapturedErrors
        {
        public:

            CapturedErrors() : m_saved (std::cerr.rdbuf (m_text.rdbuf()))
            {
            }

            ~CapturedErrors()
            {
                std::streambuf *  discarded = std::cerr.rdbuf (m_saved);

                (void) discarded;
            }

            CapturedErrors (const CapturedErrors &)             = delete;
            CapturedErrors & operator= (const CapturedErrors &) = delete;

            std::string  Text() const
            {
                return m_text.str();
            }

        private:

            std::ostringstream  m_text;
            std::streambuf   *  m_saved;
        };



        //  A source reader that hands back one fixed text, so a run needs no
        //  file on the host.
        class OneSource : public FileReader
        {
        public:

            explicit OneSource (const std::string & text) : m_text (text)
            {
            }

            FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) override
            {
                FileReadResult  result;

                (void) filename;
                (void) baseDir;

                result.success  = true;
                result.contents = m_text;

                return result;
            }

        private:

            std::string  m_text;
        };



        //  THE REFUSAL A READER ACTUALLY SEES. The sink carries its words and
        //  the mode is what prints them, so this drives the mode rather than the
        //  sink: asking the sink what it recorded is exactly the question that
        //  stayed answerable while the tool said nothing.
        TEST_METHOD (ATypeTheFilesystemCannotHold_IsSaidOutLoud)
        {
            FakeDiskFileIo      io;
            OneSource           source (" TYP $FF\n ORG $2000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("SYSPROG");
            std::string         said;
            int                 exitCode = 0;

            options.dialect   = DialectId::Merlin;
            options.inputFile = "SYSPROG.S";

            Fixture::SeedImage (io, Fixture::MakeDos33Image());

            ImageArtifactSink  sink (io);

            {
                CapturedErrors  captured;

                std::unique_ptr<AssemblerMode>  mode = AssemblerMode::CreateFor (DialectId::Merlin);
                HRESULT                         ran  = mode->Run (options, exitCode, &source, &sink);

                said = captured.Text();

                Assert::IsTrue (FAILED (ran), L"the run reports the failure as well as printing it");
            }

            Assert::AreNotEqual (0, exitCode, L"a type DOS 3.3 has no equivalent for is refused");
            Assert::IsTrue (said.find ("$FF") != std::string::npos,
                            L"and the reader is told which type");
            Assert::IsTrue (said.find ("DOS 3.3") != std::string::npos,
                            L"and which filesystem could not hold it");
        }



        //  The same, for the refusal a caller meets first.
        TEST_METHOD (AMissingImage_IsSaidOutLoud)
        {
            FakeDiskFileIo      io;
            OneSource           source (" ORG $2000\n LDA #$11\n RTS\n");
            CommandLineOptions  options = Fixture::ImageOptions ("PROG");
            std::string         said;
            int                 exitCode = 0;

            options.dialect   = DialectId::Merlin;
            options.inputFile = "PROG.S";

            ImageArtifactSink  sink (io);

            {
                CapturedErrors  captured;

                std::unique_ptr<AssemblerMode>  mode = AssemblerMode::CreateFor (DialectId::Merlin);
                HRESULT                         ran  = mode->Run (options, exitCode, &source, &sink);

                said = captured.Text();

                Assert::IsTrue (FAILED (ran), L"the run reports the failure as well as printing it");
            }

            Assert::AreNotEqual (0, exitCode, L"an image that is not there is refused");
            Assert::IsTrue (said.find (kImagePath) != std::string::npos,
                            L"and the reader is told which image");
        }
    };


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinFileTypeSetTests
    //
    //  Which type bytes a source may state, against the set Merlin accepts.
    //
    //  THE SET IS QUOTED FROM THE MANUAL: "Valid file types are 0,6,$F0~$F7, and
    //  $FF (no type, BIN, CMD, user defined, SYS)". Four of those five were
    //  refused for a release -- only $06 and $FF were recognized -- so a genuine
    //  Merlin source stating $00 or a command type was told the tool did not
    //  recognize it. That is a port failing on a construct the assembler being
    //  copied assembles.
    //
    //  The sweep runs the WHOLE set rather than a sample, in both directions:
    //  every type Merlin accepts must be accepted on ProDOS, and a byte outside
    //  the set must still be refused, because accepting everything would pass
    //  this suite just as well.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinFileTypeSetTests)
    {
    public:

        //  Where a stated type ends up, or why it could not be honored.
        static HRESULT Place (VolumeKind kind, Byte stated, Byte & outType, std::string & outError)
        {
            return AssembledFilePlacement::ResolveSourceType (kind, stated, outType, outError);
        }



        //  Merlin's own set, byte for byte from the manual.
        TEST_METHOD (EveryTypeMerlinAcceptsIsAcceptedOnProDos)
        {
            Byte  merlinSet[] = { 0x00, 0x06, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xFF };

            for (Byte stated : merlinSet)
            {
                Byte         resolved = 0;
                std::string  error;

                AssertSucceeded (Place (VolumeKind::ProDos, stated, resolved, error));

                Assert::AreEqual ((int) stated, (int) resolved,
                                  L"a ProDOS type passes through as the byte the source stated");
                Assert::IsTrue   (error.empty(), L"and nothing is said about it");
            }
        }



        //  Ours, beyond Merlin, and running in the safe direction: accepting
        //  them refuses no source Merlin accepts.
        TEST_METHOD (TextAndApplesoftAreAcceptedThoughMerlinListsNeither)
        {
            Byte         text      = 0;
            Byte         applesoft = 0;
            std::string  error;

            AssertSucceeded (Place (VolumeKind::ProDos, 0x04, text, error));
            AssertSucceeded (Place (VolumeKind::ProDos, 0xFC, applesoft, error));

            Assert::AreEqual ((int) 0x04, (int) text,      L"text passes through");
            Assert::AreEqual ((int) 0xFC, (int) applesoft, L"and Applesoft does");
        }



        //  A byte outside the set is refused NAMING ITSELF, which is what Merlin
        //  does with ILLEGAL FILE TYPE. Without this the sweep above would pass
        //  on an implementation that accepted everything.
        TEST_METHOD (AByteOutsideTheSetIsRefusedNamingItself)
        {
            Byte         resolved = 0;
            std::string  error;
            HRESULT      placed   = Place (VolumeKind::ProDos, 0xE0, resolved, error);

            Assert::IsTrue (FAILED (placed),
                            L"$E0 is in neither Merlin's set nor ours");
            Assert::IsTrue (error.find ("$E0") != std::string::npos,
                            L"and the refusal names the byte");
        }



        //  Only three of the set have a DOS 3.3 counterpart. The rest are
        //  refused by name rather than approximated, which is the rule that
        //  keeps a program from being filed under a type that cannot start it.
        TEST_METHOD (OnDos33OnlyTheTypesWithCounterpartsMap)
        {
            Byte         resolved = 0;
            std::string  error;

            AssertSucceeded (Place (VolumeKind::Dos33, 0x04, resolved, error));
            Assert::AreEqual ((int) Dos33Volume::kTypeText, (int) resolved, L"text maps");

            AssertSucceeded (Place (VolumeKind::Dos33, 0x06, resolved, error));
            Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) resolved, L"binary maps");

            AssertSucceeded (Place (VolumeKind::Dos33, 0xFC, resolved, error));
            Assert::AreEqual ((int) Dos33Volume::kTypeApplesoft, (int) resolved, L"Applesoft maps");
        }



        //  Each refusal says which type and why, rather than one message
        //  covering three different reasons.
        TEST_METHOD (OnDos33TheTypesWithNoCounterpartAreRefusedByName)
        {
            struct Case { Byte stated; const char * says; };

            Case  cases[] =
            {
                { 0x00, "typeless"          },
                { 0xF0, "command"           },
                { 0xF7, "command"           },
                { 0xFF, "system"            },
            };

            for (const Case & one : cases)
            {
                Byte         resolved = 0;
                std::string  error;
                HRESULT      placed   = Place (VolumeKind::Dos33, one.stated, resolved, error);

                Assert::IsTrue (FAILED (placed),
                                L"DOS 3.3 has no equivalent for this type");
                Assert::IsTrue (error.find (one.says) != std::string::npos,
                                L"and the refusal says which kind of file it was");
                Assert::IsTrue (error.find ("DOS 3.3") != std::string::npos,
                                L"and which filesystem could not hold it");
            }
        }
    };
}
