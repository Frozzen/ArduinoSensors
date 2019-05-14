#ifndef COM2MQTT_HPP
#define COM2MQTT_HPP
//#define NO_TMO
/**
 * @brief States for the parser state machine
 */
enum ParseState {
    PARSE_STATE_IDLE,
    PARSE_STATE_GOT_START_BYTE,
    PARSE_STATE_GOT_PAYLOAD
};

#define START_BYTE ':'
const int PAYLOAD_LEN = 255;

/**
 * @brief The CCom2Mqtt class объект обрабатывающий получаение данных с com
 */
class CCom2Mqtt {
    ParseState parse_state = PARSE_STATE_IDLE; //!< Current state of the parser state machine
    size_t payload_count = 0;   //!< счетчик в буфере
    uint8_t receive_buffer[PAYLOAD_LEN]; //!< Buffer for accumulating received payload
    /**
     * @brief send_payload_mqtt послать сообщение буфера по MQTT
     *
     * @param receive_buffer разделить topic=payload
     * @param payload_count
     * @return false если ошибка
     */
    bool send_payload_mqtt(const char *receive_buffer, size_t payload_count);

    //!< статиcтика по com порту
    size_t  badCS = 0, badData = 0, receive_count = 0;
    /**
     * @brief publish_counters отправлять на сервер статистику
     * собвтсенно отправка данных
     */
    void publish_counters() {
        std::string str = fmt::format("{d}", badCS);
        cli->publish(topic_head+"log/bad_cs", str.c_str(), str.length());
        str = fmt::format("{d}", badData);
        cli->publish(topic_head+"log/bad_date", str.c_str(), str.length());
        str = fmt::format("{d}", receive_count);
        cli->publish(topic_head+"log/receive_count", str.c_str(), str.length());
    }
    std::string topic_head; //!< префикс для дерева topic на MQTT
#ifdef NO_TMO
    std::future<void> request2send;
#endif
    bool reconnect();

public:
    CCom2Mqtt() {}
    void operator()(const uint8_t*buf, size_t len) {
        parse_bytes(buf, len);
    }
    std::shared_ptr<mqtt::client> cli;  //!< интерфейс mqtt

    void init();    //<! инициироваод объект
    /**
     * @brief loop цикл обработки
     */
    void loop() {
        while (true) {
            if(reconnect())
                break;
#ifdef NO_TMO
            if(request2send.valid())
                request2send.wait();
            else
                std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
        }
    }
    void parse_bytes(const uint8_t *buf, size_t len); //<! разбираем входной поток

    /**
     * @brief req_send_counters послать статистику через 5 сек
     *//*
    void req_send_counters() {
        if(request2send.valid())
            return;
        request2send = std::async([] (CCom2Mqtt *t) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            t->publish_counters();
        }, this);
    } */
};


#endif // COM2MQTT_HPP
