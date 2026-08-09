#include "Pch.h"

#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue::Find
//
////////////////////////////////////////////////////////////////////////////////

const JsonValue * JsonValue::Find (const string & key) const
{
    const JsonValue *  found = nullptr;



    // Insertion-ordered scan: a duplicate key resolves to the FIRST one, which
    // is what a merge layered on top of a base expects.
    for (const auto & entry : m_object)
    {
        if (found == nullptr && entry.first == key)
        {
            found = &entry.second;
        }
    }

    return found;
}
