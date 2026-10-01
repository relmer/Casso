#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ByteChanges
//
//  Which bytes of the 64K changed, for a pane that shows bytes in the changed
//  color. It is StopChanges for bytes, kept by address so a view that scrolls
//  compares a byte with the value last seen at that address wherever it was
//  shown.
//
//  WHILE THE MACHINE IS STOPPED THE MARKS HOLD until a later snapshot shows a
//  byte with a different value: a step, an edit in any pane, a poke typed at
//  the console. A snapshot that only repeats what was shown leaves them, so a
//  paused machine's repeated snapshots do not wipe them out. While it runs,
//  each snapshot marks what changed since the one before.
//
//  A byte never seen before is new, not changed.
//
////////////////////////////////////////////////////////////////////////////////

class ByteChanges
{
public:
    //  One snapshot's bytes: an address and the value read there.
    using Seen = std::vector<std::pair<Word, Byte>>;

    ByteChanges();

    void  Update    (bool isPaused, const Seen & seen);
    bool  IsChanged (Word address) const { return m_changed[address]; }
    void  Clear     ();

    //  An edit of `count` bytes from `address`, as an undo item or tip gives
    //  it: "changed 2 bytes at $0300".
    static std::wstring  GetEditText (Word address, size_t count);

    //  A disassembly row's bytes, written "A9 05" with "--" for one that could
    //  not be read, as the bytes from `address` they show.
    static Seen  ParseRowBytes (Word address, const std::string & text);

    //  The character ranges [first, last) of a row's bytes that changed.
    std::vector<std::pair<int, int>>  GetChangedRanges (Word address, const std::string & text) const;

private:
    static constexpr size_t  kAddressSpace = 0x10000;
    static constexpr int     kUnknown      = -1;

    std::vector<int>   m_known;
    std::vector<bool>  m_changed;
};
