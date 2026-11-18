#include "Pch.h"

#include "CassoExplorer/Model/PreviewSaveChoices.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSaveChoices::Build
//
////////////////////////////////////////////////////////////////////////////////

void PreviewSaveChoices::Build (const PreviewContent & preview, const std::wstring & convertedName, std::vector<Choice> & outChoices)
{
    std::wstring  converted = std::filesystem::path (convertedName).extension().wstring();
    bool          isText    = preview.kind == PreviewContent::Kind::Text || preview.kind == PreviewContent::Kind::Listing
                           || preview.kind == PreviewContent::Kind::Hex  || preview.kind == PreviewContent::Kind::Document
                           || preview.kind == PreviewContent::Kind::Grid || preview.kind == PreviewContent::Kind::Variables
                           || preview.kind == PreviewContent::Kind::Catalog;
    auto          add       = [&outChoices] (Kind kind, const std::wstring & label, std::wstring extension)
    {
        bool  taken = false;

        std::transform (extension.begin(), extension.end(), extension.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

        for (const Choice & choice : outChoices)
        {
            taken = taken || choice.extension == extension;
        }

        if (!taken && !extension.empty())
        {
            outChoices.push_back (Choice { kind, label, extension });
        }
    };



    outChoices.clear();

    if (preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty())
    {
        add (Kind::ViewPicture, L"PNG picture", L"png");
    }
    else if (isText)
    {
        add (Kind::ViewText, L"Text as displayed", L"txt");
    }

    if (converted.size() > 1)
    {
        add (Kind::Converted, L"As Get copies it", converted.substr (1));
    }

    add (Kind::Original,    L"Original bytes",                       L"bin");
    add (Kind::AppleSingle, L"AppleSingle, with type and resource fork", L"as");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSaveChoices::Match
//
////////////////////////////////////////////////////////////////////////////////

PreviewSaveChoices::Kind PreviewSaveChoices::Match (const std::vector<Choice> & choices, const std::filesystem::path & picked)
{
    std::wstring  extension = picked.extension().wstring();



    for (const Choice & choice : choices)
    {
        if (extension.size() > 1 && _wcsicmp (extension.c_str() + 1, choice.extension.c_str()) == 0)
        {
            return choice.kind;
        }
    }

    return Kind::Original;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSaveChoices::GetViewText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewSaveChoices::GetViewText (const PreviewContent & preview)
{
    std::wstring  text;



    switch (preview.kind)
    {
        case PreviewContent::Kind::Document:
            for (const Paragraph & paragraph : preview.paragraphs)
            {
                for (const Run & run : paragraph.runs)
                {
                    text += run.text;
                }

                text += kLineEnd;
            }

            break;

        case PreviewContent::Kind::Grid:
            for (size_t row = 0; row <= preview.grid.rows.size(); row++)
            {
                const std::vector<std::wstring> &  cells = (row == 0) ? preview.grid.columns : preview.grid.rows[row - 1];

                for (size_t cell = 0; cell < cells.size(); cell++)
                {
                    text += (cell > 0 ? L"\t" : L"") + cells[cell];
                }

                text += kLineEnd;
            }

            break;

        case PreviewContent::Kind::Variables:
            for (const VariableRow & variable : preview.variables)
            {
                text += variable.name + L"\t" + variable.type + L"\t" + variable.value + kLineEnd;
            }

            break;

        case PreviewContent::Kind::Hex:
            if (preview.lines.empty())
            {
                for (size_t at = 0; at < preview.bytes.size(); at += kHexBytesPerLine)
                {
                    std::wstring  chars;

                    text += std::format (L"{:04X} ", (unsigned) (preview.origin + at));

                    for (size_t i = at; i < at + kHexBytesPerLine; i++)
                    {
                        Byte  value = (i < preview.bytes.size()) ? preview.bytes[i] : (Byte) 0;
                        Byte  plain = (Byte) (value & 0x7F);

                        text  += (i < preview.bytes.size()) ? std::format (L" {:02X}", value) : std::wstring (L"   ");
                        chars += (i >= preview.bytes.size()) ? L"" : (plain >= 0x20 && plain < 0x7F) ? std::wstring (1, (wchar_t) plain) : std::wstring (L".");
                    }

                    text += L"  " + chars + kLineEnd;
                }

                break;
            }

            [[fallthrough]];

        default:
            for (const std::wstring & line : preview.lines)
            {
                text += line + kLineEnd;
            }

            break;
    }

    return text;
}
