#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorLink
//
//  What the CassoLink tests against another emulator share: a session sink
//  to wait on, the emulator's process, ended when the test ends, and helpers
//  for settings, host files, listings and operations.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorLink
{
public:
    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::condition_variable                       changed;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::vector<LinkState>                        states;
        bool                                          savingReads = false;

        bool  HasFullSendReadsOn () override
        {
            return savingReads;
        }

        void  OnStateChanged (LinkState state) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            states.push_back (state);
            changed.notify_all();
        }

        void  OnProgress (const std::shared_ptr<LinkOperation> &) override
        {
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
            changed.notify_all();
        }

        bool  WaitForDone (const std::shared_ptr<LinkOperation> & operation, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return std::find (done.begin(), done.end(), operation) != done.end(); });
        }

        bool  WaitForState (LinkState state, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return !states.empty() && states.back() == state; });
        }
    };


    //  The emulator, with no window to take the focus. When the test ends the
    //  process is ended and the stop command, if any, is run.
    struct Process
    {
        static constexpr DWORD  kStopMs = 30'000;

        PROCESS_INFORMATION  process = {};
        std::wstring         stopCommand;

        ~Process()
        {
            if (process.hProcess != nullptr)
            {
                TerminateProcess (process.hProcess, 0);
                WaitForSingleObject (process.hProcess, INFINITE);
                CloseHandle (process.hProcess);
                CloseHandle (process.hThread);
            }

            if (!stopCommand.empty())
            {
                Run (stopCommand);
            }
        }

        void  Start (std::wstring command, const std::filesystem::path & folder)
        {
            STARTUPINFOW  startup = { sizeof (startup) };
            BOOL          started = FALSE;



            started = CreateProcessW (nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                                      folder.c_str(), &startup, &process);
            Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (started != FALSE, L"the emulator starts");
        }

    private:
        static void  Run (std::wstring command)
        {
            STARTUPINFOW         startup = { sizeof (startup) };
            PROCESS_INFORMATION  stop    = {};



            if (CreateProcessW (nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &stop))
            {
                WaitForSingleObject (stop.hProcess, kStopMs);
                CloseHandle (stop.hProcess);
                CloseHandle (stop.hThread);
            }
        }
    };


    static std::string  GetSetting (const char * name)
    {
        char         * value  = nullptr;
        size_t         length = 0;
        std::string    text;



        if (_dupenv_s (&value, &length, name) == 0 && value != nullptr)
        {
            text = value;
            free (value);
        }

        return text;
    }


    //  A command line that runs one of scripts\EmuTargets's scripts, found
    //  from this source file's place in the tree, with its arguments.
    static std::wstring  MakeTargetCommand (const wchar_t * script, const std::wstring & arguments)
    {
        std::filesystem::path  folder = std::filesystem::path (__FILE__).parent_path().parent_path() / L"scripts" / L"EmuTargets";



        return std::format (L"pwsh.exe -NoProfile -File \"{}\" {}", (folder / script).wstring(), arguments);
    }


    //  The first line a command writes, with no line end; empty if the
    //  command can't be run.
    static std::string  ReadCommandLine (const wchar_t * command)
    {
        FILE         * pipe  = _wpopen (command, L"rt");
        char           buffer[256] = {};
        std::string    line;



        if (pipe != nullptr)
        {
            if (fgets (buffer, (int) sizeof (buffer), pipe) != nullptr)
            {
                line = buffer;
            }

            _pclose (pipe);
        }

        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
        {
            line.pop_back();
        }

        return line;
    }


    static void  WriteHostFile (const std::filesystem::path & path, std::span<const Byte> bytes)
    {
        std::ofstream  out (path, std::ios::binary);



        out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
    }


    static std::vector<std::string>  GetNames (const VolumeListing & listing)
    {
        std::vector<std::string>  files;



        for (const FileEntry & entry : listing.entries)
        {
            files.push_back (entry.name);
        }

        return files;
    }


    static std::shared_ptr<LinkOperation>  Finish (Sink & sink, std::shared_ptr<LinkOperation> operation, DWORD timeoutMs)
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (sink.WaitForDone (operation, timeoutMs), L"the operation finishes");
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (SUCCEEDED (operation->result), std::format (L"{:08X} {} at {}%", (uint32_t) operation->result, TextEncoding::NarrowToWide (operation->message), operation->progress.load()).c_str());

        return operation;
    }


    static FilePayload  MakeBinary (size_t size, Byte seed)
    {
        FilePayload  payload;



        payload.bytes.resize (size);

        for (size_t i = 0; i < size; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 13);
        }

        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }
};
