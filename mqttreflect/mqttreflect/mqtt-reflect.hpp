#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP
#include <list>
#include <set>
#include <memory>
#include <mqtt/message.h>
#include <boost/property_tree/ptree.hpp>

using CSendQueue = std::list<std::shared_ptr<mqtt::message>>;
/**
 * @brief The Handler class
 * интерфейс для переадресации выполнения
 */
class Handler {
    friend class HandlerFactory;
  protected:
    Handler(std::shared_ptr<Handler> nextInLine) { nextInLine->next = next;  next = nextInLine; }
  public:
    Handler(): next{NULL} {    }
    static CSendQueue queue;
    std::shared_ptr<Handler> next;
    // TODO check payload - for sensible - our case is alphanum
    void send_msg(mqtt::message_ptr msg);
    /**
     * @brief request
     * отправляем дальше сообщение на обработку
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(mqtt::message_ptr m) {
        if(next == NULL)
            return false;
        else
            return next->request(m);
    }
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
    explicit DomotizcHandler(std::shared_ptr<Handler> h) : Handler(h) {}
    std::map<std::string, int> domotizc;
    bool request(mqtt::message_ptr m);
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
    explicit ReflectHandler(std::shared_ptr<Handler> h) : Handler(h) {}
    bool request(mqtt::message_ptr m);
};

/**
 * @brief The DecodeEnergyHandler class
 * декрлируем JSON поля в значение подключи. определяем в ini файле какие ключи смотрим на JSON
 */
class DecodeJsonHandler : public Handler {
    void recursive_dump_json(int level, boost::property_tree::ptree &pt, const std::string &from);
protected:
public:
    explicit DecodeJsonHandler(std::shared_ptr<Handler> h) : Handler(h) {}
    std::set<std::string> valid_case;
    bool request(mqtt::message_ptr m);
    friend class HandlerFactory;
};

class HandlerFactory {
    static std::shared_ptr<Handler> top_handler;
    static void set_config(Config *c, std::shared_ptr<DecodeJsonHandler> h);
    static void set_config(Config *c, std::shared_ptr<ReflectHandler> h);
    static void set_config(Config *c, std::shared_ptr<DomotizcHandler> h);
public:
    static std::shared_ptr<Handler> makeAll(Config *cfg);
    /**
     * @brief request_again
     * послать сообщение в цепь обработчиков снова
     * @param m
     * @return
     */
    static bool request_again(mqtt::message_ptr m) {
        return top_handler->request(m);
    }
};


#endif // MQTTREFLECT_HPP
