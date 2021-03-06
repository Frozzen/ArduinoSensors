#include <chrono>
#include <stdlib.h>
#include <cstdint>
#include <cstdio>
#include <thread>

#include <chrono>
#include <thread>
#include <config.hpp>
#include <thread>
#include <mqtt/client.h>
#include <cstring>

#include "spdlog/fmt/ostr.h" // must be included
#include <spdlog/spdlog.h>
#include <spdlog/sinks/syslog_sink.h>
#include "spdlog/sinks/stdout_sinks.h"

#include <cxxopts.hpp>
#include "SimpleSerial.h"
#include "config.hpp"
#include "com2mqtt.hpp"

#define NUM_BYTES 64
const char *LWT = "tele/com2mqtt/LWT";
std::shared_ptr<spdlog::logger> sysloger;
CCom2Mqtt s_mqtt;

const char START_BYTE = ':';
const char CS_BYTE = ';';
const char COM_PAYLOAD_SEPARATOR = '=';
using namespace std;

/**
 * @brief check_valid_msg проверяем правильность строки
 * @param str полная входная строка
 * @return пустая строка если ошибка, или чистая строка без обрамления
 */
string CCom2Mqtt::check_valid_msg(string str)
{
    if(str[0] != START_BYTE) {
        ++bad_data;
        req_send_counters();
        return "";
    }
    char *e;
    auto it_cs = str.find(CS_BYTE);
    if(it_cs == std::string::npos) {
        ++bad_data;
        req_send_counters();
        return "";
    }
    string ss= str.substr(it_cs+1);
    int csorg = strtol(ss.c_str(), &e, 16);
    uint8_t cs = 0;
    for (auto it = str.begin(); it < str.end()-3; ++it) {
        cs += (int8_t)*it;
    }    
    cs = 0xff - cs;
    if (cs != csorg) {
        sysloger->warn("bad data checksum");
        ++bad_cs;
        req_send_counters();
        return "";
    }
    return str.substr(3, it_cs);
}

bool try_reconnect(mqtt::client &cli) {
    constexpr int N_ATTEMPT = 30;

    for (int i = 0; i < N_ATTEMPT && !cli.is_connected(); ++i) {
        try {
            cli.reconnect();
            return true;
        }
        catch (const mqtt::exception &) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
    return false;
}

/**
 * @brief CCom2Mqtt::send_payload_mqtt послать сообщение в mqtt
 *
 * если неправильное сообщение - не шлем
 * @param str
 * @return
 */
bool CCom2Mqtt::send_payload_mqtt(string str)
{
    auto it_sep = str.find(COM_PAYLOAD_SEPARATOR);
    if(it_sep == std::string::npos)
        return false;
    // посчитать сообщение и послать статистику в mqtt
    receive_count++;
    req_send_counters();
    string topic(str, 0, it_sep);
    string payload(str, it_sep + 1, str.length()-2-it_sep);
    cli->publish(topic_head+topic, payload.c_str(), payload.length(), 1, false);
    sysloger->trace(">{}:{}", (topic_head+topic).c_str(), payload.c_str());
    return true;
}


/**
 * @brief CCom2Mqtt::init инициирую пакет
 */
void CCom2Mqtt::init()
{
    Config *cfg = Config::getInstance();

    topic_head = cfg->getOpt("COM", "topic_head");

    cli = make_shared<mqtt::client>(cfg->getOpt("MQTT", "server"), "com2mttjkj");
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    mqtt::message willmsg(topic_head + LWT, "Offline", 1, RETAIN);
    mqtt::will_options will(willmsg);
    connOpts.set_will(will);

    cli->connect(connOpts);
    sysloger->info("Connecting to the MQTT server... {}", cfg->getOpt("MQTT", "server").c_str());
    mqtt::message msg(topic_head + LWT, "Online", 1, RETAIN);
    cli->publish(msg);
}

template<typename T>
bool future_is_ready(std::future<T>& t){
    return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
/**
 * @brief req_send_counters послать статистику через 5 сек
 */
void CCom2Mqtt::req_send_counters() {
    if((request2send.valid() && !future_is_ready(request2send)))
        return;
    request2send = std::async(std::launch::async, [] (CCom2Mqtt *t) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        t->publish_counters();
    }, this);
}

/**
 * @brief publish_counters отправлять на сервер статистику
 * собвтсенно отправка данных
 */
void CCom2Mqtt::publish_counters() {
    string str = fmt::format("{d}", bad_cs);
    cli->publish(topic_head+"log/bad_cs", str.c_str(), str.length(), 1, RETAIN);
    str = fmt::format("{d}", bad_data);
    cli->publish(topic_head+"log/bad_data", str.c_str(), str.length(), 1, RETAIN);
    str = fmt::format("{d}", receive_count);
    cli->publish(topic_head+"log/receive_count", str.c_str(), str.length(), 1, RETAIN);
    sysloger->debug("pub log: cs:{} bad:{} rcv:{}", bad_cs, bad_data, receive_count);
}

/**
 * @brief CCom2Mqtt::loop главный цикл обработки сообщений
 *
 * выходит при неисправляемых ошибках
 * @param com
 */
void CCom2Mqtt::loop(SimpleSerial &com){
    // wait for all bytes to be received
    while (true) {
        if (!cli->is_connected()) {
            sysloger->error("Lost connection. Attempting reconnect");
            if (try_reconnect(*cli)) {
                sysloger->warn("Reconnected");
                continue;
            } else {
                sysloger->critical("Reconnect failed.");
                break;
            }
        }
        string str = com.readLine();
        sysloger->trace("{}", str.c_str());
        string res = check_valid_msg(str);
        if(res.length() > 0)
            send_payload_mqtt(res);
    }
}
/**
 * @brief exec выполнить команду системы
// https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-output-of-command-within-c-using-posix
 * @param cmd
 * @return
 */
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

//----------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    cxxopts::Options options("com2mqtt", "reflect events from com poer to MQTT bus");
    string port{"/dev/ttyUSB0"};
    int baud = 38400;
    options.add_options()
      ("s,stdout", "Log to stdout")
      ("d,debug", "Enable debugging")
      ("p,port", "com port", cxxopts::value<std::string>(port))
      ("b,baud", "com port speed", cxxopts::value(baud));
    auto result = options.parse(argc, argv);
    // initialize

    Config *cfg = Config::getInstance();
    cfg->open("com2mqtt.json");

    if(result["stdout"].as<bool>())
        sysloger = spdlog::stdout_logger_mt("console");
    else
        sysloger = spdlog::syslog_logger_mt("syslog", "com2mqtt", LOG_PID);
    if(result["debug"].as<bool>())
        sysloger->set_level(spdlog::level::trace);
    else
        sysloger->set_level(spdlog::level::info);
    // полать RESETUSB ---------------------
    exec("python reset_usb.py search UART");
    try {
        // open serial port
        SimpleSerial serial(port, baud);
        sysloger->info("use port:{0}", port);

        s_mqtt.init();
        s_mqtt.loop(serial);
    } catch(boost::system::system_error& e)
    {
        sysloger->critical("Serial Error: {}", e.what());
    }
    return 0;
}
