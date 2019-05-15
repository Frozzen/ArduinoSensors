/**

  */
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <mutex>

#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/filereadstream.h>
#include "config.hpp"
#include <stdexcept>

using namespace std;
using namespace rapidjson;

static std::mutex s_mtx;
Document s_document;

/**
 * key = "MQTT.server" напрмер
 */
std::string Config::getOpt(const char *sect, const char *key) const {
    auto l1 = s_document[sect].GetObject();
    return l1[key].GetString();
}



Config *Config::getInstance()
{
    std::lock_guard<std::mutex> lock(s_mtx);
    static Config s_cfg("");
    return &s_cfg;
}

/**
 * @brief Config::open
 * загрузить конфигурацию open("mqttfrwd.ini");
 * @param ini_file
 */
void Config::open(const char* ini_file)
{
    std::lock_guard<std::mutex> lock(s_mtx);
    FILE *fp = fopen(ini_file, "rb"); // non-Windows use "r"
    if (fp == NULL)
        throw std::runtime_error(string("no file") + ini_file);
    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    s_document.ParseStream(is);
    //  throw std::runtime_error( string("json error:") + ini_file );
}

/**
 * @brief Config::getSection
 * загрузить конфигурацию getSection("reflect");
 * @param sect
 * @return
 */
std::unique_ptr<IniSection> Config::getSection(const char *sect)
{
    std::map<std::string, std::string> res;
    auto section = s_document[sect].GetObject();
    auto iterator = section.MemberBegin();
    if (iterator == section.MemberEnd()) {
        std::stringstream error;
        error << "ERROR: \"" << sect << "\" missing from config file!";
        throw std::runtime_error(error.str());
    }

    for (auto p = section.MemberBegin(); p != section.MemberEnd(); ++p)
        res[p->name.GetString()] = p->value.GetString();
    return make_unique<IniSection>(res);
}

