#include <chrono>
#include <async_comm/serial.h>
#include <stdlib.h>
#include <cstdint>
#include <cstdio>

#include <chrono>
#include <thread>
#include <config.hpp>
#include <thread>
#include <mqtt/client.h>
#include <cstring>
#include "config.hpp"

#define NUM_BYTES 64
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

class CCom2Mqtt {
    ParseState parse_state = PARSE_STATE_IDLE; //!< Current state of the parser state machine
    uint8_t receive_buffer[PAYLOAD_LEN]; //!< Buffer for accumulating received payload
    std::mutex mutex; //!< mutex for synchronization between the main thread and callback thread
    std::condition_variable condition_variable; //!< condition variable used to suspend main thread until all messages have been received back

    bool send_payload_mqtt(const char *receive_buffer, size_t payload_count);
    // TODO отправлять на сервер
    size_t  badCS = 0, badData = 0, receive_count = 0;

    bool check_cs(const char *receive_buffer, size_t payload_count);

public:
    std::shared_ptr<mqtt::client> cli;
    std::string topic_head;

    void init();

    void parse_bytes(const uint8_t *buf, size_t len);

    void reconnect() {
        auto msg = cli->consume_message();
        // восстанавливаем соединение
        if (!msg) {
            if (!cli->is_connected()) {
                //cout << "Lost connection. Attempting reconnect" << endl;
                if (try_reconnect(*cli)) {
                    //cout << "Reconnected" << endl;
                } else {
                    //cout << "Reconnect failed." << endl;
                }
            }
        }
    }
};

void CCom2Mqtt::init() {
    Config *cfg = Config::getInstance();

    topic_head = cfg->getOpt("COM", "topic_head");

    // TODO create uniq
    const string CLIENT_ID{"com_sync_consume_cpp"};

    cli = make_shared<mqtt::client>(cfg->getOpt("MQTT", "server"), CLIENT_ID);
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
};

CCom2Mqtt s_mqtt;


/**
 * @brief Recursively update the cyclic redundancy check (CRC)
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

bool CCom2Mqtt::check_cs(const char *receive_buffer, size_t payload_count)
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
        // https://en.cppreference.com/w/cpp/thread/future/wait_for
        // TODO start timeeout to send to Server
        ++badCS;
        return false;
    }
    return true;
}

void CCom2Mqtt::parse_bytes(const uint8_t *buf, size_t len) {
    static size_t payload_count = 0;
    static uint8_t crc;
    for (size_t ix = 0; ix < len; ++ix) {
        uint8_t byte = buf[ix];
        receive_buffer[payload_count] = byte;
        switch (parse_state) {
            case PARSE_STATE_IDLE:
                if (byte == START_BYTE) {
                    payload_count = 0;
                    crc = 0;
                    // crc = update_crc(crc, byte);
                    parse_state = PARSE_STATE_GOT_START_BYTE;
                }
                break;
            case PARSE_STATE_GOT_START_BYTE:
                if (++payload_count >= PAYLOAD_LEN) {
                    parse_state = PARSE_STATE_IDLE;
                    // TODO start timeout to send to Server
                    ++badData;
                } else if (byte == '\n') {
                    receive_buffer[payload_count] = '\0';
                    // send message tp MQTT
                    if(check_cs((const char*)receive_buffer, payload_count))
                        send_payload_mqtt((const char*)receive_buffer, payload_count);
                    // condition_variable.notify_one();
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
    receive_count++;
    std::string topic(receive_buffer, ix_sep);
    std::string payload(ix_sep + 1, ix_sep + 1);
    cli->publish(topic, payload.c_str(), payload.length());
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
    // initialize
    char *port;
    if (argc < 2) {
        std::printf("USAGE: %s PORT\n", argv[0]);
        return 1;
    } else {
        std::printf("Using port %s\n", argv[1]);
        port = argv[1];
    }

    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.json");

    // open serial port
    async_comm::Serial serial(port, 38400);
    serial.register_receive_callback(&callback);

    if (!serial.init()) {
        std::printf("Failed to initialize serial port\n");
        return 2;
    }
    s_mqtt.init();
    /*
    uint8_t buffer[NUM_BYTES];

    // test sending bytes one at a time
    std::printf("Transmit individual bytes:\n");
    for (uint8_t i = 0; i < NUM_BYTES; i++) {
        buffer[i] = i;
        serial.send_byte(i);
    }
    */
    // wait for all bytes to be received
    while (true) {
        s_mqtt.reconnect();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    // close serial port
    serial.close();

    return 0;
}
