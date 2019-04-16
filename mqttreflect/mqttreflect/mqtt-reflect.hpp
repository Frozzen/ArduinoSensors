#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP
#include <list>
#include <set>
#include <memory>
#include <mqtt/message.h>
#include <boost/property_tree/ptree.hpp>

using CSendQueue = std::list<mqtt::message>;
/**
 * @brief The Handler class
 * интерфейс для переадресации выполнения
 */
class Handler {
    friend class HandlerFactory;
  protected:
    Handler() { }
  public:
    // check payload - for sensible - our case is alphanum
    void send_msg(mqtt::message msg);
    /**
     * @brief request
     * отправляем дальше сообщение на обработку
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(mqtt::message m) = 0;
};

/**
 * @brief The DomotizcHandler class
 * конвертируем простое значение в json
 * TODO не учитываем регистр topic
 */
class DomotizcHandler : public Handler {
    friend class HandlerFactory;
protected:
public:
    std::map<std::string, int> domotizc;
    bool request(mqtt::message m);
};

/**
 * @brief The ReflectHandler class
 * разворачиваем цепочки переадресации, может быть несколько адресатов верхний обработчик
 * TODO не учитываем регистр topic
 */
class ReflectHandler : public Handler {
    friend class HandlerFactory;
protected:
public:
    std::map<std::string, std::string> reflect;
    bool request(mqtt::message m);
};

/**
 * @brief The DecodeEnergyHandler class
 * декрлируем JSON поля в значение подключи. определяем в ini файле какие ключи смотрим на JSON
 */
class DecodeJsonHandler : public Handler {
    void recursive_dump_json(int level, boost::property_tree::ptree &pt, const std::string &from);
protected:
public:
    std::set<std::string> valid_case;
    bool request(mqtt::message m);
    friend class HandlerFactory;
};

class HandlerFactory {
    static void set_config(Config *c, std::shared_ptr<DecodeJsonHandler> h);
    static void set_config(Config *c, std::shared_ptr<ReflectHandler> h);
    static void set_config(Config *c, std::shared_ptr<DomotizcHandler> h);

    static std::list<std::shared_ptr<Handler>> handler_list;
public:
    static CSendQueue mqtt_mag_queue;
    static void makeAll(Config *cfg);
    /**
     * @brief request_again
     * послать сообщение в цепь обработчиков снова
     * @param m
     * @return
     */
    static bool request(mqtt::message m) {
        for(auto &h : handler_list)
            h->request(m);
        return true;
    }
};


#endif // MQTTREFLECT_HPP
