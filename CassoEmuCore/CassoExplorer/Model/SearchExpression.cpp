#include "Pch.h"

#include "CassoExplorer/Model/SearchExpression.h"
#include "CassoExplorer/Model/ISearchSources.h"
#include "CassoExplorer/Model/SearchQuery.h"





//  The properties a walked search tests itself, by the word typed before the
//  colon; any other word is looked up in the Windows property system.
static constexpr std::pair<const wchar_t *, SearchExpression::Field>  s_kFields[] =
{
    { L"name",         SearchExpression::Field::Name         },
    { L"filename",     SearchExpression::Field::Name         },
    { L"ext",          SearchExpression::Field::Extension    },
    { L"fileext",      SearchExpression::Field::Extension    },
    { L"folder",       SearchExpression::Field::Folder       },
    { L"path",         SearchExpression::Field::Path         },
    { L"size",         SearchExpression::Field::Size         },
    { L"datemodified", SearchExpression::Field::DateModified },
    { L"datecreated",  SearchExpression::Field::DateCreated  },
    { L"dateaccessed", SearchExpression::Field::DateAccessed },
    { L"kind",         SearchExpression::Field::Kind         },
    { L"type",         SearchExpression::Field::Type         },
    { L"attributes",   SearchExpression::Field::Attributes   },
    { L"content",      SearchExpression::Field::Content      },
};

//  After a property's colon, longest first, so <= is not read as <.
static constexpr std::pair<const wchar_t *, SearchExpression::Operator>  s_kOperators[] =
{
    { L"<>", SearchExpression::Operator::NotEqual       },
    { L"<=", SearchExpression::Operator::LessOrEqual    },
    { L">=", SearchExpression::Operator::GreaterOrEqual },
    { L"~<", SearchExpression::Operator::StartsWith     },
    { L"~=", SearchExpression::Operator::Contains       },
    { L"~!", SearchExpression::Operator::DoesNotContain },
    { L"=",  SearchExpression::Operator::Equal          },
    { L"<",  SearchExpression::Operator::Less           },
    { L">",  SearchExpression::Operator::Greater        },
};

//  File Explorer's size keywords, each with the bytes it covers: more than
//  the first and at most the second. Empty is no bytes at all.
static constexpr std::tuple<const wchar_t *, uint64_t, uint64_t>  s_kSizeKeywords[] =
{
    { L"tiny",     0ULL,                  16ULL * 1024             },
    { L"small",    16ULL * 1024,          1024ULL * 1024           },
    { L"medium",   1024ULL * 1024,        128ULL * 1024 * 1024     },
    { L"large",    128ULL * 1024 * 1024,  1024ULL * 1024 * 1024    },
    { L"huge",     1024ULL * 1024 * 1024, 4096ULL * 1024 * 1024    },
    { L"gigantic", 4096ULL * 1024 * 1024, (uint64_t) INT64_MAX - 1 },
};

static constexpr std::pair<const wchar_t *, uint64_t>  s_kSizeUnits[] =
{
    { L"kb", 1024ULL               },
    { L"mb", 1024ULL * 1024        },
    { L"gb", 1024ULL * 1024 * 1024 },
    { L"b",  1ULL                  },
};

static constexpr std::pair<const wchar_t *, DWORD>  s_kAttributes[] =
{
    { L"hidden",     FILE_ATTRIBUTE_HIDDEN     },
    { L"system",     FILE_ATTRIBUTE_SYSTEM     },
    { L"readonly",   FILE_ATTRIBUTE_READONLY   },
    { L"read-only",  FILE_ATTRIBUTE_READONLY   },
    { L"compressed", FILE_ATTRIBUTE_COMPRESSED },
    { L"archive",    FILE_ATTRIBUTE_ARCHIVE    },
    { L"encrypted",  FILE_ATTRIBUTE_ENCRYPTED  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Node
//
//  A term, or AND, OR or NOT over the nodes under it. A term's value and its
//  range's upper end are lowercased; a regular expression is the tree's, by
//  its index.
//
////////////////////////////////////////////////////////////////////////////////

struct SearchExpression::Node
{
    enum class Kind
    {
        Term,
        And,
        Or,
        Not,
    };

    Kind               kind         = Kind::Term;
    std::vector<Node>  children;
    Field              field        = Field::Text;
    Operator           op           = Operator::Default;
    std::wstring       property;
    std::wstring       value;
    std::wstring       high;
    std::wstring       raw;
    bool               phrase       = false;
    bool               wildcard     = false;
    bool               invalidRegex = false;
    int                regex        = -1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Tree
//
////////////////////////////////////////////////////////////////////////////////

struct SearchExpression::Tree
{
    Node                      root;
    std::vector<std::wregex>  regexes;
    std::wstring              error;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Token
//
////////////////////////////////////////////////////////////////////////////////

struct SearchExpression::Token
{
    TokenKind     kind = TokenKind::Word;
    std::wstring  text;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Cursor
//
////////////////////////////////////////////////////////////////////////////////

struct SearchExpression::Cursor
{
    const std::vector<Token>  & tokens;
    Tree                      & tree;
    size_t                      at = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Prepared
//
//  A file's facts with its name, extension and paths lowercased once, for
//  every term that tests them.
//
////////////////////////////////////////////////////////////////////////////////

struct SearchExpression::Prepared
{
    const FileFacts  & facts;
    std::wstring       name;
    std::wstring       extension;
    std::wstring       folder;
    std::wstring       path;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::SearchExpression
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::SearchExpression() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::~SearchExpression
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::~SearchExpression() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Parse
//
//  The query's tree, or its plain words when it doesn't parse: an unmatched
//  parenthesis, an operator with nothing to apply to, or a property with no
//  value. The terms under each AND and OR are tested cheapest first, so a
//  file's contents and other properties are read only when the result
//  depends on them.
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression SearchExpression::Parse (const std::wstring & query, const MatchOptions & options)
{
    SearchExpression    expression;
    auto                tree   = std::make_shared<Tree>();
    std::vector<Token>  tokens = Tokenize (query);
    Cursor              cursor { tokens, *tree };
    bool                parsed = false;



    expression.m_options = options;

    parsed = !tokens.empty() && TryParseOr (cursor, std::wstring(), tree->root) && IsDone (cursor);

    if (!parsed)
    {
        tree->regexes.clear();
        tree->error.clear();
        tree->root              = MakePlainWords (query, *tree);
        expression.m_plainWords = true;
    }

    SortByCost (tree->root, options.contents);

    expression.m_error = tree->error;
    expression.m_tree  = std::move (tree);

    return expression;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Tokenize
//
//  Parentheses, AND, OR and NOT in capitals, a leading -, and words. A word
//  runs to a space or a parenthesis outside quotes and outside a regular
//  expression, which starts with a slash at the word's start or after a
//  property's colon and operator.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SearchExpression::Token> SearchExpression::Tokenize (const std::wstring & query)
{
    std::vector<Token>  tokens;
    size_t              at = 0;



    while (at < query.size())
    {
        wchar_t  c       = query[at];
        size_t   start   = at;
        bool     quoted  = false;
        bool     inRegex = false;

        if (iswspace (c))
        {
            at++;
            continue;
        }

        if (c == L'(' || c == L')' || (c == L'-' && at + 1 < query.size() && !iswspace (query[at + 1])))
        {
            tokens.push_back ({ (c == L'(') ? TokenKind::Open : (c == L')') ? TokenKind::Close : TokenKind::Minus, std::wstring (1, c) });
            at++;
            continue;
        }

        for (; at < query.size(); at++)
        {
            wchar_t  ch = query[at];

            if (quoted)
            {
                quoted = ch != L'"';
                continue;
            }

            if (inRegex)
            {
                at     += (ch == L'\\' && at + 1 < query.size()) ? 1 : 0;
                inRegex = ch != L'/';
                continue;
            }

            if (ch == L'"')
            {
                quoted = true;
                continue;
            }

            if (ch == L'/' && IsValueStart (query.substr (start, at - start)))
            {
                inRegex = true;
                continue;
            }

            if (iswspace (ch) || ch == L'(' || ch == L')')
            {
                break;
            }
        }

        tokens.push_back ({ TokenKind::Word, query.substr (start, at - start) });

        if      (tokens.back().text == L"OR")  { tokens.back().kind = TokenKind::Or;  }
        else if (tokens.back().text == L"AND") { tokens.back().kind = TokenKind::And; }
        else if (tokens.back().text == L"NOT") { tokens.back().kind = TokenKind::Not; }
    }

    return tokens;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::IsAt
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::IsAt (const Cursor & cursor, TokenKind kind)
{
    return cursor.at < cursor.tokens.size() && cursor.tokens[cursor.at].kind == kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::IsDone
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::IsDone (const Cursor & cursor)
{
    return cursor.at >= cursor.tokens.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::IsValueStart
//
//  Whether the text before a slash is nothing, or a property's word, its
//  colon and an operator, so the slash opens a regular expression.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::IsValueStart (const std::wstring & text)
{
    std::wstring  property;
    std::wstring  rest;



    if (text.empty())
    {
        return true;
    }

    if (!TrySplitProperty (text, property, rest))
    {
        return false;
    }

    TakeOperator (rest);

    return rest.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryParseOr
//
//  AND binds closer than OR, and NOT closer than either.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryParseOr (Cursor & cursor, const std::wstring & context, Node & outNode)
{
    Node  first;



    if (!TryParseAnd (cursor, context, first))
    {
        return false;
    }

    if (!IsAt (cursor, TokenKind::Or))
    {
        outNode = std::move (first);
        return true;
    }

    outNode      = Node();
    outNode.kind = Node::Kind::Or;
    outNode.children.push_back (std::move (first));

    while (IsAt (cursor, TokenKind::Or))
    {
        Node  next;

        cursor.at++;

        if (!TryParseAnd (cursor, context, next))
        {
            return false;
        }

        outNode.children.push_back (std::move (next));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryParseAnd
//
//  Terms side by side, or with AND between them.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryParseAnd (Cursor & cursor, const std::wstring & context, Node & outNode)
{
    std::vector<Node>  items;
    bool               pendingAnd = false;



    while (!IsDone (cursor) && !IsAt (cursor, TokenKind::Or) && !IsAt (cursor, TokenKind::Close))
    {
        Node  item;

        if (IsAt (cursor, TokenKind::And))
        {
            if (items.empty() || pendingAnd)
            {
                return false;
            }

            pendingAnd = true;
            cursor.at++;
            continue;
        }

        if (!TryParseUnary (cursor, context, item))
        {
            return false;
        }

        items.push_back (std::move (item));
        pendingAnd = false;
    }

    if (items.empty() || pendingAnd)
    {
        return false;
    }

    if (items.size() == 1)
    {
        outNode = std::move (items[0]);
        return true;
    }

    outNode          = Node();
    outNode.kind     = Node::Kind::And;
    outNode.children = std::move (items);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryParseUnary
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryParseUnary (Cursor & cursor, const std::wstring & context, Node & outNode)
{
    Node  operand;



    if (!IsAt (cursor, TokenKind::Not) && !IsAt (cursor, TokenKind::Minus))
    {
        return TryParsePrimary (cursor, context, outNode);
    }

    cursor.at++;

    if (!TryParseUnary (cursor, context, operand))
    {
        return false;
    }

    outNode      = Node();
    outNode.kind = Node::Kind::Not;
    outNode.children.push_back (std::move (operand));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryParsePrimary
//
//  A group in parentheses, a term, or a property's word and colon before a
//  group, as in name:(a OR b), which applies the property to each bare term
//  in it.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryParsePrimary (Cursor & cursor, const std::wstring & context, Node & outNode)
{
    std::wstring  property;
    std::wstring  rest;
    std::wstring  inner      = context;
    std::wstring  text;
    bool          isProperty = false;



    if (IsAt (cursor, TokenKind::Word))
    {
        text       = cursor.tokens[cursor.at].text;
        isProperty = TrySplitProperty (text, property, rest);
        cursor.at++;

        if (!isProperty || !rest.empty() || !IsAt (cursor, TokenKind::Open))
        {
            return TryParseTerm ((isProperty || context.empty()) ? text : context + text, cursor.tree, outNode);
        }

        inner = text;
    }

    if (!IsAt (cursor, TokenKind::Open))
    {
        return false;
    }

    cursor.at++;

    if (!TryParseOr (cursor, inner, outNode) || !IsAt (cursor, TokenKind::Close))
    {
        return false;
    }

    cursor.at++;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TrySplitProperty
//
//  A word that starts with a letter and runs on in letters, digits and
//  periods to a colon, as System.Author: does, is a property's. A drive's
//  letter and colon before a backslash is a path instead.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TrySplitProperty (const std::wstring & text, std::wstring & outProperty, std::wstring & outRest)
{
    size_t  colon = text.find (L':');



    if (colon == std::wstring::npos || colon == 0 || !iswalpha (text[0]))
    {
        return false;
    }

    for (size_t at = 1; at < colon; at++)
    {
        if (!iswalnum (text[at]) && text[at] != L'.')
        {
            return false;
        }
    }

    if (colon == 1 && colon + 1 < text.size() && text[colon + 1] == L'\\')
    {
        return false;
    }

    outProperty = ToLower (text.substr (0, colon));
    outRest     = text.substr (colon + 1);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::GetField
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::Field SearchExpression::GetField (const std::wstring & property)
{
    for (const auto & [word, field] : s_kFields)
    {
        if (property == word)
        {
            return field;
        }
    }

    return Field::Other;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TakeOperator
//
//  The comparison at the start of a property's value, removed from it.
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::Operator SearchExpression::TakeOperator (std::wstring & ioValue)
{
    for (const auto & [text, op] : s_kOperators)
    {
        if (ioValue.starts_with (text))
        {
            ioValue.erase (0, wcslen (text));
            return op;
        }
    }

    return Operator::Default;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryParseTerm
//
//  A word, a phrase or a regular expression, alone or after a property and
//  its comparison. A property with nothing after it doesn't parse. An
//  extension is compared whole, with its period.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryParseTerm (const std::wstring & text, Tree & tree, Node & outNode)
{
    std::wstring  property;
    std::wstring  rest       = text;
    bool          isProperty = TrySplitProperty (text, property, rest);



    outNode = Node();

    if (isProperty)
    {
        outNode.field    = GetField (property);
        outNode.property = property;
        outNode.op       = TakeOperator (rest);
    }

    if (rest.empty())
    {
        return false;
    }

    outNode.raw = ToLowerUnquoted (text);
    ParseValue (rest, true, tree, outNode);

    if (outNode.field == Field::Extension && !outNode.wildcard && outNode.value[0] != L'.')
    {
        outNode.value = L"." + outNode.value;
    }

    if (outNode.field == Field::Extension && outNode.op == Operator::Default)
    {
        outNode.op = Operator::Equal;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::ParseValue
//
//  A value in quotes is a phrase, matched as written. One between slashes is
//  a regular expression where a name or a path is tested, and text anywhere
//  else. Two periods between two values are a range. A value with * or ? is
//  a pattern over the whole of what it tests.
//
////////////////////////////////////////////////////////////////////////////////

void SearchExpression::ParseValue (const std::wstring & raw, bool allowRegex, Tree & tree, Node & ioNode)
{
    constexpr size_t  kDelimited = 2;



    bool    phrase  = raw.size() >= kDelimited && raw.front() == L'"' && raw.back() == L'"';
    bool    slashes = raw.size() >= kDelimited && raw.front() == L'/' && raw.back() == L'/';
    bool    takesRe = ioNode.field == Field::Text || ioNode.field == Field::Name || ioNode.field == Field::Path || ioNode.field == Field::Folder;
    size_t  range   = std::wstring::npos;



    ioNode.value = ToLowerUnquoted (raw);

    if (slashes && takesRe && allowRegex)
    {
        try
        {
            tree.regexes.emplace_back (raw.substr (1, raw.size() - kDelimited), std::regex_constants::ECMAScript | std::regex_constants::icase);
            ioNode.regex = (int) tree.regexes.size() - 1;
        }
        catch (const std::regex_error &)
        {
            tree.error          = L"The regular expression " + raw + L" isn't valid.";
            ioNode.invalidRegex = true;
        }

        return;
    }

    ioNode.phrase = phrase;
    range         = phrase ? std::wstring::npos : ioNode.value.find (L"..");

    if (range != std::wstring::npos && ioNode.op == Operator::Default && range > 0 && range + kDelimited < ioNode.value.size())
    {
        ioNode.high  = ioNode.value.substr (range + kDelimited);
        ioNode.value = ioNode.value.substr (0, range);
        ioNode.op    = Operator::Range;
    }

    ioNode.wildcard = !phrase && ioNode.value.find_first_of (L"*?") != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MakePlainWords
//
//  Every word of the query, quoted phrases whole, each one matched as text.
//
////////////////////////////////////////////////////////////////////////////////

SearchExpression::Node SearchExpression::MakePlainWords (const std::wstring & query, Tree & tree)
{
    Node          all;
    std::wstring  word;
    bool          quoted = false;



    all.kind = Node::Kind::And;

    for (size_t at = 0; at <= query.size(); at++)
    {
        wchar_t  c   = (at < query.size()) ? query[at] : L' ';
        bool     end = iswspace (c) && !quoted;
        Node     term;

        if (c == L'"')
        {
            quoted = !quoted;
        }

        if (!end)
        {
            word += c;
            continue;
        }

        if (word.empty())
        {
            continue;
        }

        term.raw = ToLowerUnquoted (word);
        ParseValue (word, false, tree, term);
        all.children.push_back (std::move (term));
        word.clear();
    }

    return all;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::GetCost
//
//  The facts a listing has cost nothing to test. Kinds, types and other
//  properties are read from the file's property store, a bare word with
//  contents on can read its contents, and content: always reads them.
//
////////////////////////////////////////////////////////////////////////////////

int SearchExpression::GetCost (const Node & node, bool contents)
{
    constexpr int  kFacts      = 0;
    constexpr int  kProperties = 1;
    constexpr int  kText       = 2;
    constexpr int  kContents   = 3;



    int  cost = kFacts;



    if (node.kind != Node::Kind::Term)
    {
        for (const Node & child : node.children)
        {
            cost = (std::max) (cost, GetCost (child, contents));
        }

        return cost;
    }

    switch (node.field)
    {
        case Field::Kind:
        case Field::Type:
        case Field::Other:
            return kProperties;

        case Field::Text:
            return contents ? kText : kFacts;

        case Field::Content:
            return kContents;

        default:
            return kFacts;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::SortByCost
//
////////////////////////////////////////////////////////////////////////////////

void SearchExpression::SortByCost (Node & ioNode, bool contents)
{
    for (Node & child : ioNode.children)
    {
        SortByCost (child, contents);
    }

    std::stable_sort (ioNode.children.begin(), ioNode.children.end(), [contents] (const Node & a, const Node & b)
    {
        return GetCost (a, contents) < GetCost (b, contents);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchExpression::ToLower (const std::wstring & text)
{
    std::wstring  lower;



    lower.reserve (text.size());

    for (wchar_t c : text)
    {
        lower += (wchar_t) towlower (c);
    }

    return lower;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::ToLowerUnquoted
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SearchExpression::ToLowerUnquoted (const std::wstring & text)
{
    std::wstring  lower;



    for (wchar_t c : text)
    {
        if (c != L'"')
        {
            lower += (wchar_t) towlower (c);
        }
    }

    return lower;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::IsEmpty
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::IsEmpty() const
{
    return m_tree == nullptr || (m_tree->root.kind == Node::Kind::And && m_tree->root.children.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::HasTermWhere
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::HasTermWhere (const Node & node, const std::function<bool (const Node &)> & test)
{
    if (node.kind == Node::Kind::Term)
    {
        return test (node);
    }

    return std::any_of (node.children.begin(), node.children.end(), [&test] (const Node & child) { return HasTermWhere (child, test); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::HasRegex
//
//  A regular expression that isn't valid counts, so the query is walked and
//  reported rather than given to the index.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::HasRegex() const
{
    return !IsEmpty() && HasTermWhere (m_tree->root, [] (const Node & node) { return node.regex >= 0 || node.invalidRegex; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::HasWildcards
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::HasWildcards() const
{
    return !IsEmpty() && HasTermWhere (m_tree->root, [] (const Node & node) { return node.wildcard; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::GetNameHighlights
//
//  The words a result's name has whichever way it matched: name: terms, and
//  bare words when contents are off, under ANDs alone. A word under OR or
//  NOT, a pattern and a regular expression mark nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> SearchExpression::GetNameHighlights() const
{
    std::vector<std::wstring>  words;



    if (!IsEmpty())
    {
        CollectHighlights (m_tree->root, m_options.contents, words);
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::CollectHighlights
//
////////////////////////////////////////////////////////////////////////////////

void SearchExpression::CollectHighlights (const Node & node, bool contents, std::vector<std::wstring> & ioWords)
{
    bool  inName = node.field == Field::Name || (node.field == Field::Text && !contents);
    bool  marks  = node.op == Operator::Default || node.op == Operator::Contains || node.op == Operator::Equal || node.op == Operator::StartsWith;



    if (node.kind == Node::Kind::And)
    {
        for (const Node & child : node.children)
        {
            CollectHighlights (child, contents, ioWords);
        }

        return;
    }

    if (node.kind == Node::Kind::Term && inName && marks && node.regex < 0 && !node.invalidRegex && !node.wildcard && !node.value.empty())
    {
        ioWords.push_back (node.value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::Matches (const FileFacts & facts, ISearchSources & sources) const
{
    Prepared  file  { facts, ToLower (facts.name), std::wstring(), std::wstring(), ToLower (facts.path) };
    size_t    dot   = file.name.find_last_of (L'.');
    size_t    slash = file.path.find_last_of (L"\\/");



    if (IsEmpty())
    {
        return true;
    }

    file.extension = (facts.isFolder || dot == std::wstring::npos) ? std::wstring() : file.name.substr (dot);
    file.folder    = (slash == std::wstring::npos) ? std::wstring() : file.path.substr (0, slash);

    return Evaluate (m_tree->root, file, sources);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::Evaluate
//
//  AND stops at the first term that fails and OR at the first that matches,
//  so the costlier terms after them are read only when the result depends
//  on them.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::Evaluate (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    switch (node.kind)
    {
        case Node::Kind::And:
            return std::all_of (node.children.begin(), node.children.end(), [&] (const Node & child) { return Evaluate (child, file, sources); });

        case Node::Kind::Or:
            return std::any_of (node.children.begin(), node.children.end(), [&] (const Node & child) { return Evaluate (child, file, sources); });

        case Node::Kind::Not:
            return !Evaluate (node.children[0], file, sources);

        default:
            return EvaluateTerm (node, file, sources);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::EvaluateTerm
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::EvaluateTerm (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    bool  negate = node.op == Operator::DoesNotContain || node.op == Operator::NotEqual;



    switch (node.field)
    {
        case Field::Text:         return EvaluateText (node, file, sources);
        case Field::Name:         return MatchesString (node, file.name);
        case Field::Extension:    return MatchesString (node, file.extension);
        case Field::Folder:       return MatchesString (node, file.folder);
        case Field::Path:         return MatchesString (node, file.path);
        case Field::Size:         return MatchesSize (node, file);
        case Field::DateModified: return MatchesDate (node, file.facts.modified);
        case Field::DateCreated:  return MatchesDate (node, file.facts.created);
        case Field::DateAccessed: return MatchesDate (node, file.facts.accessed);
        case Field::Attributes:   return MatchesAttributes (node, file);
        case Field::Kind:         return MatchesKind (node, file, sources);
        case Field::Type:         return MatchesType (node, file, sources);
        case Field::Other:        return EvaluateOther (node, file, sources);
        case Field::Content:      return (!file.facts.isFolder && sources.ContainsText (node.value)) != negate;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::EvaluateText
//
//  A bare word or phrase is in the name, or with contents on, in the file's
//  text, which is read only when the name doesn't have it. A pattern or a
//  regular expression tests the name alone.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::EvaluateText (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    bool  textual = node.regex < 0 && !node.invalidRegex && !node.wildcard;



    if (MatchesString (node, file.name))
    {
        return true;
    }

    return m_options.contents && textual && !file.facts.isFolder && sources.ContainsText (node.value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::EvaluateOther
//
//  A property the Windows property system doesn't have is matched as text,
//  word and colon and all, as File Explorer matches it. A number compares as
//  a number.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::EvaluateOther (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    std::wstring            value;
    ISearchSources::Lookup  lookup = sources.GetProperty (node.property, value);
    double                  target = 0;
    double                  actual = 0;
    double                  high   = 0;
    bool                    negate = node.op == Operator::NotEqual || node.op == Operator::DoesNotContain;



    if (lookup == ISearchSources::Lookup::UnknownProperty)
    {
        return file.name.find (node.raw) != std::wstring::npos
            || (m_options.contents && !file.facts.isFolder && sources.ContainsText (node.raw));
    }

    if (lookup == ISearchSources::Lookup::NoValue)
    {
        return negate;
    }

    if (!TryGetNumber (value, actual) || !TryGetNumber (node.value, target))
    {
        return MatchesString (node, value);
    }

    switch (node.op)
    {
        case Operator::Less:           return actual < target;
        case Operator::Greater:        return actual > target;
        case Operator::LessOrEqual:    return actual <= target;
        case Operator::GreaterOrEqual: return actual >= target;
        case Operator::NotEqual:       return actual != target;
        case Operator::Range:          return TryGetNumber (node.high, high) && actual >= target && actual <= high;
        default:                       return actual == target;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesString
//
//  Without a comparison, the value is somewhere in the text.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesString (const Node & node, const std::wstring & lowerValue) const
{
    bool  negate = node.op == Operator::NotEqual || node.op == Operator::DoesNotContain;



    if (node.invalidRegex)
    {
        return false;
    }

    if (node.regex >= 0)
    {
        return std::regex_search (lowerValue, m_tree->regexes[(size_t) node.regex]) != negate;
    }

    if (node.wildcard)
    {
        return SearchQuery::MatchesPattern (lowerValue.c_str(), node.value.c_str()) != negate;
    }

    switch (node.op)
    {
        case Operator::Equal:          return lowerValue == node.value;
        case Operator::NotEqual:       return lowerValue != node.value;
        case Operator::Less:           return lowerValue < node.value;
        case Operator::Greater:        return lowerValue > node.value;
        case Operator::LessOrEqual:    return lowerValue <= node.value;
        case Operator::GreaterOrEqual: return lowerValue >= node.value;
        case Operator::StartsWith:     return lowerValue.starts_with (node.value);
        case Operator::DoesNotContain: return lowerValue.find (node.value) == std::wstring::npos;
        case Operator::Range:          return lowerValue >= node.value && lowerValue <= node.high;
        default:                       return lowerValue.find (node.value) != std::wstring::npos;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesSize
//
//  A keyword covers File Explorer's range for it, and a number with a unit
//  that unit's span from the number: size:1mb is 1 MB up to 2 MB. A folder
//  has no size.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesSize (const Node & node, const Prepared & file) const
{
    uint64_t  low     = 0;
    uint64_t  high    = 0;
    uint64_t  lastLow = 0;
    uint64_t  last    = 0;
    int64_t   bytes   = (int64_t) (std::min) (file.facts.sizeBytes, (uint64_t) INT64_MAX);



    if (file.facts.isFolder || !file.facts.hasSize || !TryGetSizeRange (node.value, low, high))
    {
        return false;
    }

    if (node.op == Operator::Range)
    {
        return TryGetSizeRange (node.high, lastLow, last) && bytes >= (int64_t) low && bytes < (int64_t) last;
    }

    return IsInRange (node.op, bytes, (int64_t) low, (int64_t) high);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryGetSizeRange
//
//  The bytes a size value covers, from the first up to but not including the
//  second. A keyword's range is moved up a byte, so its upper bound is in it,
//  as File Explorer's are.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryGetSizeRange (const std::wstring & value, uint64_t & outLow, uint64_t & outHigh)
{
    double        number = 0;
    std::wstring  digits = value;
    uint64_t      unit   = 1;



    if (value == L"empty")
    {
        outLow  = 0;
        outHigh = 1;
        return true;
    }

    for (const auto & [keyword, low, high] : s_kSizeKeywords)
    {
        if (value == keyword)
        {
            outLow  = low + 1;
            outHigh = high + 1;
            return true;
        }
    }

    for (const auto & [suffix, bytes] : s_kSizeUnits)
    {
        if (digits.ends_with (suffix))
        {
            digits.resize (digits.size() - wcslen (suffix));
            unit = bytes;
            break;
        }
    }

    while (!digits.empty() && iswspace (digits.back()))
    {
        digits.pop_back();
    }

    if (!TryGetNumber (digits, number) || number < 0)
    {
        return false;
    }

    outLow  = (uint64_t) (number * (double) unit);
    outHigh = outLow + unit;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::IsInRange
//
//  A value against the span a term's value covers, from low up to but not
//  including high: equal is within it, and greater is past its end.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::IsInRange (Operator op, int64_t value, int64_t low, int64_t high)
{
    switch (op)
    {
        case Operator::NotEqual:       return value < low || value >= high;
        case Operator::Less:           return value < low;
        case Operator::Greater:        return value >= high;
        case Operator::LessOrEqual:    return value < high;
        case Operator::GreaterOrEqual: return value >= low;
        default:                       return value >= low && value < high;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesDate
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesDate (const Node & node, const Date & date) const
{
    int64_t  start     = 0;
    int64_t  end       = 0;
    int64_t  lastStart = 0;
    int64_t  last      = 0;
    int64_t  day       = 0;



    if (!date.has || !TryGetDayRange (node.value, m_options, start, end))
    {
        return false;
    }

    day = RowGrouping::ToDayNumber (date.year, date.month, date.day);

    if (node.op == Operator::Range)
    {
        return TryGetDayRange (node.high, m_options, lastStart, last) && day >= start && day < last;
    }

    return IsInRange (node.op, day, start, end);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryGetDayRange
//
//  The days a date value covers, from the first up to but not including the
//  second: a keyword, a year, a year and month, a date in ISO 8601, or a
//  date in the user's short format.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryGetDayRange (const std::wstring & value, const MatchOptions & options, int64_t & outStart, int64_t & outEnd)
{
    return TryGetKeywordDays (value, options.today, outStart, outEnd) || TryGetNumericDays (value, options.shortDate, outStart, outEnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryGetKeywordDays
//
//  Weeks start on the locale's first day, as the date groups' do.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryGetKeywordDays (const std::wstring & value, const RowGrouping::Today & today, int64_t & outStart, int64_t & outEnd)
{
    constexpr int64_t  kThursday = 4;      // 1970-01-01 was a Thursday
    constexpr int64_t  kWeek     = 7;
    constexpr int      kDecember = 12;



    int64_t  t         = RowGrouping::ToDayNumber (today.year, today.month, today.day);
    int64_t  dow       = ((t % kWeek) + kWeek + kThursday) % kWeek;
    int64_t  weekStart = t - ((dow - today.firstDayOfWeek + kWeek) % kWeek);
    int      lastYear  = (today.month == 1) ? today.year - 1 : today.year;
    int      lastMonth = (today.month == 1) ? kDecember : today.month - 1;



    if      (value == L"today")      { outStart = t;                                          outEnd = t + 1;                                          }
    else if (value == L"yesterday")  { outStart = t - 1;                                      outEnd = t;                                              }
    else if (value == L"this week")  { outStart = weekStart;                                  outEnd = weekStart + kWeek;                              }
    else if (value == L"last week")  { outStart = weekStart - kWeek;                          outEnd = weekStart;                                      }
    else if (value == L"this month") { outStart = GetMonthStart (today.year, today.month);    outEnd = GetMonthStart (today.year, today.month + 1);    }
    else if (value == L"last month") { outStart = GetMonthStart (lastYear, lastMonth);        outEnd = GetMonthStart (today.year, today.month);        }
    else if (value == L"this year")  { outStart = GetMonthStart (today.year, 1);              outEnd = GetMonthStart (today.year + 1, 1);              }
    else if (value == L"last year")  { outStart = GetMonthStart (today.year - 1, 1);          outEnd = GetMonthStart (today.year, 1);                  }
    else                             { return false;                                                                                                   }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryGetNumericDays
//
//  ISO 8601 has a four-digit year first: 2026, 2026-11 or 2026-11-05, with a
//  time after the date ignored. Otherwise the three numbers are in the order
//  of the short date's d, M and y, with a two-digit year below 50 in this
//  century and any other in the last.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryGetNumericDays (const std::wstring & value, const std::wstring & shortDate, int64_t & outStart, int64_t & outEnd)
{
    constexpr size_t  kParts         = 3;
    constexpr size_t  kDayPart       = 0;
    constexpr size_t  kMonthPart     = 1;
    constexpr size_t  kYearPart      = 2;
    constexpr size_t  kIsoYearDigits = 4;
    constexpr int     kTwoDigitYears = 100;
    constexpr int     kCenturyPivot  = 50;
    constexpr int     kThisCentury   = 2000;
    constexpr int     kLastCentury   = 1900;
    constexpr int     kMonths        = 12;
    constexpr int     kMaxDay        = 31;



    std::vector<int>      numbers;
    std::vector<wchar_t>  order;
    size_t                firstDigits   = 0;
    int                   parts[kParts] = {};
    int                   year          = 0;
    int                   month         = 0;
    int                   day           = 0;



    if (!TrySplitNumbers (value.substr (0, value.find_first_of (L"t ")), numbers, firstDigits))
    {
        return false;
    }

    for (wchar_t c : shortDate)
    {
        if ((c == L'd' || c == L'M' || c == L'y') && std::find (order.begin(), order.end(), c) == order.end())
        {
            order.push_back (c);
        }
    }

    if (firstDigits == kIsoYearDigits)
    {
        year  = numbers[0];
        month = (numbers.size() > 1) ? numbers[1] : 0;
        day   = (numbers.size() > 2) ? numbers[2] : 0;
    }
    else if (numbers.size() == kParts && order.size() == kParts)
    {
        for (size_t at = 0; at < kParts; at++)
        {
            parts[(order[at] == L'd') ? kDayPart : (order[at] == L'M') ? kMonthPart : kYearPart] = numbers[at];
        }

        day   = parts[kDayPart];
        month = parts[kMonthPart];
        year  = parts[kYearPart];
        year  = (year >= kTwoDigitYears) ? year : (year < kCenturyPivot ? kThisCentury : kLastCentury) + year;
    }
    else
    {
        return false;
    }

    if (month < 0 || month > kMonths || day < 0 || day > kMaxDay || (month == 0 && day != 0))
    {
        return false;
    }

    outStart = (month == 0) ? GetMonthStart (year, 1)     : (day == 0) ? GetMonthStart (year, month)     : RowGrouping::ToDayNumber (year, month, day);
    outEnd   = (month == 0) ? GetMonthStart (year + 1, 1) : (day == 0) ? GetMonthStart (year, month + 1) : outStart + 1;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TrySplitNumbers
//
//  Up to three runs of digits, each pair apart by one character that is not
//  a digit, and the first run's length.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TrySplitNumbers (const std::wstring & date, std::vector<int> & outNumbers, size_t & outFirstDigits)
{
    constexpr size_t  kMaxParts = 3;



    std::wstring  run;



    outNumbers.clear();
    outFirstDigits = 0;

    for (size_t at = 0; at <= date.size(); at++)
    {
        bool  end = at == date.size();

        if (!end && iswdigit (date[at]))
        {
            run += date[at];
            continue;
        }

        if (run.empty() || outNumbers.size() == kMaxParts)
        {
            return false;
        }

        outFirstDigits = outNumbers.empty() ? run.size() : outFirstDigits;
        outNumbers.push_back (_wtoi (run.c_str()));
        run.clear();
    }

    return !outNumbers.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::GetMonthStart
//
//  The first day of a month, with month 13 as January of the next year.
//
////////////////////////////////////////////////////////////////////////////////

int64_t SearchExpression::GetMonthStart (int year, int month)
{
    constexpr int  kMonths = 12;



    return (month > kMonths) ? RowGrouping::ToDayNumber (year + 1, month - kMonths, 1) : RowGrouping::ToDayNumber (year, month, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesAttributes
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesAttributes (const Node & node, const Prepared & file) const
{
    bool  negate = node.op == Operator::NotEqual || node.op == Operator::DoesNotContain;



    if (!file.facts.hasAttributes)
    {
        return false;
    }

    for (const auto & [word, flag] : s_kAttributes)
    {
        if (node.value == word)
        {
            return ((file.facts.attributes & flag) != 0) != negate;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesKind
//
//  A folder is the kind folder without its properties being read.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesKind (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    std::vector<std::wstring>  kinds;
    bool                       negate = node.op == Operator::NotEqual || node.op == Operator::DoesNotContain;
    bool                       found  = false;



    if (node.value == L"folder")
    {
        return file.facts.isFolder != negate;
    }

    kinds = sources.GetKinds();
    found = std::find (kinds.begin(), kinds.end(), node.value) != kinds.end();

    return found != negate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::MatchesType
//
//  The extension, with or without its period, or a part of the type's text,
//  as Text Document is for a .txt file.
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::MatchesType (const Node & node, const Prepared & file, ISearchSources & sources) const
{
    std::wstring  dotted = (node.value.empty() || node.value[0] == L'.') ? node.value : L"." + node.value;
    bool          negate = node.op == Operator::NotEqual || node.op == Operator::DoesNotContain;
    bool          found  = !file.extension.empty() && file.extension == dotted;



    found = found || sources.GetTypeText().find (node.value) != std::wstring::npos;

    return found != negate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SearchExpression::TryGetNumber
//
////////////////////////////////////////////////////////////////////////////////

bool SearchExpression::TryGetNumber (const std::wstring & text, double & outNumber)
{
    wchar_t  * end = nullptr;



    if (text.empty() || (!iswdigit (text[0]) && text[0] != L'.' && text[0] != L'-'))
    {
        return false;
    }

    outNumber = wcstod (text.c_str(), &end);

    return end != nullptr && *end == L'\0';
}
