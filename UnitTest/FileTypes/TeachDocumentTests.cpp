#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/IigsResources.h"
#include "FileTypes/TeachDocument.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IigsResourcesTests and TeachDocumentTests
//
//  A resource fork built with one rStyleBlock, found by its type and ID,
//  and a Teach file's text styled by it.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendLong (std::vector<Byte> & bytes, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        bytes.push_back ((Byte) (value >> shift));
    }
}



//  A fork with the map at 16, its index at 48, one entry and the end entry,
//  and the resource at 88.
static std::vector<Byte> MakeFork (Word type, std::span<const Byte> resource)
{
    std::vector<Byte>  fork;



    AppendLong (fork, 0);
    AppendLong (fork, 16);
    fork.resize (16 + 14, 0);
    fork.push_back (32);
    fork.push_back (0);
    fork.resize (48, 0);
    fork.push_back ((Byte) type);
    fork.push_back ((Byte) (type >> 8));
    AppendLong (fork, 1);
    AppendLong (fork, 88);
    fork.push_back (0);
    fork.push_back (0);
    AppendLong (fork, (uint32_t) resource.size());
    AppendLong (fork, 0);
    fork.resize (88, 0);
    fork.insert (fork.end(), resource.begin(), resource.end());

    return fork;
}



TEST_CLASS (IigsResourcesTests)
{
public:

    TEST_METHOD (Find_ReturnsTheResourceOfTheTypeAndId)
    {
        std::vector<Byte>      data = { 1, 2, 3 };
        std::vector<Byte>      fork = MakeFork (0x8012, data);
        std::span<const Byte>  found;



        Assert::IsTrue   (IigsResources::Find (fork, 0x8012, 1, found));
        Assert::AreEqual ((size_t) 3, found.size());
        Assert::AreEqual ((Byte) 3, found[2]);

        Assert::IsFalse (IigsResources::Find (fork, 0x8012, 2, found));
        Assert::IsFalse (IigsResources::Find (fork, 0x8013, 1, found));
        Assert::IsFalse (IigsResources::Find (std::span<const Byte> (fork).first (60), 0x8012, 1, found), L"A cut-off index");
    }
};



TEST_CLASS (TeachDocumentTests)
{
public:

    TEST_METHOD (Decode_StylesRunsAndAlignsFromTheRuler)
    {
        std::vector<Byte>  format = { 0, 0 };
        std::vector<Byte>  text   = { 'A', 'B', 0x0D, 'C', 'D', 'E' };
        std::vector<Byte>  fork;
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        AppendLong (format, 8);
        format.insert (format.end(), { 0, 0, 0, 0, 0, 0, 1, 0 });
        AppendLong (format, 24);
        format.insert (format.end(), { 0xFE, 0xFF, 0x00, 12, 0, 0, 0, 0, 0, 0, 0, 0 });
        format.insert (format.end(), { 0xFE, 0xFF, 0x01, 12, 0, 0, 0, 0, 0, 0, 0, 0 });
        AppendLong (format, 2);
        AppendLong (format, 3);
        AppendLong (format, 0);
        AppendLong (format, 3);
        AppendLong (format, 12);

        fork               = MakeFork (IigsResources::kStyleBlock, format);
        facts.resourceFork = fork;

        AssertSucceeded (TeachDocument::Decode (text, facts, DecodeOptions(), file, error));

        Assert::AreEqual ((size_t) 2, file.paragraphs.size());
        Assert::IsTrue   (file.paragraphs[0].alignment == Alignment::Center);
        Assert::AreEqual (std::wstring (L"AB"), file.paragraphs[0].runs[0].text);
        Assert::IsFalse  (file.paragraphs[0].runs[0].bold);
        Assert::AreEqual (std::wstring (L"CDE"), file.paragraphs[1].runs[0].text);
        Assert::IsTrue   (file.paragraphs[1].runs[0].bold);
        Assert::IsTrue   (file.notes.empty());
    }


    TEST_METHOD (Decode_PlainWithoutTheResourceFork)
    {
        std::vector<Byte>  text = { 'H', 'i' };
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (TeachDocument::Decode (text, FileFacts(), DecodeOptions(), file, error));

        Assert::AreEqual ((size_t) 1, file.paragraphs.size());
        Assert::AreEqual (std::wstring (L"Hi"), file.paragraphs[0].runs[0].text);
        Assert::AreEqual ((size_t) 1, file.notes.size());
    }
};
