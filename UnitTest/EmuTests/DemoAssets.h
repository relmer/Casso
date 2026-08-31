#pragma once

//
//  The casso-rocks demo's sources and payloads, as resources in the test
//  assembly rather than as files on the machine running the test.
//
//  WHY THEY ARE NOT FILES ANY MORE. This test read all eight off the repo at
//  run time, and for a long while wrote the built disk image back over
//  Apple2/Demos/casso-rocks.dsk as well. The write normally put the same bytes
//  there, so nothing showed but a changed timestamp, until a run that built a
//  different image and left a corrupted binary asset in the tree.
//
//  A test that reads the tree is a smaller version of the same mistake: it
//  reports on the state of the machine it happens to be running on, and it
//  skipped itself entirely on a checkout arranged differently, which is a test
//  that can pass by not running. Reading them at BUILD time, through the
//  resource compiler, is reading them where reading is the job.
//
//  The ids are plain integers because that is what RCDATA takes. They start
//  well clear of anything Casso.rc uses, though nothing links both.
//

#define IDR_DEMO_STAGE1_SRC   4001
#define IDR_DEMO_STAGE2_SRC   4002
#define IDR_DEMO_HGR          4003
#define IDR_DEMO_HGR_MONO     4004
#define IDR_DEMO_DHGR_AUX     4006
#define IDR_DEMO_DHGR_MAIN    4007

//
//  The Applesoft construct corpus: our own listing, and the bytes Applesoft
//  itself stored for it. Regeneration goes through a booted guest -- see
//  UnitTest/Fixtures/Basic/construct-inventory.md, circularity guard included.
//

#define IDR_BASIC_CORPUS_SRC  4008
#define IDR_BASIC_CORPUS_TOK  4009

//
//  The monochrome cassowaries: the same photo as IDR_DEMO_DHGR_* and
//  IDR_DEMO_HGR, encoded for the other decode each framebuffer has.
//  See scripts/DhgrCassowaryGen.py and scripts/HgrCassowaryGen.py.
//

#define IDR_DEMO_DHGR_MONO_AUX   4010
#define IDR_DEMO_DHGR_MONO_MAIN  4011

#ifndef RC_INVOKED





////////////////////////////////////////////////////////////////////////////////
//
//  DemoAssets
//
//  Reads one of the embedded payloads above.
//
//  EVERY FAILURE HERE IS A BUILD FAILURE, not a condition a test should carry
//  on past: a resource that is not in the assembly was not compiled into it,
//  and no amount of retrying at run time will produce it. So the accessors
//  assert rather than return an error nobody could act on.
//
////////////////////////////////////////////////////////////////////////////////

class DemoAssets
{
public:
    //  The bytes of one embedded payload. Valid for the life of the process:
    //  a locked resource is a pointer into the loaded module image, not a
    //  copy, so nothing here allocates and nothing needs freeing.
    static std::span<const Byte>  Bytes (int resourceId);

    //  The same, as text, for the two .a65 sources the test assembles.
    static std::string            GetText (int resourceId);

    //  And as a vector, for the callers that want to own it.
    static std::vector<Byte>      Copy    (int resourceId);
};

#endif  // RC_INVOKED
