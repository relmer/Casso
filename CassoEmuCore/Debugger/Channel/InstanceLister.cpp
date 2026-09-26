#include "Pch.h"

#include "Debugger/Channel/InstanceLister.h"

#include "Core/JsonParser.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstanceLister::List
//
//  Replies other than the hello -- a notification the instance happened to
//  send first -- are read past, bounded by the same timeout, so an instance
//  that is busy talking is still listed.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ListedInstance> InstanceLister::List (IInstanceDirectory & directory, DWORD helloTimeoutMs)
{
    std::vector<ListedInstance>  listed;



    for (uint32_t processId : directory.ListProcessIds())
    {
        std::unique_ptr<IChannelClient>  client   = directory.Connect (processId);
        ListedInstance                   instance;
        std::string                      record;
        ULONGLONG                        deadline = GetTickCount64() + helloTimeoutMs;
        bool                             answered = false;



        if (client == nullptr)
        {
            continue;
        }

        if (!client->WriteLine (R"({"type":"hello","id":1,"protocol":1})"))
        {
            continue;
        }

        while (!answered && GetTickCount64() < deadline)
        {
            DWORD  remaining = (DWORD) (deadline - GetTickCount64());

            if (!client->ReadLine (record, remaining))
            {
                break;
            }

            answered = TryParseHello (record, instance);
        }

        if (answered)
        {
            listed.push_back (std::move (instance));
        }
    }

    return listed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstanceLister::FormatJson
//
//  Each instance's hello record, one per line, as the channel sent it.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstanceLister::FormatJson (const std::vector<ListedInstance> & instances)
{
    std::string  output;



    for (const ListedInstance & instance : instances)
    {
        output += instance.record + "\n";
    }

    return output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstanceLister::TryParseHello
//
////////////////////////////////////////////////////////////////////////////////

bool InstanceLister::TryParseHello (const std::string & record, ListedInstance & instance)
{
    JsonValue          value;
    JsonParseError     error;
    HRESULT            hr     = JsonParser::Parse (record, value, error);
    std::string        type;
    uint32_t           pid    = 0;
    const JsonValue  * disks  = nullptr;



    if (FAILED (hr) || !value.HasString ("type", type) || type != "hello" || !value.HasUint32 ("pid", pid))
    {
        return false;
    }

    instance           = ListedInstance();
    instance.processId = pid;
    instance.record    = record;

    //  Both are optional: an instance started with no title label has none.
    if (!value.HasString ("title", instance.title))
    {
        instance.title.clear();
    }

    if (!value.HasString ("machine", instance.machine))
    {
        instance.machine.clear();
    }

    if (value.HasArray ("disks", disks))
    {
        for (size_t i = 0; i < disks->GetArraySize(); i++)
        {
            const JsonValue & disk = disks->GetArrayElement (i);

            if (disk.GetType() == JsonType::String)
            {
                instance.disks.push_back (disk.GetString());
            }
            else
            {
                instance.disks.push_back (std::nullopt);
            }
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstanceLister::Format
//
//  Tab-separated, so a title or a path containing spaces stays one field and
//  a script can split the rows without guessing at column widths.
//
////////////////////////////////////////////////////////////////////////////////

std::string InstanceLister::Format (const std::vector<ListedInstance> & instances)
{
    std::string  output = "pid\ttitle\tmachine\tdisk1\tdisk2\n";



    for (const ListedInstance & instance : instances)
    {
        auto  field = [] (const std::string & text) { return text.empty() ? std::string ("-") : text; };
        auto  disk  = [&] (size_t index)
        {
            return (index < instance.disks.size() && instance.disks[index].has_value())
                       ? field (*instance.disks[index])
                       : std::string ("-");
        };

        output += std::format ("{}\t{}\t{}\t{}\t{}\n",
                               instance.processId, field (instance.title), field (instance.machine), disk (0), disk (1));
    }

    return output;
}
