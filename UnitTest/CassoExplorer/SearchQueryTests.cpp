#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/SearchQuery.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (SearchQueryTests)
{
public:

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
};