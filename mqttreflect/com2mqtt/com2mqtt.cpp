#include <chrono>
#include <stdlib.h>
#include <cstdint>
#include <cstdio>
#include <future>

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

std::shared_ptr<spdlog::logger> sysloger;
CCom2Mqtt s_mqtt;

const char START_BYTE = ':';
const char CS_BYTE = ';';
const char COM_PAYLOAD_SEPARATOR = '=';
using namespace std;
/**
 * @brief check_valid_msg проверяем правильность строки
 * @param str
 * @return
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
        ++badCS;
        req_send_counters();
        return "";
    }
    return str.substr(1, it_cs);
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

void CCom2Mqtt::init()
{
    Config *cfg = Config::getInstance();

    topic_head = cfg->getOpt("COM", "topic_head");

    cli = make_shared<mqtt::client>(cfg->getOpt("MQTT", "server"), "com2mttjkj");
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    cli->connect(connOpts);
    sysloger->info("Connecting to the MQTT server... {}", cfg->getOpt("MQTT", "server").c_str());
}



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
        sysloger->trace("com:{}", str.c_str());
        string res = check_valid_msg(str);
        if(res.length() > 0)
            send_payload_mqtt(res);
    }
}

bool CCom2Mqtt::send_payload_mqtt(string str)
{
    auto it_sep = str.find(COM_PAYLOAD_SEPARATOR);
    if(it_sep == std::string::npos)
        return false;
    // посчитать сообщение и послать статистику в mqtt
    receive_count++;
    req_send_counters();
    string topic(str, it_sep);
    string payload(str.begin()+it_sep + 1, str.end());
    cli->publish(topic_head+topic, payload.c_str(), payload.length());
    sysloger->trace(">{0}:{1}", (topic_head+topic).c_str(), payload.c_str());
    return true;
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
    cfg->open("mqttfrwd.json");

    if(result["stdout"].as<bool>())
        sysloger = spdlog::stdout_logger_mt("console");
    else
        sysloger = spdlog::syslog_logger_mt("syslog", "com2mqtt", LOG_PID);
    if(result["debug"].as<bool>())
        sysloger->set_level(spdlog::level::trace);
    else
        sysloger->set_level(spdlog::level::info);
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
