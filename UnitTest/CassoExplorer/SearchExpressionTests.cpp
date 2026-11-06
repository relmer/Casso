#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/ISearchSources.h"
#include "CassoExplorer/Model/SearchExpression.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (SearchExpressionTests)
{
public:

    //  A file's contents and properties, with a count of each contents read.
    struct Sources : ISearchSources
    {
        std::wstring                         text;
        std::map<std::wstring, std::wstring> properties;
        std::vector<std::wstring>            kinds;
        std::wstring                         typeText;
        int                                  reads = 0;

        bool  ContainsText (const std::wstring & lowerText) override
        {
            reads++;
            return text.find (lowerText) != std::wstring::npos;
        }

        Lookup  GetProperty (const std::wstring & name, std::wstring & outLowerValue) override
        {
            auto  found = properties.find (name);

            if (found == properties.end())
            {
                return (name == L"author") ? Lookup::NoValue : Lookup::UnknownProperty;
            }

            outLowerValue = found->second;
            return Lookup::Value;
        }

        std::vector<std::wstring>  GetKinds() override    { return kinds; }
        std::wstring               GetTypeText() override { return typeText; }
    };

    static SearchExpression::MatchOptions  MakeOptions (bool contents)
    {
        SearchExpression::MatchOptions  options;

        options.contents             = contents;
        options.today.year           = 2026;
        options.today.month          = 11;
        options.today.day            = 5;
        options.today.firstDayOfWeek = 0;

        return options;
    }

    static SearchExpression::FileFacts  MakeFile (const std::wstring & name, uint64_t sizeBytes = 100)
    {
        SearchExpression::FileFacts  facts;

        facts.name           = name;
        facts.path           = L"C:\\Disks\\Games\\" + name;
        facts.sizeBytes      = sizeBytes;
        facts.modified       = { true, 2026, 11, 5 };
        facts.created        = { true, 2025, 3, 14 };
        facts.accessed       = { true, 2026, 11, 4 };
        facts.hasAttributes  = true;
        facts.attributes     = FILE_ATTRIBUTE_ARCHIVE;

        return facts;
    }

    static bool  IsMatch (const std::wstring & query, const SearchExpression::FileFacts & facts, bool contents = false)
    {
        Sources  sources;

        return SearchExpression::Parse (query, MakeOptions (contents)).Matches (facts, sources);
    }

    TEST_METHOD (PlainWords_EachAppearInTheName_InAnyCase)
    {
        Assert::IsTrue  (IsMatch (L"karateka DSK", MakeFile (L"Karateka.dsk")));
        Assert::IsFalse (IsMatch (L"karateka woz", MakeFile (L"Karateka.dsk")));
    }

    TEST_METHOD (APlainWord_BeginsAWordOfTheName_AsFileExplorerMatchesIt)
    {
        Assert::IsTrue  (IsMatch (L"setup", MakeFile (L"setup.exe")));
        Assert::IsTrue  (IsMatch (L"setup", MakeFile (L"oem_setup.inf")));
        Assert::IsTrue  (IsMatch (L"setup", MakeFile (L"Microsoft-Windows-Setup.mum")));
        Assert::IsTrue  (IsMatch (L"setup", MakeFile (L"SetupApi.dll")));
        Assert::IsFalse (IsMatch (L"setup", MakeFile (L"winsetup.dll")));
        Assert::IsFalse (IsMatch (L"setup", MakeFile (L"WinSetup.dll")), L"a capital inside a word doesn't begin one");
        Assert::IsFalse (IsMatch (L"name:setup", MakeFile (L"winsetup.dll")));
        Assert::IsTrue  (IsMatch (L"*setup*", MakeFile (L"winsetup.dll")));
        Assert::IsTrue  (IsMatch (L"name:~=setup", MakeFile (L"winsetup.dll")));
        Assert::IsTrue  (IsMatch (L"/setup/", MakeFile (L"winsetup.dll")));
        Assert::IsTrue  (IsMatch (L".dsk", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"setup", MakeFile (L"winsetup-setup.log")), L"a later occurrence can begin a word");
    }

    TEST_METHOD (ContentsOn_AWordMatchesTheNameOrTheContents_WordByWord)
    {
        SearchExpression  expression = SearchExpression::Parse (L"karateka princess", MakeOptions (true));
        Sources           sources;

        sources.text = L"rescue the princess";

        Assert::IsTrue (expression.Matches (MakeFile (L"Karateka.txt"), sources));
        Assert::AreEqual (1, sources.reads, L"only the word the name lacks is read");

        sources.text = L"nothing here";
        Assert::IsFalse (expression.Matches (MakeFile (L"Karateka.txt"), sources));
    }

    TEST_METHOD (Contents_AreNotRead_WhenCheaperTermsDecide)
    {
        SearchExpression             expression = SearchExpression::Parse (L"princess size:large", MakeOptions (true));
        Sources                      sources;
        SearchExpression::FileFacts  folder     = MakeFile (L"Games", 0);

        sources.text    = L"princess";
        folder.isFolder = true;

        Assert::IsFalse  (expression.Matches (MakeFile (L"Karateka.txt", 100), sources));
        Assert::AreEqual (0, sources.reads, L"the size fails first");

        Assert::IsFalse  (SearchExpression::Parse (L"content:princess", MakeOptions (true)).Matches (folder, sources), L"a folder has no contents");
        Assert::AreEqual (0, sources.reads);
    }

    TEST_METHOD (ContentsOff_ABareWordTestsTheNameAlone_AndContentStillReads)
    {
        Sources  sources;

        sources.text = L"princess";

        Assert::IsFalse (SearchExpression::Parse (L"princess", MakeOptions (false)).Matches (MakeFile (L"Karateka.txt"), sources));
        Assert::IsTrue  (SearchExpression::Parse (L"content:princess", MakeOptions (false)).Matches (MakeFile (L"Karateka.txt"), sources));
        Assert::IsFalse (SearchExpression::Parse (L"name:princess", MakeOptions (true)).Matches (MakeFile (L"Karateka.txt"), sources));
    }

    TEST_METHOD (OrNotMinusAndParentheses_Combine)
    {
        Assert::IsTrue  (IsMatch (L"(karateka OR lode) -woz", MakeFile (L"Lode Runner.dsk")));
        Assert::IsFalse (IsMatch (L"(karateka OR lode) -woz", MakeFile (L"Lode Runner.woz")));
        Assert::IsTrue  (IsMatch (L"karateka AND dsk", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"NOT woz dsk", MakeFile (L"Karateka.dsk")), L"NOT binds closer than AND");
        Assert::IsTrue  (IsMatch (L"woz OR karateka dsk", MakeFile (L"Karateka.dsk")), L"AND binds closer than OR");
        Assert::IsFalse (IsMatch (L"woz OR karateka po", MakeFile (L"Karateka.dsk")));
    }

    TEST_METHOD (APhrase_MatchesAsWritten)
    {
        Assert::IsTrue  (IsMatch (L"\"lode runner\"", MakeFile (L"Lode Runner.dsk")));
        Assert::IsFalse (IsMatch (L"\"runner lode\"", MakeFile (L"Lode Runner.dsk")));
    }

    TEST_METHOD (Wildcards_MatchTheWholeName)
    {
        SearchExpression  expression = SearchExpression::Parse (L"*.dsk", MakeOptions (false));
        Sources           sources;

        Assert::IsTrue  (expression.HasWildcards());
        Assert::IsTrue  (expression.Matches (MakeFile (L"Karateka.dsk"), sources));
        Assert::IsFalse (expression.Matches (MakeFile (L"Karateka.dsk.bak"), sources));
        Assert::IsTrue  (IsMatch (L"k?rateka.*", MakeFile (L"Karateka.dsk")));
    }

    TEST_METHOD (ARegularExpression_MatchesAnywhere_IgnoringCase)
    {
        SearchExpression  expression = SearchExpression::Parse (L"/^kb\\d+/", MakeOptions (true));
        Sources           sources;

        Assert::IsTrue   (expression.HasRegex());
        Assert::IsTrue   (expression.GetError().empty());
        Assert::IsTrue   (expression.Matches (MakeFile (L"KB5031.log"), sources));
        Assert::IsFalse  (expression.Matches (MakeFile (L"xKB5031.log"), sources));
        Assert::AreEqual (0, sources.reads, L"a regular expression doesn't read contents");

        Assert::IsTrue   (IsMatch (L"path:/disks\\\\games/", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue   (IsMatch (L"name:/te.a/", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue   (IsMatch (L"/(lode|choplifter) r.nner/ dsk", MakeFile (L"Lode Runner.dsk")), L"a space inside the slashes stays in the term");
        Assert::IsFalse  (IsMatch (L"/(lode|choplifter) r.nner/ woz", MakeFile (L"Lode Runner.dsk")));
    }

    TEST_METHOD (AnInvalidRegularExpression_IsReported_AndMatchesNothing)
    {
        SearchExpression  expression = SearchExpression::Parse (L"/kb(/", MakeOptions (true));
        Sources           sources;

        Assert::IsFalse (expression.GetError().empty());
        Assert::IsTrue  (expression.HasRegex());
        Assert::IsFalse (expression.Matches (MakeFile (L"kb(.txt"), sources));
    }

    TEST_METHOD (NamePathFolderAndExtension_AreTestedByTheirProperty)
    {
        Assert::IsTrue  (IsMatch (L"name:karateka", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"filename:~<kara", MakeFile (L"Karateka.dsk")));
        Assert::IsFalse (IsMatch (L"filename:~<rate", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"name:=karateka.dsk", MakeFile (L"Karateka.dsk")));
        Assert::IsFalse (IsMatch (L"name:=karateka", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"name:~!woz", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"name:<>x", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"ext:dsk", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"fileext:.DSK", MakeFile (L"Karateka.dsk")));
        Assert::IsFalse (IsMatch (L"ext:ds", MakeFile (L"Karateka.dsk")), L"an extension is compared whole");
        Assert::IsTrue  (IsMatch (L"folder:games", MakeFile (L"Karateka.dsk")));
        Assert::IsFalse (IsMatch (L"folder:karateka", MakeFile (L"Karateka.dsk")), L"the folder is the parent's path");
        Assert::IsTrue  (IsMatch (L"path:\"c:\\disks\"", MakeFile (L"Karateka.dsk")));
        Assert::IsTrue  (IsMatch (L"name:(karateka OR lode)", MakeFile (L"Lode Runner.dsk")));
        Assert::IsFalse (IsMatch (L"name:(karateka OR choplifter)", MakeFile (L"Lode Runner.dsk")));
    }

    TEST_METHOD (Size_TakesKeywordsNumbersComparisonsAndRanges)
    {
        constexpr uint64_t  kMiB = 1024 * 1024;

        Assert::IsTrue  (IsMatch (L"size:empty", MakeFile (L"a.txt", 0)));
        Assert::IsFalse (IsMatch (L"size:tiny", MakeFile (L"a.txt", 0)));
        Assert::IsTrue  (IsMatch (L"size:tiny", MakeFile (L"a.txt", 16 * 1024)), L"a keyword's upper bound is in it");
        Assert::IsTrue  (IsMatch (L"size:small", MakeFile (L"a.txt", 16 * 1024 + 1)));
        Assert::IsTrue  (IsMatch (L"size:large", MakeFile (L"a.txt", 200 * kMiB)));
        Assert::IsTrue  (IsMatch (L"size:>1mb", MakeFile (L"a.txt", 3 * kMiB)));
        Assert::IsFalse (IsMatch (L"size:>1mb", MakeFile (L"a.txt", kMiB + 5)), L"greater is past the unit's span");
        Assert::IsTrue  (IsMatch (L"size:>=1mb", MakeFile (L"a.txt", kMiB)));
        Assert::IsTrue  (IsMatch (L"size:<10kb", MakeFile (L"a.txt", 9 * 1024)));
        Assert::IsTrue  (IsMatch (L"size:1mb", MakeFile (L"a.txt", kMiB + kMiB / 2)));
        Assert::IsTrue  (IsMatch (L"size:1mb..4mb", MakeFile (L"a.txt", 3 * kMiB)));
        Assert::IsFalse (IsMatch (L"size:1mb..4mb", MakeFile (L"a.txt", 6 * kMiB)));
        Assert::IsTrue  (IsMatch (L"size:>medium", MakeFile (L"a.txt", 200 * kMiB)));
        Assert::IsTrue  (IsMatch (L"size:2.5kb", MakeFile (L"a.txt", 2560)));
    }

    TEST_METHOD (Dates_TakeKeywordsIsoAndTheUsersFormat)
    {
        SearchExpression::FileFacts  file = MakeFile (L"a.txt");

        Assert::IsTrue  (IsMatch (L"datemodified:today", file));
        Assert::IsTrue  (IsMatch (L"datemodified:\"this week\"", file));
        Assert::IsTrue  (IsMatch (L"dateaccessed:yesterday", file));
        Assert::IsTrue  (IsMatch (L"datemodified:\"this month\"", file));
        Assert::IsTrue  (IsMatch (L"datecreated:\"last year\"", file));
        Assert::IsTrue  (IsMatch (L"datemodified:2026-11-05", file));
        Assert::IsTrue  (IsMatch (L"datemodified:2026-11-05T09:30", file));
        Assert::IsTrue  (IsMatch (L"datemodified:2026-11", file));
        Assert::IsTrue  (IsMatch (L"datemodified:2026", file));
        Assert::IsTrue  (IsMatch (L"datemodified:11/5/2026", file));
        Assert::IsTrue  (IsMatch (L"datemodified:11/5/26", file));
        Assert::IsTrue  (IsMatch (L"datemodified:>2026-11-04", file));
        Assert::IsFalse (IsMatch (L"datemodified:>2026-11-05", file));
        Assert::IsTrue  (IsMatch (L"datemodified:<=2026-11-05", file));
        Assert::IsTrue  (IsMatch (L"datecreated:2025-01-01..2025-06-30", file));
        Assert::IsFalse (IsMatch (L"datecreated:2025-04-01..2025-06-30", file));
        Assert::IsTrue  (IsMatch (L"datecreated:<2026", file));
    }

    TEST_METHOD (Dates_FollowTheUsersDayMonthOrder)
    {
        SearchExpression::MatchOptions  options = MakeOptions (false);
        Sources                         sources;

        options.shortDate = L"dd.MM.yyyy";

        Assert::IsTrue  (SearchExpression::Parse (L"datemodified:5.11.2026", options).Matches (MakeFile (L"a.txt"), sources));
        Assert::IsFalse (SearchExpression::Parse (L"datemodified:11.5.2026", options).Matches (MakeFile (L"a.txt"), sources));
    }

    TEST_METHOD (Attributes_KindAndType_AreTested)
    {
        SearchExpression::FileFacts  file = MakeFile (L"Readme.txt");
        Sources                      sources;

        sources.kinds    = { L"document" };
        sources.typeText = L"text document";

        Assert::IsTrue  (IsMatch (L"attributes:archive", file));
        Assert::IsFalse (IsMatch (L"attributes:hidden", file));
        Assert::IsTrue  (IsMatch (L"attributes:<>hidden", file));
        Assert::IsTrue  (SearchExpression::Parse (L"kind:document", MakeOptions (false)).Matches (file, sources));
        Assert::IsFalse (SearchExpression::Parse (L"kind:picture", MakeOptions (false)).Matches (file, sources));
        Assert::IsTrue  (SearchExpression::Parse (L"type:txt", MakeOptions (false)).Matches (file, sources));
        Assert::IsTrue  (SearchExpression::Parse (L"type:\"text document\"", MakeOptions (false)).Matches (file, sources));

        file.isFolder = true;
        Assert::IsTrue  (SearchExpression::Parse (L"kind:folder", MakeOptions (false)).Matches (file, sources));
    }

    TEST_METHOD (OtherProperties_AreLookedUp_OrMatchedAsText)
    {
        SearchExpression::FileFacts  file = MakeFile (L"Notes.docx");
        Sources                      sources;

        sources.properties[L"rating"] = L"75";
        sources.text                  = L"see foo:bar inside";

        Assert::IsTrue  (SearchExpression::Parse (L"rating:>50", MakeOptions (false)).Matches (file, sources));
        Assert::IsFalse (SearchExpression::Parse (L"rating:<50", MakeOptions (false)).Matches (file, sources));
        Assert::IsFalse (SearchExpression::Parse (L"author:bob", MakeOptions (false)).Matches (file, sources), L"a property the file doesn't have");
        Assert::IsTrue  (SearchExpression::Parse (L"foo:bar", MakeOptions (true)).Matches (file, sources), L"an unknown property is text");
    }

    TEST_METHOD (AQueryThatDoesNotParse_IsSearchedAsPlainWords)
    {
        SearchExpression  expression = SearchExpression::Parse (L"lode (runner", MakeOptions (false));
        Sources           sources;

        Assert::IsTrue  (expression.IsPlainWords());
        Assert::IsTrue  (expression.Matches (MakeFile (L"Lode (Runner.dsk"), sources));
        Assert::IsTrue  (SearchExpression::Parse (L"lode OR", MakeOptions (false)).IsPlainWords());
        Assert::IsTrue  (SearchExpression::Parse (L"name:", MakeOptions (false)).IsPlainWords());
        Assert::IsFalse (SearchExpression::Parse (L"C:\\Disks", MakeOptions (false)).IsPlainWords(), L"a drive's colon is a path");
    }

    TEST_METHOD (Highlights_AreTheTermsANameMustHave)
    {
        std::vector<std::wstring>  off = SearchExpression::Parse (L"lode name:runner -woz (a OR b) *.dsk", MakeOptions (false)).GetNameHighlights();
        std::vector<std::wstring>  on  = SearchExpression::Parse (L"lode name:runner", MakeOptions (true)).GetNameHighlights();

        Assert::AreEqual (size_t (2), off.size());
        Assert::AreEqual (std::wstring (L"lode"), off[0]);
        Assert::AreEqual (std::wstring (L"runner"), off[1]);

        Assert::AreEqual (size_t (1), on.size(), L"with contents on, a bare word can match the text instead");
        Assert::AreEqual (std::wstring (L"runner"), on[0]);
    }
};
