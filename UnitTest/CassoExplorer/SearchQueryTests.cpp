#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/SearchQuery.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (SearchQueryTests)
{
public:

    TEST_METHOD (ThisPc_HasALabel_AndItsIdRoundTrips)
    {
        std::wstring  scope;
        std::wstring  query;

        Assert::AreEqual (std::wstring (L"Search results in This PC"), SearchQuery::GetLabel (SearchQuery::kThisPc));
        Assert::IsTrue   (SearchQuery::TryParseId (SearchQuery::MakeId (SearchQuery::kThisPc, L"Sixies"), scope, query));
        Assert::AreEqual (std::wstring (SearchQuery::kThisPc), scope);
        Assert::AreEqual (std::wstring (L"Sixies"), query);
    }

    TEST_METHOD (Options_FollowTheQueryInTheId)
    {
        SearchQuery::Options  options;
        std::wstring          scope;
        std::wstring          query;

        Assert::AreEqual (SearchQuery::MakeId (L"C:\\Disks", L"x"), SearchQuery::MakeId (L"C:\\Disks", L"x", options), L"The defaults add nothing");

        options.currentFolder = true;
        options.systemFiles   = false;
        options.contents      = true;

        std::wstring  id = SearchQuery::MakeId (L"C:\\Disks", L"x", options);

        Assert::IsTrue   (SearchQuery::TryParseId (id, scope, query));
        Assert::AreEqual (std::wstring (L"x"), query);
        Assert::IsTrue   (SearchQuery::GetOptions (id).currentFolder);
        Assert::IsFalse  (SearchQuery::GetOptions (id).systemFiles);
        Assert::IsFalse  (SearchQuery::GetOptions (id).zipped);
        Assert::IsTrue   (SearchQuery::GetOptions (id).contents);
    }

    TEST_METHOD (Tokens_ReplaceTheirPropertyAndToggleOff)
    {
        std::wstring  query = SearchQuery::ToggleToken (L"sixies", SearchQuery::kSizeToken, L"tiny");

        Assert::AreEqual (std::wstring (L"sixies size:tiny"), query);
        Assert::IsTrue   (SearchQuery::HasToken (query, SearchQuery::kSizeToken, L"tiny"));

        query = SearchQuery::ToggleToken (query, SearchQuery::kSizeToken, L"huge");
        Assert::AreEqual (std::wstring (L"sixies size:huge"), query);

        query = SearchQuery::ToggleToken (query, SearchQuery::kDateToken, L"this week");
        Assert::AreEqual (std::wstring (L"sixies size:huge datemodified:\"this week\""), query);
        Assert::IsTrue   (SearchQuery::HasToken (query, SearchQuery::kDateToken, L"this week"));
        Assert::AreEqual ((size_t) 1, SearchQuery::GetNameWords (query).size(), L"Only the plain word matches names");

        query = SearchQuery::ToggleToken (query, SearchQuery::kSizeToken, L"huge");
        Assert::AreEqual (std::wstring (L"sixies datemodified:\"this week\""), query);
    }

    TEST_METHOD (Filters_TestDateSizeAndKind)
    {
        RowGrouping::Today      today;
        SearchQuery::ItemFacts  facts;

        today.year = 2026; today.month = 10; today.day = 31;    // a Saturday

        facts.hasDate   = true;
        facts.year      = 2026;
        facts.month     = 10;
        facts.day       = 26;
        facts.sizeBytes = 2048;
        facts.kinds     = { L"document" };

        Assert::IsTrue  (SearchQuery::MatchesFilters (L"datemodified:\"this week\" size:tiny kind:document", facts, today));
        Assert::IsFalse (SearchQuery::MatchesFilters (L"datemodified:today",      facts, today));
        Assert::IsFalse (SearchQuery::MatchesFilters (L"datemodified:\"last week\"", facts, today));
        Assert::IsTrue  (SearchQuery::MatchesFilters (L"datemodified:\"this month\"", facts, today));
        Assert::IsFalse (SearchQuery::MatchesFilters (L"size:huge",               facts, today));
        Assert::IsFalse (SearchQuery::MatchesFilters (L"kind:picture",            facts, today));
        Assert::IsFalse (SearchQuery::MatchesFilters (L"kind:unknown",            facts, today));
        Assert::IsTrue  (SearchQuery::MatchesFilters (L"author:someone",          facts, today), L"Another property is the index's");
    }

    TEST_METHOD (Id_CarriesTheFolderAndTheQuery)
    {
        std::wstring  scope;
        std::wstring  query;
        std::wstring  id = SearchQuery::MakeId (L"C:\\Disks", L"kind:document hello");

        Assert::IsTrue   (SearchQuery::IsId (id));
        Assert::IsTrue   (SearchQuery::TryParseId (id, scope, query));
        Assert::AreEqual (std::wstring (L"C:\\Disks"), scope);
        Assert::AreEqual (std::wstring (L"kind:document hello"), query);
        Assert::IsFalse  (SearchQuery::TryParseId (L"C:\\Disks", scope, query), L"A folder is not a search");
        Assert::AreEqual (std::wstring (L"Search results in Disks"), SearchQuery::GetLabel (L"C:\\Disks\\"));
        Assert::AreEqual (std::wstring (L"Search results in C:\\"),  SearchQuery::GetLabel (L"C:\\"));
    }


    TEST_METHOD (Names_MatchEveryPlainWordInAnyCase)
    {
        std::vector<std::wstring>  words = SearchQuery::GetNameWords (L"Hello \"dos 3.3\" kind:document");

        Assert::AreEqual ((size_t) 2, words.size(), L"A quoted phrase is one word, and a property is the index's");
        Assert::IsTrue   (SearchQuery::MatchesName (L"HELLO DOS 3.3 MASTER.dsk", words));
        Assert::IsFalse  (SearchQuery::MatchesName (L"HELLO.dsk", words), L"Every word must appear");
    }

    TEST_METHOD (Scopes_JoinAndSplitAndCasso)
    {
        std::wstring               scope   = SearchQuery::JoinScopes ({ L"C:\\Casso\\Disks", L"D:\\Apple" });
        std::vector<std::wstring>  folders = SearchQuery::SplitScopes (scope);
        std::wstring               parsed;
        std::wstring               query;

        Assert::AreEqual ((size_t) 2, folders.size());
        Assert::AreEqual (std::wstring (L"D:\\Apple"), folders[1]);
        Assert::AreEqual ((size_t) 1, SearchQuery::SplitScopes (L"C:\\Disks").size(), L"One folder is one scope");
        Assert::IsTrue   (SearchQuery::TryParseId (SearchQuery::MakeId (scope, L"hello"), parsed, query));
        Assert::AreEqual (scope, parsed, L"Several folders survive the id");
        Assert::AreEqual (std::wstring (L"Search results in Casso"), SearchQuery::GetLabel (scope));
    }

    TEST_METHOD (Wildcards_MatchTheWholeNameAndLeaveTheIndexOut)
    {
        Assert::IsTrue  (SearchQuery::HasWildcards (L"*paint*"));
        Assert::IsFalse (SearchQuery::HasWildcards (L"paint"));
        Assert::IsTrue  (SearchQuery::MatchesName (L"MousePaint.do", SearchQuery::GetNameWords (L"*paint*")), L"* stands for any run");
        Assert::IsTrue  (SearchQuery::MatchesName (L"MOUSEPAINT",    SearchQuery::GetNameWords (L"*paint")));
        Assert::IsFalse (SearchQuery::MatchesName (L"MousePaint.do", SearchQuery::GetNameWords (L"paint*")), L"and the pattern covers the whole name");
        Assert::IsTrue  (SearchQuery::MatchesName (L"disk1.dsk",     SearchQuery::GetNameWords (L"disk?.dsk")), L"? stands for one character");
        Assert::IsFalse (SearchQuery::MatchesName (L"disk10.dsk",    SearchQuery::GetNameWords (L"disk?.dsk")));
    }
};