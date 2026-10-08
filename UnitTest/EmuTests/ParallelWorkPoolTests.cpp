#include "Pch.h"

#include "Core/ParallelWorkPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ParallelWorkPoolTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ParallelWorkPoolTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ParallelWorkPoolTests)
    {
    public:

        //  Every job runs exactly once, and RunAll returns only after the last.
        TEST_METHOD (EveryJobRunsOnceBeforeRunAllReturns)
        {
            static constexpr int  kJobs       = 64;
            static constexpr int  kMaxThreads = 4;

            ParallelWorkPool                    pool;
            std::array<std::atomic<int>, kJobs> runs  = {};
            std::vector<IParallelRunner::Job>   jobs;
            HRESULT                             hr    = S_OK;



            hr = pool.Create (kMaxThreads);
            Assert::AreEqual (S_OK, hr);

            for (int i = 0; i < kJobs; i++)
            {
                jobs.push_back ([&runs, i] { runs[(size_t) i]++; });
            }

            pool.RunAll (jobs);

            for (int i = 0; i < kJobs; i++)
            {
                Assert::AreEqual (1, runs[(size_t) i].load(), L"each job runs once");
            }
        }


        //  Two jobs that each wait for the other finish only if they run at the
        //  same time. Each gives up after a bound, so a pool that runs them one
        //  after the other fails instead of hanging.
        TEST_METHOD (JobsRunAtTheSameTime)
        {
            static constexpr int       kJobs       = 2;
            static constexpr int       kMaxThreads = 2;
            static constexpr uint32_t  kWaitMs     = 5000;

            ParallelWorkPool                   pool;
            std::atomic<int>                   arrived = 0;
            std::atomic<int>                   met     = 0;
            std::vector<IParallelRunner::Job>  jobs;
            HRESULT                            hr      = S_OK;



            hr = pool.Create (kMaxThreads);
            Assert::AreEqual (S_OK, hr);

            for (int i = 0; i < kJobs; i++)
            {
                jobs.push_back ([&arrived, &met]
                {
                    ULONGLONG  until = GetTickCount64() + kWaitMs;



                    arrived++;

                    while (arrived.load() < kJobs && GetTickCount64() < until)
                    {
                        SwitchToThread();
                    }

                    if (arrived.load() == kJobs)
                    {
                        met++;
                    }
                });
            }

            pool.RunAll (jobs);

            Assert::AreEqual (kJobs, met.load(), L"both jobs saw the other running");
        }


        //  With no pool yet, the jobs still run, on the calling thread.
        TEST_METHOD (APoolNotCreatedRunsTheJobsInline)
        {
            ParallelWorkPool                   pool;
            DWORD                              caller = GetCurrentThreadId();
            std::vector<DWORD>                 ran;
            std::vector<IParallelRunner::Job>  jobs;



            jobs.push_back ([&ran] { ran.push_back (GetCurrentThreadId()); });
            jobs.push_back ([&ran] { ran.push_back (GetCurrentThreadId()); });

            pool.RunAll (jobs);

            Assert::AreEqual ((size_t) 2, ran.size(), L"both ran");
            Assert::AreEqual (caller, ran[0]);
            Assert::AreEqual (caller, ran[1]);
        }


        //  An empty set returns at once.
        TEST_METHOD (AnEmptySetReturns)
        {
            ParallelWorkPool                   pool;
            std::vector<IParallelRunner::Job>  jobs;
            HRESULT                            hr = S_OK;



            hr = pool.Create (2);
            Assert::AreEqual (S_OK, hr);

            pool.RunAll (jobs);
        }
    };
}
