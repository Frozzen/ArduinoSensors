#ifndef COM2MQTT_HPP
#define COM2MQTT_HPP
//#define NO_TMO


/**
 * @brief The CCom2Mqtt class объект обрабатывающий получаение данных с com
 */
class CCom2Mqtt {

    //!< статиcтика по com порту
    size_t  badCS = 0, bad_data = 0, receive_count = 0;
    std::string topic_head; //!< префикс для дерева topic на MQTT
    /**
     * @brief publish_counters отправлять на сервер статистику
     * собвтсенно отправка данных
     */
    void publish_counters() {
        std::string str = fmt::format("{d}", badCS);
        cli->publish(topic_head+"log/bad_cs", str.c_str(), str.length());
        str = fmt::format("{d}", bad_data);
        cli->publish(topic_head+"log/bad_data", str.c_str(), str.length());
        str = fmt::format("{d}", receive_count);
        cli->publish(topic_head+"log/receive_count", str.c_str(), str.length());
    }
    std::string check_valid_msg(std::string str);
    bool send_payload_mqtt(std::string str);
#ifdef NO_TMO
    std::future<void> request2send;
#endif
    bool reconnect();

public:
    std::shared_ptr<mqtt::client> cli;  //!< интерфейс mqtt

    void init();    //<! инициироваод объект
    void loop(SimpleSerial &com);

    /**
     * @brief req_send_counters послать статистику через 5 сек
     */
    void req_send_counters() {
#ifdef NO_TMO
        if(request2send.valid())
            return;
        request2send = std::async([] (CCom2Mqtt *t) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            t->publish_counters();
        }, this);
#endif
    }
};


#endif // COM2MQTT_HPP
