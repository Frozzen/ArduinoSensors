#ifndef COM2MQTT_HPP
#define COM2MQTT_HPP
#define NO_TMO


/**
 * @brief The CCom2Mqtt class объект обрабатывающий получаение данных с com
 */
class CCom2Mqtt {

    //!< статиcтика по com порту
    size_t  bad_cs = 0, bad_data = 0, receive_count = 0;
    std::string topic_head; //!< префикс для дерева topic на MQTT
    /**
     * @brief publish_counters отправлять на сервер статистику
     * собвтсенно отправка данных
     */
    void publish_counters();
    std::string check_valid_msg(std::string str);
    bool send_payload_mqtt(std::string str);
    std::future<void> request2send;
    bool reconnect();

public:
    std::shared_ptr<mqtt::client> cli;  //!< интерфейс mqtt

    void init();    //<! инициироваод объект
    void loop(SimpleSerial &com);

    /**
     * @brief req_send_counters послать статистику через 5 сек
     */
    void req_send_counters();
};


#endif // COM2MQTT_HPP
