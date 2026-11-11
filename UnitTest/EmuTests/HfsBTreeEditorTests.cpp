#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/HfsBTreeEditor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditorTests
//
//  Random inserts, removes and replaces on catalog and extents trees, with
//  HfsBTreeEditor::Check run on the tree as they go, and the records
//  compared with a sorted list of the same changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HfsBTreeEditorTests)
{
public:

    using Kind   = HfsBTree::Kind;
    using Record = HfsBTree::Record;

    //  A tree of `kind` with `nodes` nodes and no records, as a new volume
    //  has.
    static std::vector<Byte> MakeEmptyTree (Kind kind, uint32_t nodes)
    {
        std::vector<Byte>  bytes;



        AssertSucceeded (HfsBTree::Build (kind, {}, nodes, bytes));

        return bytes;
    }


    //  A catalog key for parent `parent` and a name of `length` characters,
    //  mixed case, from `number`.
    static std::vector<Byte> MakeCatalogKey (uint32_t parent, uint32_t number, size_t length)
    {
        std::string        name = std::format ("{}{:08}", (number % 2) ? "File " : "file ", number);
        std::vector<Byte>  key;



        name.resize (std::max<size_t> (length, 13), 'x');
        key.assign (7, 0);
        key[0] = (Byte) (6 + name.size());
        HfsBTree::PutLong (key, 2, parent);
        key[6] = (Byte) name.size();
        key.insert (key.end(), name.begin(), name.end());

        return key;
    }


    static std::vector<Byte> MakeExtentsKey (uint32_t file, Byte fork, uint32_t start)
    {
        std::vector<Byte>  key = { 7, fork, 0, 0, 0, 0, 0, 0 };



        HfsBTree::PutLong (key, 2, file);
        HfsBTree::PutWord (key, 6, start);

        return key;
    }


    //  Keys in the tree's order.
    struct KeyOrder
    {
        Kind  kind = Kind::Catalog;

        bool operator() (const std::vector<Byte> & a, const std::vector<Byte> & b) const
        {
            return HfsBTree::CompareKeys (kind, a.data(), a.size(), b.data(), b.size()) < 0;
        }
    };

    using Reference = std::map<std::vector<Byte>, std::vector<Byte>, KeyOrder>;


    static void AssertClean (const HfsBTreeEditor & editor, const Reference & reference, const std::string & step)
    {
        std::string          problems;
        std::vector<Record>  records;
        size_t               at = 0;



        editor.Check (problems);
        Assert::IsTrue (problems.empty(), TextEncoding::Utf8ToWide (step + ": " + problems).c_str());

        AssertSucceeded (editor.ReadAll (records));
        Assert::AreEqual (reference.size(), records.size(), TextEncoding::Utf8ToWide (step).c_str());

        for (const auto & [key, data] : reference)
        {
            Assert::IsTrue (records[at].key == key && records[at].data == data, TextEncoding::Utf8ToWide (std::format ("{}: record {} differs", step, at)).c_str());
            at++;
        }
    }


    //  Inserts only until `fillNodes` nodes are in use, then `operations`
    //  random changes: mostly inserts in the first half, mostly removes in
    //  the second, and replaces throughout. The tree is emptied at the end.
    static void RunRandom (Kind kind, uint32_t seed, size_t operations, size_t checkEvery, uint32_t & outDeepest, uint32_t & outMostUsed, uint32_t fillNodes = 0)
    {
        std::mt19937         random (seed);
        std::vector<Byte>    nodes  = MakeEmptyTree (kind, 16);
        HfsBTreeEditor       editor (kind, nodes);
        Reference                       reference (KeyOrder { kind });
        std::vector<std::vector<Byte>>  keys;
        uint32_t                        number = 0;



        outDeepest  = 0;
        outMostUsed = 0;
        AssertSucceeded (editor.Open());

        for (size_t step = 0, counted = 0; counted < operations; step++)
        {
            bool    filling = editor.GetTotalNodes() - editor.GetFreeNodes() < fillNodes;
            bool    growing = counted < operations / 2;
            int     choice  = filling ? 0 : (int) (random() % 10);
            Record  record;

            counted += filling ? 0 : 1;

            if (editor.GetFreeNodes() < editor.GetNodesForInsert())
            {
                AssertSucceeded (editor.Extend (editor.GetTotalNodes() * 2));
            }

            if (reference.empty() || (growing ? choice < 7 : choice < 3))
            {
                if (kind == Kind::Catalog)
                {
                    static constexpr size_t  s_kSizes[] = { 46, 70, 102 };

                    record.key = MakeCatalogKey (2 + random() % 20, number++, 13 + random() % 19);
                    record.data.assign (s_kSizes[random() % 3], (Byte) step);
                }
                else
                {
                    record.key = MakeExtentsKey (16 + random() % 50, (random() % 2) ? 0xFF : 0x00, number++);
                    record.data.assign (12, (Byte) step);
                }

                AssertSucceeded (editor.Insert (record));
                reference.emplace (record.key, record.data);
                keys.push_back (record.key);
            }
            else if (choice < 9)
            {
                size_t  at = random() % keys.size();

                AssertSucceeded (editor.Remove (keys[at]));
                reference.erase (keys[at]);
                keys[at] = keys.back();
                keys.pop_back();
            }
            else
            {
                size_t               at   = random() % keys.size();
                std::vector<Byte> &  data = reference[keys[at]];

                data.assign (data.size(), (Byte) (step + 1));
                AssertSucceeded (editor.Replace (Record { keys[at], data }));
            }

            outDeepest  = std::max (outDeepest, editor.GetDepth());
            outMostUsed = std::max (outMostUsed, editor.GetTotalNodes() - editor.GetFreeNodes());

            if (step % checkEvery == 0 || counted == operations)
            {
                AssertClean (editor, reference, std::format ("seed {} step {}", seed, step));
            }
        }

        for (const std::vector<Byte> & key : keys)
        {
            AssertSucceeded (editor.Remove (key));
        }

        reference.clear();

        AssertClean (editor, reference, std::format ("seed {} emptied", seed));
        Assert::AreEqual ((uint32_t) 0, editor.GetDepth());
    }


    TEST_METHOD (Catalog_RandomChangesKeepEveryInvariant)
    {
        uint32_t  deepest = 0;
        uint32_t  used    = 0;



        for (uint32_t seed = 1; seed <= 3; seed++)
        {
            RunRandom (Kind::Catalog, seed, 3000, 250, deepest, used);
            Assert::IsTrue (deepest >= 3, L"the tree never reached three levels");
        }
    }


    TEST_METHOD (Extents_RandomChangesKeepEveryInvariant)
    {
        uint32_t  deepest = 0;
        uint32_t  used    = 0;



        for (uint32_t seed = 11; seed <= 13; seed++)
        {
            RunRandom (Kind::Extents, seed, 4000, 250, deepest, used);
            Assert::IsTrue (deepest >= 3, L"the tree never reached three levels");
        }
    }


    //  Enough records for more nodes than the header's map covers, so map
    //  nodes are added and nodes past them used.
    TEST_METHOD (Catalog_GrowsPastTheHeaderMap)
    {
        uint32_t  deepest = 0;
        uint32_t  used    = 0;



        RunRandom (Kind::Catalog, 99, 3000, 2000, deepest, used, 2100);
        Assert::IsTrue (used > 2048, std::format (L"only {} nodes were in use", used).c_str());
    }


    TEST_METHOD (Insert_RefusesAKeyTheTreeHas)
    {
        std::vector<Byte>  nodes  = MakeEmptyTree (Kind::Catalog, 8);
        HfsBTreeEditor     editor (Kind::Catalog, nodes);
        Record             record { MakeCatalogKey (2, 1, 13), std::vector<Byte> (102, 1) };
        Record             other  = record;
        Record             found;



        AssertSucceeded (editor.Open());
        AssertSucceeded (editor.Insert (record));

        //  The same name in another case is the same key.
        other.key[7] = (Byte) (other.key[7] ^ 0x20);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS), editor.Insert (other));
        AssertSucceeded (editor.Find (other.key, found));
        Assert::IsTrue (found.key == record.key);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), editor.Remove (MakeCatalogKey (2, 2, 13)));
    }


    TEST_METHOD (Insert_FailsWithDiskFullWhenNoNodeIsFree)
    {
        std::vector<Byte>  nodes  = MakeEmptyTree (Kind::Catalog, 2);
        HfsBTreeEditor     editor (Kind::Catalog, nodes);
        HRESULT            hr     = S_OK;
        uint32_t           number = 0;



        AssertSucceeded (editor.Open());

        while (SUCCEEDED (hr))
        {
            hr = editor.Insert (Record { MakeCatalogKey (2, number++, 31), std::vector<Byte> (102, 0) });
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
    }


    TEST_METHOD (Scan_VisitsRecordsFromTheKeyOn)
    {
        std::vector<Byte>       nodes  = MakeEmptyTree (Kind::Extents, 64);
        HfsBTreeEditor          editor (Kind::Extents, nodes);
        std::vector<uint32_t>   starts;



        AssertSucceeded (editor.Open());

        for (uint32_t start = 0; start < 200; start++)
        {
            AssertSucceeded (editor.Insert (Record { MakeExtentsKey (start % 2 ? 20 : 21, 0, start), std::vector<Byte> (12, 0) }));
        }

        AssertSucceeded (editor.Scan (MakeExtentsKey (21, 0, 0), [&] (const Record & record)
        {
            starts.push_back (((uint32_t) record.key[6] << 8) | record.key[7]);
            return HfsBTree::GetLong (record.key, 2) == 21;
        }));

        Assert::AreEqual ((size_t) 100, starts.size());
        Assert::AreEqual ((uint32_t) 0, starts.front());
        Assert::IsTrue (std::is_sorted (starts.begin(), starts.end()));
    }
};
