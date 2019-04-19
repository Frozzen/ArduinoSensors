// JSON simple example
// This example does not handle errors.

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <iostream>
using namespace std;
using namespace rapidjson;

static const char* kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };
void parseRecursive(std::string scope, rapidjson::Value::ConstMemberIterator object)
{
    if (scope.empty())
    {
        scope = object->name.GetString();
    }
    else
    {
        scope = scope + "::" + object->name.GetString();
        //cout << scope << endl;
    }
    auto inElement = scope + "::";

    if (object->value.IsObject())
    {
        for (auto it = object->value.MemberBegin(); it != object->value.MemberEnd(); ++it)
        {
            parseRecursive(scope, it);
        }
    }
    else if (object->value.IsDouble())
    {
        cout << inElement <<  std::to_string(object->value.GetDouble()) << endl;
    }
    else if (object->value.IsInt())
    {
        cout << inElement << std::to_string(object->value.GetInt())  << endl;
    }
    else if (object->value.IsString())
    {
        cout << inElement << object->value.GetString()  << endl;
    }
    else
    {
        cout << "Unsuported: " << inElement << object->name.GetString();
    }
}
void traverse(Document *document)
{
    printf("Content of JSON documnt:\n");
    for (auto it = document->MemberBegin(); it != document->MemberEnd(); ++it)
    {
        printf("Type of member %s is %s\n", it->name.GetString(), kTypeNames[it->value.GetType()]);
        parseRecursive("", it);
    }
}
int main() {
    // 1. Parse a JSON string into DOM.
    const char* json = R"({"Time":"2019-04-14T15:54:22","ENERGY":{"TotalStartTime":"2019-02-06T18:52:41","Total":14.185,"Yesterday":0.000,"Today":0.001,"Power":0,"ApparentPower":0,"ReactivePower":0,"Factor":0.00,"Voltage":213,"Current":0.000}})";

    Document d;
    d.Parse(json);
    traverse(&d);
    /*
    // 2. Modify it by DOM.
    Value& s = d["stars"];
    s.SetInt(s.GetInt() + 1);
*/
    // 3. Stringify the DOM
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    // Output {"project":"rapidjson","stars":11}
    std::cout << buffer.GetString() << std::endl;
    return 0;
}
