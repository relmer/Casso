#include "Pch.h"

#include "Debugger/ReplyJson.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeBasicReport
//
//  The line, null in direct mode; a variable breakpoint's variable, the
//  line of the statement that changed it and its values; an error's code,
//  message and whether ONERR was on.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeBasicReport (const BasicStopReport & report)
{
    Members  members;



    members.emplace_back ("line", report.line.has_value() ? MakeNumber (*report.line) : JsonValue (nullptr));

    if (!report.variable.empty())
    {
        members.emplace_back ("variable",   MakeString (report.variable));
        members.emplace_back ("changeLine", report.changeLine.has_value() ? MakeNumber (*report.changeLine) : JsonValue (nullptr));
        members.emplace_back ("previous",   MakeString (report.previous));
        members.emplace_back ("value",      MakeString (report.value));
    }

    if (report.errorCode.has_value())
    {
        members.emplace_back ("code",    MakeNumber (*report.errorCode));
        members.emplace_back ("error",   MakeString (report.error));
        members.emplace_back ("handled", JsonValue (report.isHandled));
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeBasicVariables
//
//  Every value as PRINT prints it; an array's sizes are its subscripts'
//  counts, first subscript first, and its values vary the first subscript
//  fastest.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeBasicVariables (const BasicVariablesData & data)
{
    static constexpr const char * kTypes[] = { "real", "integer", "string" };
    std::vector<JsonValue>        variables;
    std::vector<JsonValue>        arrays;



    for (const BasicVariable & variable : data.variables)
    {
        variables.push_back (JsonValue (Members { { "name",    MakeString (variable.name) },
                                                  { "type",    MakeString (kTypes[(int) variable.type]) },
                                                  { "address", MakeNumber (variable.address) },
                                                  { "value",   MakeString (variable.value) } }));
    }

    for (const BasicArray & array : data.arrays)
    {
        std::vector<JsonValue>  sizes;



        std::ranges::transform (array.sizes, std::back_inserter (sizes), [] (int size) { return MakeNumber (size); });

        arrays.push_back (JsonValue (Members { { "name",    MakeString (array.name) },
                                               { "type",    MakeString (kTypes[(int) array.type]) },
                                               { "address", MakeNumber (array.address) },
                                               { "sizes",   JsonValue (std::move (sizes)) },
                                               { "values",  MakeTextArray (array.values) } }));
    }

    return JsonValue (Members { { "kind",      MakeString ("basicVariables") },
                                { "variables", JsonValue (std::move (variables)) },
                                { "arrays",    JsonValue (std::move (arrays)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeBasicStack
//
//  Newest first; a FOR frame's variable, limit and step, as PRINT prints
//  them, only on a FOR frame.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeBasicStack (const BasicStackData & data)
{
    std::vector<JsonValue>  frames;



    for (const BasicFrame & frame : data.frames)
    {
        Members  members = { { "kind",    MakeString ((frame.kind == BasicFrameKind::Gosub) ? "gosub" : "for") },
                             { "address", MakeNumber (frame.address) },
                             { "line",    MakeNumber (frame.line) } };



        if (frame.kind == BasicFrameKind::For)
        {
            members.emplace_back ("variable", MakeString (frame.variable));
            members.emplace_back ("limit",    MakeString (frame.limit));
            members.emplace_back ("step",     MakeString (frame.step));
        }

        frames.push_back (JsonValue (std::move (members)));
    }

    return JsonValue (Members { { "kind", MakeString ("basicStack") }, { "frames", JsonValue (std::move (frames)) } });
}
