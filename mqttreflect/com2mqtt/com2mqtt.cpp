#include <chrono>
#include <async_comm/serial.h>
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

#include "config.hpp"
#include "com2mqtt.hpp"

#define NUM_BYTES 64

using namespace std;

std::shared_ptr<spdlog::logger> sysloger;
shared_ptr<CCom2Mqtt> s_mqtt;


void CCom2Mqtt::init() {
    Config *cfg = Config::getInstance();

    topic_head = cfg->getOpt("COM", "topic_head");
    cli = make_shared<mqtt::client>(cfg->getOpt("MQTT", "server"), "com2mttjkj");
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    sysloger->info("Connecting to the MQTT server... {0}", cfg->getOpt("MQTT", "server").c_str());
    this_thread::sleep_for(chrono::seconds(1));
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
 * @brief CCom2Mqtt::reconnect проверить подключение
 * @return true если подключение не удалось
 */
bool CCom2Mqtt::reconnect() {
    /*auto msg = cli->consume_message();
    // восстанавливаем соединение
    if (!msg) {*/
        if (!cli->is_connected()) {
            sysloger->warn("Lost connection. Attempting reconnect");
            if (try_reconnect(*cli)) {
                sysloger->info("Reconnected");
            } else {
                sysloger->critical("Reconnect failed.");
                return true;
            }
        }/*
    }*/
    // TODO возможно обработать полученное сообщение
    return false;
}

bool check_cs(const char *receive_buffer, size_t payload_count)
{
    const char *ix_start = std::strchr(receive_buffer, ':');
    if(ix_start == NULL)
        return false;
    int cs = 0;
    char *e;
    int csorg = strtol(receive_buffer + payload_count - 2, &e, 16);
    for (size_t it = ix_start - (char *) receive_buffer; it < payload_count - 4; ++it)
        cs += receive_buffer[it];
    if ((cs & 0xff) != csorg) {
        return false;
    }
    return true;
}

void CCom2Mqtt::parse_bytes(const uint8_t *buf, size_t len) {
    for (size_t ix = 0; ix < len; ++ix) {
        uint8_t byte = buf[ix];
        receive_buffer[payload_count] = byte;
        switch (parse_state) {
            case PARSE_STATE_IDLE:
                if (byte == START_BYTE) {
                    payload_count = 0;
                    // crc = 0; crc = update_crc(crc, byte);
                    parse_state = PARSE_STATE_GOT_START_BYTE;
                }
                break;
            case PARSE_STATE_GOT_START_BYTE:
                if (++payload_count >= PAYLOAD_LEN) {
                    parse_state = PARSE_STATE_IDLE;
                    ++badData;
                    sysloger->warn("bad data len:{0}", payload_count);
#ifdef NO_TMO
                    req_send_counters();
#endif
                } else if (byte == '\n') {
                    receive_buffer[payload_count] = '\0';
                    // send message tp MQTT
                    if(check_cs((const char*)receive_buffer, payload_count))
                        send_payload_mqtt((const char*)receive_buffer, payload_count);
                    else {
                        // https://en.cppreference.com/w/cpp/thread/future/wait_for
                        // TODO start timeeout to send to Server
                        sysloger->warn("bad data checksum");
                        ++badCS;
#ifdef NO_TMO
                        req_send_counters();
#endif
                    }
                    parse_state = PARSE_STATE_IDLE;
                } else {
                    receive_buffer[payload_count] = byte;
                    // crc = update_crc(crc, byte);
                }
            break;
        }
    }
}

bool CCom2Mqtt::send_payload_mqtt(const char *receive_buffer, size_t payload_count) {
    const char *ix_sep = strchr(receive_buffer, '=');
    if(ix_sep == NULL)
        return false;
    // посчитать сообщение и послать статистику в mqtt
    string topic(receive_buffer, ix_sep);
    string payload(ix_sep + 1, ix_sep + 1);
    cli->publish(topic_head+topic, payload.c_str(), payload.length());

    sysloger->trace(">{0}:{1}", (topic_head+topic).c_str(), payload.c_str());
    receive_count++;
#ifdef NO_TMO
    req_send_counters();
#endif
    return true;
}

//----------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    string port{"/dev/ttyUSB0"};
    int baud = 38400;
    cxxopts::Options options("com2mqtt", "reflect events from com poer to MQTT bus");
    options.add_options()
      ("s,stdout", "Log to stdout")
      ("d,debug", "Enable debugging")
      ("p,port", "com port", cxxopts::value<std::string>(port)),
      ("b,baud", "com port speed", cxxopts::value<int>(baud));
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

    // open serial port
    async_comm::Serial serial(port, baud);
    if (!serial.init()) {
        std::printf("Failed to initialize serial port %s\n", port.c_str());
        return 2;
    }
    s_mqtt = make_shared<CCom2Mqtt>();
    sysloger->info("use port:{0}", port);
    serial.register_receive_callback([](const uint8_t* buf, size_t len) {
        s_mqtt->parse_bytes(buf, len);
    });

    s_mqtt->init();
    s_mqtt->loop();
    // close serial port
    serial.close();
    return 0;
}
