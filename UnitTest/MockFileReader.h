#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockFileReader
//
//  An in-memory FileReader so file inclusion can be tested without touching the
//  filesystem.
//
//  That is what makes include testing practical at all -- a nested include
//  graph, a circular include, and a missing file are all a few table entries
//  here, where on disk each would be a fixture tree to create and clean up.
//
//  It records which paths were REQUESTED, not just what it returned, so a test
//  can assert the resolution order and that a file was read once rather than
//  repeatedly. A request is recorded even when it misses, because a lookup that
//  found nothing is exactly what a resolution-order test needs to see.
//
//  Path matching is exact and deliberately unforgiving, which is how the
//  assembler's own path resolution stays the thing under test rather than being
//  papered over by a lenient double.
//
//  Shared rather than nested in one test file because the Merlin corpus serves
//  its multi-file entries through this same seam.
//
////////////////////////////////////////////////////////////////////////////////

class MockFileReader : public FileReader
{
public:
    std::unordered_map<std::string, std::string>  files;
    std::vector<std::string>                      requests;



    FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) override
    {
        FileReadResult                                                 result = {};
        std::unordered_map<std::string, std::string>::const_iterator   found  = files.find (filename);
        bool                                                           wasFound = found != files.end();



        requests.push_back (filename);

        if (wasFound)
        {
            result.success  = true;
            result.contents = found->second;
        }
        else
        {
            result.success = false;
            result.error   = "File not found: " + filename;
        }

        return result;
    }



    // How many times a path was asked for. Zero for a path never requested,
    // which is the assertion a "read once" test actually needs.
    int CountRequests (const std::string & filename) const
    {
        int count = 0;



        for (const std::string & requested : requests)
        {
            if (requested == filename)
            {
                count++;
            }
        }

        return count;
    }
};
