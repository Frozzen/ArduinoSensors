#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP
#include <list>
#include <mqtt/message.h>

typedef std::list<mqtt::message> CSendQueue;

class Handler {
    CSendQueue *queue;
   protected:
    Handler *next;
    virtual void send_msg(mqtt::message & msg);
  public:
    Handler(CSendQueue *q): queue{q}, next{NULL}  {}
    Handler(): queue{NULL}, next{NULL} {}
    /**
     * @brief request
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(mqtt::message & m) {
        if(next == NULL)
            return false;
        else
            return next->request(m);
    }
    void pushNextHandler(Handler *nextInLine) {
                next = nextInLine;
            }
};

/**
 * @brief The DomotizcHandler class
 * конверт простое значение в json
 */
class DomotizcHandler : public Handler {
    std::map<std::string, int> domotizc;
public:
    void set_reflect(Config *c);
    bool request(mqtt::message &m);
    void pushNextHandler(Handler *nextInLine) = delete;
};

/**
 * @brief The ReflectHandler class
 * разворачиваем цепочки переадресации, может быть несколько адресатов
 */
class ReflectHandler : public Handler {
    std::map<std::string, std::string> reflect;
public:
    void set_reflect(Config *c);
    virtual void send_msg(mqtt::message &msg);
    bool request(mqtt::message &m);
    void pushNextHandler(Handler *nextInLine) = delete;
};

class DecodeEnergyHandler : public Handler {


    // Handler interface
protected:
    void send_msg(mqtt::message &msg);

public:
    bool request(mqtt::message &m);
};
#endif // MQTTREFLECT_HPP
