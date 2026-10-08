#include "Pch.h"

#include "Core/ParallelWorkPool.h"
#include "Core/ThreadName.h"
#include "Core/ThreadPoolWorkQueue.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ThreadNameTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ThreadNameTests
    //
    //  Casso's threads carry names a profiler shows, and a pool thread carries
    //  a pool's name only while it runs that pool's work.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ThreadNameTests)
    {
    public:

        TEST_METHOD (AScopeNamesTheThreadAndPutsTheOldNameBack)
        {
            std::wstring  before = ThreadName::Get();



            {
                ThreadName::Scope  named (L"Casso test scope");



                Assert::AreEqual (std::wstring (L"Casso test scope"), ThreadName::Get());
            }

            Assert::AreEqual (before, ThreadName::Get(), L"the old name is back");
        }


        //  An item on a named queue runs under the queue's name.
        TEST_METHOD (AQueuesItemRunsUnderTheQueuesName)
        {
            ThreadPoolWorkQueue  queue;
            std::wstring         seen;
            HRESULT              hr    = S_OK;



            hr = queue.Create (1, L"Casso test queue");
            Assert::AreEqual (S_OK, hr);

            hr = queue.Submit ([] (void * context) { *static_cast<std::wstring *> (context) = ThreadName::Get(); }, &seen);
            Assert::AreEqual (S_OK, hr);

            queue.WaitAll();

            Assert::AreEqual (std::wstring (L"Casso test queue"), seen);
        }


        //  A job the pool runs on its own threads runs under the pool's name.
        TEST_METHOD (APoolsJobsRunUnderThePoolsNameOnItsThreads)
        {
            static constexpr int    kJobs       = 8;
            static constexpr DWORD  kMaxThreads = 4;

            ParallelWorkPool                   pool;
            DWORD                              caller = GetCurrentThreadId();
            std::mutex                         lock;
            std::vector<std::wstring>          names;
            std::vector<IParallelRunner::Job>  jobs;
            HRESULT                            hr     = S_OK;



            hr = pool.Create (kMaxThreads, L"Casso test pool");
            Assert::AreEqual (S_OK, hr);

            for (int i = 0; i < kJobs; i++)
            {
                jobs.push_back ([&lock, &names, caller]
                {
                    std::lock_guard<std::mutex>  held (lock);



                    if (GetCurrentThreadId() != caller)
                    {
                        names.push_back (ThreadName::Get());
                    }

                    Sleep (1);
                });
            }

            pool.RunAll (jobs);

            Assert::IsFalse (names.empty(), L"some jobs ran on the pool's threads");

            for (const std::wstring & name : names)
            {
                Assert::AreEqual (std::wstring (L"Casso test pool"), name);
            }
        }
    };
}
