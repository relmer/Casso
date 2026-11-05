#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/RowGrouping.h"

class ISearchSources;





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression
//
//  A query typed into the search box, parsed into a tree of terms joined by
//  AND, OR and NOT, for a search that walks a folder and tests each file
//  against it. A query that doesn't parse is searched as plain words.
//
////////////////////////////////////////////////////////////////////////////////

class SearchExpression
{
public:
    enum class Field
    {
        Text,
        Name,
        Extension,
        Folder,
        Path,
        Size,
        DateModified,
        DateCreated,
        DateAccessed,
        Kind,
        Type,
        Attributes,
        Content,
        Other,
    };

    enum class Operator
    {
        Default,
        Equal,
        NotEqual,
        Less,
        Greater,
        LessOrEqual,
        GreaterOrEqual,
        StartsWith,
        Contains,
        DoesNotContain,
        Range,
    };

    //  A local date; `has` false for an item without one.
    struct Date
    {
        bool  has   = false;
        int   year  = 0;
        int   month = 0;
        int   day   = 0;
    };

    //  What a folder's listing has about a file, tested before anything else.
    struct FileFacts
    {
        std::wstring  name;
        std::wstring  path;
        bool          isFolder      = false;
        bool          hasSize       = true;
        uint64_t      sizeBytes     = 0;
        Date          modified;
        Date          created;
        Date          accessed;
        bool          hasAttributes = false;
        DWORD         attributes    = 0;
    };

    struct MatchOptions
    {
        bool                contents  = true;
        RowGrouping::Today  today;

        //  The user's short date, as the locale writes it: d, M and y runs.
        std::wstring        shortDate = L"M/d/yyyy";
    };

    SearchExpression();
    ~SearchExpression();

    static SearchExpression  Parse (const std::wstring & query, const MatchOptions & options);

    bool  Matches (const FileFacts & facts, ISearchSources & sources) const;

    bool                       IsEmpty           () const;
    bool                       HasRegex          () const;
    bool                       HasWildcards      () const;
    bool                       HasField          (Field field) const;
    bool                       HasNameTerms      () const;
    bool                       IsPlainWords      () const { return m_plainWords; }
    const std::wstring       & GetError          () const { return m_error; }
    std::vector<std::wstring>  GetNameHighlights () const;

private:
    enum class TokenKind
    {
        Word,
        Open,
        Close,
        Or,
        And,
        Not,
        Minus,
    };

    struct Node;
    struct Tree;
    struct Token;
    struct Cursor;
    struct Prepared;

    static std::vector<Token>  Tokenize          (const std::wstring & query);
    static bool                IsAt              (const Cursor & cursor, TokenKind kind);
    static bool                IsDone            (const Cursor & cursor);
    static bool                IsValueStart      (const std::wstring & text);
    static bool                TryParseOr        (Cursor & cursor, const std::wstring & context, Node & outNode);
    static bool                TryParseAnd       (Cursor & cursor, const std::wstring & context, Node & outNode);
    static bool                TryParseUnary     (Cursor & cursor, const std::wstring & context, Node & outNode);
    static bool                TryParsePrimary   (Cursor & cursor, const std::wstring & context, Node & outNode);
    static bool                TryParseTerm      (const std::wstring & text, Tree & tree, Node & outNode);
    static bool                TrySplitProperty  (const std::wstring & text, std::wstring & outProperty, std::wstring & outRest);
    static Field               GetField          (const std::wstring & property);
    static Operator            TakeOperator      (std::wstring & ioValue);
    static void                ParseValue        (const std::wstring & raw, bool allowRegex, Tree & tree, Node & ioNode);
    static Node                MakePlainWords    (const std::wstring & query, Tree & tree);
    static int                 GetCost           (const Node & node, bool contents);
    static void                SortByCost        (Node & ioNode, bool contents);
    static void                CollectHighlights (const Node & node, bool contents, std::vector<std::wstring> & ioWords);
    static bool                HasTermWhere      (const Node & node, const std::function<bool (const Node &)> & test);
    static std::wstring        ToLower           (const std::wstring & text);
    static std::wstring        ToLowerUnquoted   (const std::wstring & text);

    bool  Evaluate          (const Node & node, const Prepared & file, ISearchSources & sources) const;
    bool  EvaluateTerm      (const Node & node, const Prepared & file, ISearchSources & sources) const;
    bool  EvaluateText      (const Node & node, const Prepared & file, ISearchSources & sources) const;
    bool  EvaluateOther     (const Node & node, const Prepared & file, ISearchSources & sources) const;
    bool  MatchesString     (const Node & node, const std::wstring & lowerValue) const;
    bool  MatchesSize       (const Node & node, const Prepared & file) const;
    bool  MatchesDate       (const Node & node, const Date & date) const;
    bool  MatchesAttributes (const Node & node, const Prepared & file) const;
    bool  MatchesKind       (const Node & node, const Prepared & file, ISearchSources & sources) const;
    bool  MatchesType       (const Node & node, const Prepared & file, ISearchSources & sources) const;

    static bool     TryGetSizeRange   (const std::wstring & value, uint64_t & outLow, uint64_t & outHigh);
    static bool     TryGetDayRange    (const std::wstring & value, const MatchOptions & options, int64_t & outStart, int64_t & outEnd);
    static bool     TryGetKeywordDays (const std::wstring & value, const RowGrouping::Today & today, int64_t & outStart, int64_t & outEnd);
    static bool     TryGetNumericDays (const std::wstring & value, const std::wstring & shortDate, int64_t & outStart, int64_t & outEnd);
    static bool     TrySplitNumbers   (const std::wstring & date, std::vector<int> & outNumbers, size_t & outFirstDigits);
    static int64_t  GetMonthStart     (int year, int month);
    static bool     IsInRange         (Operator op, int64_t value, int64_t low, int64_t high);
    static bool     TryGetNumber      (const std::wstring & text, double & outNumber);

    std::shared_ptr<const Tree>  m_tree;
    MatchOptions                 m_options;
    std::wstring                 m_error;
    bool                         m_plainWords = false;
};
