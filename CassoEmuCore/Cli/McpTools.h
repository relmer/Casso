#pragma once

#include "Core/JsonValue.h"
#include "Debugger/Channel/ChannelProtocol.h"

class ICassoLauncher;
class IInstanceDirectory;
class McpChannel;
struct McpChannelResult;





////////////////////////////////////////////////////////////////////////////////
//
//  McpToolResult
//
//  A tool call's result as MCP returns it: content blocks, structured
//  content, and whether the call failed.
//
////////////////////////////////////////////////////////////////////////////////

struct McpToolResult
{
    std::vector<JsonValue>    content;
    std::optional<JsonValue>  structured;
    bool                      isError = false;

    void  AddText  (const std::string & text);
    void  AddImage (const std::string & base64, const std::string & mimeType);
    void  Fail     (const std::string & text);
};





////////////////////////////////////////////////////////////////////////////////
//
//  McpTools
//
//  The tools `CassoCli mcp` offers, each built from debugger command lines
//  sent through the debug channel. The tools act on one Casso at a time: the
//  one `attach` or `launch` chose, else the only one running. `close` closes
//  only a Casso this server launched.
//
////////////////////////////////////////////////////////////////////////////////

class McpTools
{
public:
    static constexpr DWORD     kCommandTimeoutMs    = 10000;
    static constexpr DWORD     kHelloTimeoutMs      = 2000;
    static constexpr DWORD     kLaunchTimeoutMs     = 30000;
    static constexpr DWORD     kCloseTimeoutMs      = 15000;
    static constexpr DWORD     kPollMs              = 250;
    static constexpr int       kDefaultRunSeconds   = 30;
    static constexpr int       kMaxRunSeconds       = 600;
    static constexpr int       kMaxWaitSeconds      = 120;
    static constexpr uint64_t  kDefaultBudget       = 100000000;
    static constexpr int       kMaxReadLength       = 0x1000;

    //  The client name in the hello, which is in the console lines for host
    //  file access.
    static constexpr const char * kClientName = "MCP client";

    //  `client` is what the hello sends each Casso: the name, the extra
    //  folders and the scripts setting. `scratchFolder` is where a screenshot
    //  is written for its bytes to be read back; the temporary folder when
    //  empty. It has to be one of the client's folders for the write to be
    //  allowed.
    McpTools (IInstanceDirectory & directory, ICassoLauncher & launcher, ChannelClient client = {}, std::wstring scratchFolder = {});
    ~McpTools();

    McpTools (const McpTools &)             = delete;
    McpTools & operator= (const McpTools &) = delete;

    //  Each tool's name, description and input schema, for tools/list.
    static JsonValue    GetToolList ();

    //  False for a tool name the server doesn't have.
    bool                TryCall     (const std::string & name, const JsonValue & arguments, McpToolResult & result);

    //  A key name as `press_keys` takes it -- a character, Return, Escape,
    //  Tab, Space, Delete, Left, Right, Up, Down, Ctrl+letter or Ctrl+Reset --
    //  added to the KEY codes queued so far, or for Ctrl+Reset the lines
    //  that send them and reset. False with `error` set for any other
    //  name.
    static bool         TryAddKey   (const std::string & key, std::vector<Byte> & codes, std::vector<std::string> & lines, std::string & error);

    //  Text as PASTE takes it, in quotes: a newline, or CR LF, is \n.
    static std::string  EscapePaste (const std::string & text);

    static std::string  EncodeBase64 (const std::vector<Byte> & bytes);

private:
    using Handler = void (McpTools::*) (const JsonValue & arguments, McpToolResult & result);

    struct Tool
    {
        const char  * name;
        const char  * description;
        const char  * schema;
        Handler       handler;
    };

    static const Tool  s_kTools[];

    bool  EnsureAttached   (McpToolResult & result);
    bool  Connect          (uint32_t processId, std::string & error);
    void  RunLine          (const std::string & line, const std::string & mode, DWORD timeoutMs, uint64_t budget, bool overwrite, McpToolResult & result);
    void  RunLine          (const std::string & line, McpToolResult & result);
    void  ToResult         (const McpChannelResult & channel, McpToolResult & result);
    void  AddMachineState  (McpToolResult & result);

    static std::string  GetString (const JsonValue & arguments, const char * key, const std::string & fallback = std::string());
    static int          GetInt    (const JsonValue & arguments, const char * key, int fallback);

    void  ListInstances    (const JsonValue & arguments, McpToolResult & result);
    void  Launch           (const JsonValue & arguments, McpToolResult & result);
    void  Attach           (const JsonValue & arguments, McpToolResult & result);
    void  Close            (const JsonValue & arguments, McpToolResult & result);
    void  Command          (const JsonValue & arguments, McpToolResult & result);
    void  Run              (const JsonValue & arguments, McpToolResult & result);
    void  Resume           (const JsonValue & arguments, McpToolResult & result);
    void  Pause            (const JsonValue & arguments, McpToolResult & result);
    void  Step             (const JsonValue & arguments, McpToolResult & result);
    void  Registers        (const JsonValue & arguments, McpToolResult & result);
    void  ReadMemory       (const JsonValue & arguments, McpToolResult & result);
    void  WriteMemory      (const JsonValue & arguments, McpToolResult & result);
    void  Disassemble      (const JsonValue & arguments, McpToolResult & result);
    void  SetBreakpoint    (const JsonValue & arguments, McpToolResult & result);
    void  ClearBreakpoint  (const JsonValue & arguments, McpToolResult & result);
    void  ListBreakpoints  (const JsonValue & arguments, McpToolResult & result);
    void  Reset            (const JsonValue & arguments, McpToolResult & result);
    void  TypeText         (const JsonValue & arguments, McpToolResult & result);
    void  PressKeys        (const JsonValue & arguments, McpToolResult & result);
    void  InsertDisk       (const JsonValue & arguments, McpToolResult & result);
    void  EjectDisk        (const JsonValue & arguments, McpToolResult & result);
    void  ReadScreen       (const JsonValue & arguments, McpToolResult & result);
    void  Screenshot       (const JsonValue & arguments, McpToolResult & result);
    void  Wait             (const JsonValue & arguments, McpToolResult & result);

    IInstanceDirectory           & m_directory;
    ICassoLauncher               & m_launcher;
    std::unique_ptr<McpChannel>    m_channel;
    uint32_t                       m_processId = 0;
    ChannelClient                  m_client;
    std::wstring                   m_scratchFolder;
    std::set<uint32_t>             m_launched;
};
