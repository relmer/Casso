#include "Pch.h"

#include "AssemblerExitCode.h"
#include "AssemblerTypes.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AssemblerExitCodeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Results in the three shapes the mapping has to tell apart, built by hand
    //  rather than by assembling source. The mapping reads two fields; running an
    //  assembly to produce them would test the assembler instead.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblyResult MakeResult (bool success)
        {
            AssemblyResult  result;

            result.success = success;

            return result;
        }



        static AssemblyError MakeDiagnostic (const char * message)
        {
            AssemblyError  diagnostic = {};

            diagnostic.lineNumber = 1;
            diagnostic.message    = message;

            return diagnostic;
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExitCodeVocabulary
    //
    //  The numbers themselves. They are the contract a build script reads, so
    //  they are pinned here rather than left to whatever order the enumerators
    //  happen to be written in.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExitCodeVocabulary)
    {
    public:

        TEST_METHOD (CleanIsZero)
        {
            Assert::AreEqual (0, AssemblerExitCode::ToProcessCode (AssemblyExitCode::Clean));
        }



        TEST_METHOD (AssembledWithWarningsIsOne)
        {
            Assert::AreEqual (1, AssemblerExitCode::ToProcessCode (AssemblyExitCode::AssembledWithWarnings));
        }



        TEST_METHOD (NoOutputIsTwo)
        {
            constexpr int  kNoOutput = 2;

            Assert::AreEqual (kNoOutput, AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput));
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExitCodeFromResult
    //
    //  What each outcome earns.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExitCodeFromResult)
    {
    public:

        TEST_METHOD (AssembledWithNothingToSay_IsClean)
        {
            AssemblyResult  result = Fixture::MakeResult (true);

            Assert::IsTrue (AssemblyExitCode::Clean == AssemblerExitCode::FromResult (result));
        }



        TEST_METHOD (AssembledWithAWarning_SaysSo)
        {
            AssemblyResult  result = Fixture::MakeResult (true);

            result.warnings.push_back (Fixture::MakeDiagnostic ("branch out of range was widened"));

            Assert::IsTrue (AssemblyExitCode::AssembledWithWarnings == AssemblerExitCode::FromResult (result));
        }



        TEST_METHOD (FailedAssembly_IsNoOutput)
        {
            AssemblyResult  result = Fixture::MakeResult (false);

            result.errors.push_back (Fixture::MakeDiagnostic ("unknown instruction"));

            Assert::IsTrue (AssemblyExitCode::NoOutput == AssemblerExitCode::FromResult (result));
        }



        //  A failed assembly that also warned is still a failed assembly. This
        //  is the case a mapping that consults warnings first gets wrong, and it
        //  gets it wrong in the worst direction: it tells a script an output
        //  file exists when none was written.
        TEST_METHOD (FailedAssemblyThatAlsoWarned_IsStillNoOutput)
        {
            AssemblyResult  result = Fixture::MakeResult (false);

            result.errors.push_back   (Fixture::MakeDiagnostic ("undefined symbol"));
            result.warnings.push_back (Fixture::MakeDiagnostic ("branch out of range was widened"));

            Assert::IsTrue (AssemblyExitCode::NoOutput == AssemblerExitCode::FromResult (result));
        }



        //  A result nobody filled in means "failed, nothing assembled", and the
        //  mapping has to agree with that default rather than read an unset flag
        //  as success.
        TEST_METHOD (AResultNobodyFilledIn_IsNoOutput)
        {
            AssemblyResult  result;

            Assert::IsTrue (AssemblyExitCode::NoOutput == AssemblerExitCode::FromResult (result));
        }



        //  A construct the dialect refuses on purpose does NOT get a code of its
        //  own: it earns exactly what a syntax error earns, and the refusal is
        //  read off the message. Asserting the two are EQUAL is the point --
        //  asserting each is 2 separately would still pass if one of them later
        //  acquired a code of its own and the other did not.
        TEST_METHOD (ASubsetRefusalAndASyntaxError_EarnTheSameCode)
        {
            AssemblyResult  refusal     = Fixture::MakeResult (false);
            AssemblyResult  syntaxError = Fixture::MakeResult (false);

            refusal.errors.push_back     (Fixture::MakeDiagnostic ("relocatable assembly is outside the supported subset"));
            syntaxError.errors.push_back (Fixture::MakeDiagnostic ("unknown instruction"));

            Assert::IsTrue (AssemblerExitCode::FromResult (refusal) == AssemblerExitCode::FromResult (syntaxError));
            Assert::IsTrue (AssemblyExitCode::NoOutput == AssemblerExitCode::FromResult (refusal));
        }
    };
}
