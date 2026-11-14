#include "Pch.h"

#include "EhmTestHelper.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/CachingBlockStore.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCountTests
//
//  The blocks a large volume reads and writes on a storage card (SC-067).
//  The card is a memory store with a count of each block read and written,
//  under the CachingBlockStore a card opened in Casso Explorer reads
//  through. A volume reads through a BlockImage over the cache, and a
//  write's whole image goes back through the cache, which sends only the
//  blocks that differ, as PartitionAccess::WriteBytes does for a card.
//
//  The counts are of the blocks the volume reads from the cache, which is
//  what SC-067 measures. The card itself is read in the cache's chunks, and
//  those counts are written to the log.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockCountTests)
{
public:

    static constexpr uint32_t  k32MbBlocks   = 65535;
    static constexpr size_t    kGigabyte     = 1024ull * 1024 * 1024;
    static constexpr size_t    kFileBytes    = 10 * 1024;
    static constexpr size_t    kBlockLimit   = 64;
    static constexpr size_t    kFillFiles    = 40;
    static constexpr size_t    kFreeBlocks   = 200;
    static constexpr size_t    kSystemBlocks = 32;       // the boot blocks, the root and the bitmap, and room to spare
    static constexpr size_t    kIndexBlocks  = 8;        // a file's master and index blocks, and room to spare

    //  A memory store with the blocks read from it and written to it.
    class CountingBlockStore : public IBlockStore
    {
    public:
        explicit CountingBlockStore (std::vector<Byte> bytes) : m_store (std::move (bytes)) {}

        uint32_t  GetBlockCount() const override { return m_store.GetBlockCount(); }

        HRESULT  ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes) override
        {
            blocksRead += count;
            return m_store.ReadBlocks (first, count, outBytes);
        }

        HRESULT  WriteBlocks (uint32_t first, std::span<const Byte> bytes) override
        {
            blocksWritten += bytes.size() / kBlockBytes;
            return m_store.WriteBlocks (first, bytes);
        }

        HRESULT  Commit() override { return m_store.Commit(); }

        size_t  blocksRead    = 0;
        size_t  blocksWritten = 0;

    private:
        MemoryBlockStore  m_store;
    };

    //  A store with a count of the blocks a volume reads from the cache over
    //  the card.
    class VolumeReads : public IBlockStore
    {
    public:
        explicit VolumeReads (IBlockStore & inner) : m_inner (inner) {}

        uint32_t  GetBlockCount() const override { return m_inner.GetBlockCount(); }

        HRESULT  ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes) override
        {
            blocksRead += count;
            return m_inner.ReadBlocks (first, count, outBytes);
        }

        HRESULT  WriteBlocks (uint32_t first, std::span<const Byte> bytes) override { return m_inner.WriteBlocks (first, bytes); }

        HRESULT  Commit() override { return m_inner.Commit(); }

        size_t  blocksRead = 0;

    private:
        IBlockStore  & m_inner;
    };

    static FilePayload MakeFile (size_t bytes, Byte seed)
    {
        FilePayload  payload;



        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (bytes);

        for (size_t i = 0; i < bytes; i++)
        {
            payload.bytes[i] = (Byte) (i * 31 + seed);
        }

        return payload;
    }

    //  The root listing and a 10 KB copy on the card's volume, with the
    //  blocks each reads and writes. `open` makes the volume over an image.
    template <typename Open>
    static void CountOnCard (std::vector<Byte> image, const char * label, const char * newFile, Open open)
    {
        CountingBlockStore  card (std::move (image));
        CachingBlockStore   cache (card);
        VolumeReads         reads (cache);
        VolumeListing       listing;
        std::vector<Byte>   written;
        size_t              listed  = 0;
        size_t              cardIn  = 0;



        {
            BlockImage  blocks (reads);
            auto        volume = open (blocks);

            AssertSucceeded (volume->Enumerate (listing));

            listed = reads.blocksRead;
            cardIn = card.blocksRead;

            Assert::IsFalse (listing.entries.empty(), L"the root has files");

            AssertSucceeded (volume->Write (FilePath::Parse (newFile), MakeFile (kFileBytes, 7), written));
        }

        AssertSucceeded (cache.WriteBlocks (0, written));
        AssertSucceeded (cache.Commit());

        Logger::WriteMessage (std::format ("{}: the root listing read {} blocks from the cache and {} from the card; the copy wrote {} blocks to the card, and the whole volume was read for it\n",
                                           label, listed, cardIn, card.blocksWritten).c_str());

        Assert::IsTrue (listed < kBlockLimit,              L"listing the root reads fewer than 64 blocks");
        Assert::IsTrue (card.blocksWritten < kBlockLimit, L"copying a 10 KB file writes fewer than 64 blocks");
        Assert::IsTrue (card.blocksWritten > 0,           L"the copy is on the card");
    }


    //  A 32 MB ProDOS volume with files in its root, filled to within
    //  kFreeBlocks blocks of its end.
    TEST_METHOD (Full32MbProDos_ListsAndCopiesInFewerThan64Blocks)
    {
        std::vector<Byte>  image (k32MbBlocks * ProDosSkeleton::kBlockByteSize, 0);
        size_t             fileBytes = (k32MbBlocks - kFreeBlocks - kSystemBlocks - kFillFiles * kIndexBlocks) / kFillFiles * ProDosSkeleton::kBlockByteSize;



        AssertSucceeded (ProDosSkeleton::Write (image, "FULL"));

        for (size_t i = 0; i < kFillFiles; i++)
        {
            ProDosVolume       volume (image);
            std::vector<Byte>  next;

            AssertSucceeded (volume.Write (FilePath::Parse (std::format ("FILE{}", i)), MakeFile (fileBytes, (Byte) i), next));
            image = std::move (next);
        }

        CountOnCard (std::move (image), "32 MB ProDOS", "NEW.FILE", [] (const BlockImage & blocks) { return std::make_unique<ProDosVolume> (blocks); });
    }


    //  A 1 GB HFS volume with files in its root.
    TEST_METHOD (OneGigabyteHfs_ListsAndCopiesInFewerThan64Blocks)
    {
        std::vector<Byte>  image;



        AssertSucceeded (HfsWriter::Format (image, kGigabyte / ProDosSkeleton::kBlockByteSize, "Big"));

        for (size_t i = 0; i < 8; i++)
        {
            HfsVolume          volume (image);
            std::vector<Byte>  next;

            AssertSucceeded (volume.Write (FilePath::Parse (std::format ("File {}", i)), MakeFile (kFileBytes * (i + 1), (Byte) i), next));
            image = std::move (next);
        }

        CountOnCard (std::move (image), "1 GB HFS", "New File", [] (const BlockImage & blocks) { return std::make_unique<HfsVolume> (blocks); });
    }
};
