#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP
#include <list>
#include <mqtt/message.h>

typedef std::list<mqtt::message> CSendQueue;
/**
 * @brief The Handler class
 * интерфейс для переадресации выполнения
 */
class Handler {
   static CSendQueue queue;
   protected:
   static Handler *top_handler;    // значит верхний обработчик
    Handler *next;
    // TODO check payload - for sensible - our case is alphanum
    virtual void send_msg(mqtt::message & msg);
  public:
    Handler(): next{NULL} {
        top_handler = NULL;
    }
    Handler(Handler *nextInLine): next{nextInLine} {
        top_handler = nextInLine;
    }
    /**
     * @brief request
     * отправляем дальше сообщение на обработку
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(mqtt::message & m) {
        if(next == NULL)
            return false;
        else
            return next->request(m);
    }
    virtual void set_config(Config *) {}
};

/**
 * @brief The DomotizcHandler class
 * конвертируем простое значение в json
 * TODO не учитываем регистр topic
 */
class DomotizcHandler : public Handler {
    std::map<std::string, int> domotizc;
public:
    explicit DomotizcHandler(Handler *h) : Handler(h) {}
    void set_config(Config *c);
    bool request(mqtt::message &m);
};

/**
 * @brief The ReflectHandler class
 * разворачиваем цепочки переадресации, может быть несколько адресатов верхний обработчик
 * TODO не учитываем регистр topic
 */
class ReflectHandler : public Handler {
    std::map<std::string, std::string> reflect;
public:
    explicit ReflectHandler(Handler *h) : Handler(h) {}
    void set_config(Config *c);
    bool request(mqtt::message &m);
};

/**
 * @brief The DecodeEnergyHandler class
 * декрлируем JSON поля в значение подключи. определяем в ini файле какие ключи смотрим на JSON
 */
class DecodeEnergyHandler : public Handler {
    std::vector<std:string> valid_case;
public:
    explicit DecodeEnergyHandler(Handler *h) : Handler(h) {}
    bool request(mqtt::message &m);
    void set_config(Config *c);
};
#endif // MQTTREFLECT_HPP
