#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <map>
#include <memory>
#include <chrono>

typedef std::map<std::string, std::string> IniSection;
const int RETAIN = true;

class Config {
public:
    void open(const char* filename);
    static Config *getInstance();
    std::string m_filename;
    // sect = "MQTT", key = "server" напрмер
    std::string getOpt(const char *sect, const char *key) const;

    std::unique_ptr<IniSection> getSection(const char *sect);

    Config &operator=(const Config &r) = delete;
    Config(const Config &r) = delete;

private:

    explicit Config(const char *n) {}
};

inline std::string getTimeStr(){
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string s(30, '\0');
    std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return s;
}

#endif // CONFIG_HPP
