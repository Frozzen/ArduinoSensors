#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <map>
#include <memory>

typedef std::map<std::string, std::string> IniSection;

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

#endif // CONFIG_HPP
