#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <mutex>
#include <map>
#include <boost/property_tree/ptree.hpp>

typedef boost::property_tree::basic_ptree<std::string, std::string> ptree;
typedef std::map<std::string, std::string> IniSection;

class Config {
public:
    boost::property_tree::ptree m_config;
    void open(const char* filename);
    static Config *getInstance();
    std::string m_filename;
    // key = "MQTT.server" напрмер
    std::string getOpt(const char*key) const {
        return m_config.get<std::string>(key);
    }
    IniSection getSection(const char *sect);
private:
    static std::mutex s_mtx;
    Config(const char *n) { std::cout << "Config::Config"<< n << std::endl; }
    Config &operator=(const Config &r) = delete;
    Config(const Config &r) = delete;
};

#endif // CONFIG_HPP
