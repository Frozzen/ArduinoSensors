/**

  */
#include <iostream>
#include <boost/range/algorithm/find.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "config.hpp"

using namespace std;
using namespace boost;


std::mutex Config::s_mtx;

Config *Config::getInstance()
{
    std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
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
    cout << " Config::open" << endl;
    std::lock_guard<std::mutex> lock(s_mtx);
    property_tree::ini_parser::read_ini(ini_file, m_config);
    m_filename = ini_file;
    /*cout << config.get<std::string>("MQTT.server") << endl;
*/
}

/**
 * @brief Config::getSection
 * загрузить конфигурацию getSection("reflect");
 * @param sect
 * @return
 */
IniSection    Config::getSection(const char *sect)
{
    std::map<std::string, std::string> res;

    for (auto p : m_config.get_child(sect))
        res[p.first] = p.second.data();
    return res;
}

