#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName
//
//  The name a thread shows in a debugger, a profiler and the CPU sampler. A
//  thread Casso creates is named once, as it starts. A pool thread runs other
//  work too, so a callback names it only while the callback runs, putting the
//  pool's own name back when it returns.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadName
{
public:
    //  Names the calling thread for the rest of its life.
    static void  Set (const wchar_t * name);

    //  Names the calling thread until the object goes out of scope, then puts
    //  back the name it had.
    class Scope
    {
    public:
        explicit Scope (const wchar_t * name);
        ~Scope();

        Scope             (const Scope &) = delete;
        Scope & operator= (const Scope &) = delete;

    private:
        std::wstring  m_previous;
        bool          m_isNamed = false;
    };

    //  The calling thread's name, empty if it has none.
    static std::wstring  Get ();
};
