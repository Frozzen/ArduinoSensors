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

#define NUM_BYTES 64

std::shared_ptr<spdlog::logger> sysloger;

/**
 * @brief States for the parser state machine
 */
enum ParseState {
    PARSE_STATE_IDLE,
    PARSE_STATE_GOT_START_BYTE,
    PARSE_STATE_GOT_PAYLOAD
};

#define START_BYTE ':'

#define START_BYTE_LEN 1
#define CRC_LEN 1
#define PACKET_LEN (START_BYTE_LEN + PAYLOAD_LEN + CRC_LEN)
using namespace std;
const int PAYLOAD_LEN = 255;

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

template <typename Clock = std::chrono::steady_clock>
class timeout
{
public:
    typedef Clock clock_type;
    typedef typename clock_type::time_point time_point;
    typedef typename clock_type::duration duration;

    explicit timeout(duration maxDuration) :    mStartTime(clock_type::now()),    mMaxDuration(maxDuration)
    {}

    time_point start_time() const
    {
        return mStartTime;
    }

    duration max_duration() const
    {
        return mMaxDuration;
    }

    bool is_expired() const
    {
        const auto endTime = clock_type::now();
        return (endTime - start_time()) > max_duration();
    }

    static timeout infinity()
    {
        return timeout(duration::max());
    }

private:
    time_point mStartTime;
    duration mMaxDuration;
};


class CCom2Mqtt {
    ParseState parse_state = PARSE_STATE_IDLE; //!< Current state of the parser state machine
    uint8_t receive_buffer[PAYLOAD_LEN]; //!< Buffer for accumulating received payload

    bool send_payload_mqtt(const char *receive_buffer, size_t payload_count);
    // TODO отправлять на сервер
    size_t  badCS = 0, badData = 0, receive_count = 0;

    size_t payload_count = 0;
    uint8_t crc;
    void publish_counters() {
        string str = fmt::format("{d}", badCS);
        cli->publish("log/bad_cs", str.c_str(), str.length());
        str = fmt::format("{d}", badData);
        cli->publish("log/bad_date", str.c_str(), str.length());
        str = fmt::format("{d}", receive_count);
        cli->publish("log/receive_count", str.c_str(), str.length());
    }
    std::string topic_head;

public:

    std::future<void> rsend_counters;
    std::shared_ptr<mqtt::client> cli;

    void init();

    void parse_bytes(const uint8_t *buf, size_t len);

    bool reconnect() {
        auto msg = cli->consume_message();
        // восстанавливаем соединение
        if (!msg) {
            if (!cli->is_connected()) {
                sysloger->warn("Lost connection. Attempting reconnect");
                if (try_reconnect(*cli)) {
                    sysloger->info("Reconnected");
                } else {
                    sysloger->critical("Reconnect failed.");
                    return true;
                }
            }
        }
        return false;
    }

    void req_send_counters() {
        if(rsend_counters.valid())
            return;
        rsend_counters = std::async([] (timeout<> timelimit, CCom2Mqtt *t) {
                while(true) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (timelimit.is_expired()) {
                        t->publish_counters();
                        break;
                        }
                    }
            }, timeout<>(std::chrono::milliseconds(500)), this);
    }
};

void CCom2Mqtt::init() {
    Config *cfg = Config::getInstance();

    topic_head = cfg->getOpt("COM", "topic_head");

    cli = make_shared<mqtt::client>(cfg->getOpt("MQTT", "server"), "com2mttjkj");
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    sysloger->info("Connecting to the MQTT server... {0}", cfg->getOpt("MQTT", "server").c_str());
}

CCom2Mqtt s_mqtt;


/**
 * @brief Recursively update the cyclic redundancy check (CRC)
 * обычно стартует от 0
 *
 * This uses the CRC-8-CCITT polynomial.
 *
 * Source: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html#gab27eaaef6d7fd096bd7d57bf3f9ba083
 *
 * @param inCrc The current CRC value. This should be initialized to 0 before processing first byte.
 * @param inData The byte being processed
 * @return The new CRC value
 */
uint8_t update_crc(uint8_t inCrc, uint8_t inData) {
    uint8_t i;
    uint8_t data;

    data = inCrc ^ inData;

    for (i = 0; i < 8; i++) {
        if ((data & 0x80) != 0) {
            data <<= 1;
            data ^= 0x07;
        } else {
            data <<= 1;
        }
    }
    return data;
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
                    req_send_counters();
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
                        req_send_counters();
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
    Config *cfg = Config::getInstance();

    const char *ix_sep = strchr(receive_buffer, '=');
    if(ix_sep == NULL)
        return false;
    // посчитать сообщение и послать статистику в mqtt
    string head = cfg->getOpt("COM", "topic_head");
    receive_count++;
    req_send_counters();
    string topic(receive_buffer, ix_sep);
    string payload(ix_sep + 1, ix_sep + 1);
    cli->publish(head+topic, payload.c_str(), payload.length());
    sysloger->trace(">{0}:{1}", (head+topic).c_str(), payload.c_str());
    return true;
}

/**
 * @brief Callback function for the async_comm library
 *
 * Prints the received bytes to stdout.
 *
 * @param buf Received bytes buffer
 * @param len Number of bytes received
 */
void callback(const uint8_t *buf, size_t len) {
    s_mqtt.parse_bytes(buf, len);
}


//----------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    cxxopts::Options options("com2mqtt", "reflect events from com poer to MQTT bus");
    options.add_options()
      ("s,stdout", "Log to stdout")
      ("d,debug", "Enable debugging")
      ("p,port", "com port", cxxopts::value<std::string>(), "/dev/ttyUSB0"),
      ("b,baud", "com port speed", cxxopts::value<int>(), 38400);
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
    async_comm::Serial serial(result["port"].as<std::string>(),
            result["baud"].as<int>());
    if (!serial.init()) {
        std::printf("Failed to initialize serial port\n");
        return 2;
    }
    sysloger->info("use port:{0}", result["port"].as<std::string>());
    serial.register_receive_callback(&callback);

    s_mqtt.init();
    // wait for all bytes to be received
    while (true) {
        if(s_mqtt.reconnect())
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        s_mqtt.rsend_counters.wait();
    }
    // close serial port
    serial.close();
    return 0;
}
