﻿#ifndef MQTTREFLECT_HPP
#define MQTTREFLECT_HPP
#include <list>
#include <set>
#include <memory>
#include <mqtt/message.h>

struct SendMessge {
    std::string topic, payload;

    SendMessge(std::string t, std::string p) : topic{t}, payload{p} {}
};

using CSendQueue = std::list<SendMessge>;
/**
 * @brief The Handler class
 * интерфейс для переадресации выполнения
 */
class Handler {
    friend class HandlerFactory;
  protected:
    Handler() = default;
  public:
    // check payload - for sensible - our case is alphanum
    void send_msg(const SendMessge &msg);
    /**
     * @brief request
     * отправляем дальше сообщение на обработку
     * @param m
     * @return false if not pass message to chain
     */
    virtual bool request(const SendMessge &m) = 0;
};

/**
 * @brief The DomotizcHandler class
 * конвертируем простое значение в json
 * не учитываем регистр topic
 */
class DomotizcHandler : public Handler {
    friend class HandlerFactory;
protected:
public:
    std::unordered_map<std::string, int> domotizc;

    bool request(const SendMessge &m) override;
};

/**
 * @brief The ReflectHandler class
 * разворачиваем цепочки переадресации, может быть несколько адресатов верхний обработчик
 * не учитываем регистр topic
 */
class ReflectHandler : public Handler {
    friend class HandlerFactory;
protected:
public:
    std::unordered_map<std::string, std::string> reflect;

    bool request(const SendMessge &m) override;
};

/**
 * @brief The DecodeEnergyHandler class
 * декрлируем JSON поля в значение подключи. определяем в ini файле какие ключи смотрим на JSON
 */
class DecodeJsonHandler : public Handler {
protected:
public:
    std::set<std::string> valid_case;

    bool request(const SendMessge &m) override;
    friend class HandlerFactory;
};

/**
 * создаем обработчики и конфигурируем их
 */
class HandlerFactory {
    static void set_config(Config *c, std::shared_ptr<DecodeJsonHandler> &h);

    static void set_config(Config *c, std::shared_ptr<ReflectHandler> &h);

    static void set_config(Config *c, std::shared_ptr<DomotizcHandler> &h);

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
    static bool request(const SendMessge &m);
};


#endif // MQTTREFLECT_HPP
