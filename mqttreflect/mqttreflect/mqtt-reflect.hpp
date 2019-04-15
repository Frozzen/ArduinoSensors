#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP

class CMQTTMessage {
  public:
    std::string topic, payload;
    CMQTTMessage(std::string t, std::string p): topic{t}, payload{p} {}
};

typedef std::list<CMQTTMessage> CSendQueue;

class Handler {
    CSendQueue *queue;
   protected:
    Handler *next;
    virtual void send_msg(CMQTTMessage &msg);
  public:
    Handler(CSendQueue *q): queue{q}, next{NULL}  {}
    Handler(): queue{NULL}, next{NULL} {}
    /**
     * @brief request
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(CMQTTMessage &m) {
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
    bool request(CMQTTMessage &m);
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
    virtual void send_msg(CMQTTMessage &msg);
    bool request(CMQTTMessage &m);
    void pushNextHandler(Handler *nextInLine) = delete;
};

class DecodeEnergyHandler : public Handler {


    // Handler interface
protected:
    void send_msg(CMQTTMessage &msg);

public:
    bool request(CMQTTMessage &m);
};
#endif // MQTTREFLECT_HPP
