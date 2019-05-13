#include <chrono>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <map>
#include <mqtt/client.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/syslog_sink.h>

#include <cxxopts.hpp>

#include "config.hpp"
#include "mqtt-reflect.hpp"

const char *config_file = "mqttfrwd.json";
const int MAX_REFLECT_DEPTH = 10;
using namespace std;
using namespace rapidjson;

std::shared_ptr<spdlog::logger> sysloger;
// --------------------------------------------------------------------------
// Simple function to manually reconect a client.
CSendQueue HandlerFactory::mqtt_mag_queue;

bool try_reconnect(mqtt::client &cli) {
    constexpr int N_ATTEMPT = 30;

    for (int i = 0; i < N_ATTEMPT && !cli.is_connected(); ++i) {
        try {
            cli.reconnect();
            return true;
        }
        catch (const mqtt::exception &) {
            this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return false;
}

/**
 * проверить что значение payload - допустимое
 * TODO пока что не пропускает русские буквы
 * TODO проверить на (null) - не должно пропускать
 *
 * @param str
 * @return true допустимо
 */
bool is_valid_payload(const std::string &str) {
    for (auto x : str)
        if (!std::isprint(x) && x != '\n')
            return false;
    return true;
}

/**
 * отправить сообщение всем обработчикам
 *
 * список обработчиков лежить в @var HandlerFactory::handler_list
 * @param m
 * @return
 */
bool HandlerFactory::request(const SendMessge &m) {
    for (auto &h : handler_list)
        if (!h->request(m))
            return false;
    return true;
}

void Handler::send_msg(const SendMessge &msg) {
    HandlerFactory::mqtt_mag_queue.push_front(SendMessge(msg));
}

/////////////////////////////////////////////////////////////////////////////
/**
 * обходим дерево json и расылаем сообщения
 * https://stackoverflow.com/questions/30896857/iterate-and-retrieve-nested-object-in-json-using-rapidjson
 * @param key
 * @param it
 * @param topic
 * @param h
 */
void recursive(const string &key, Value::ConstMemberIterator it, const std::string &topic, Handler *h) {
    string _key(key);
    std::transform(_key.begin(), _key.end(), _key.begin(), ::tolower);
    string t(topic + "/" + _key);
    if (it->value.IsObject()) {
        for (auto p = it->value.MemberBegin(); p != it->value.MemberEnd(); ++p) {
            string k = p->name.GetString();
            recursive(k, p, t, h);
        }
    } else {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        it->value.Accept(writer);
        SendMessge mm(t, buffer.GetString());
        sysloger->trace("{0} : {1}", t.c_str(), mm.payload.c_str());
        h->send_msg(mm);
    }
}

// http://rapidjson.org/md_doc_tutorial.html
// https://github.com/nlohmann/json recommended
bool DecodeJsonHandler::request(const SendMessge &m) {
    int ix = m.topic.rfind('/');
    if (ix == -1)
        return false;
    string sens((m.topic.begin() + ix + 1), m.topic.end());
    if (valid_case.find(sens) != valid_case.end()) {
        Document document;
        if (document.Parse(m.payload.c_str()).HasParseError())
            return 1;
        for (auto p = document.MemberBegin(); p != document.MemberEnd(); ++p) {
            string key = p->name.GetString();
            // traverse tree
            recursive(key, p, m.topic, this);
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////
void HandlerFactory::set_config(Config *c, shared_ptr<DecodeJsonHandler> &h) {
    string head = c->getOpt("MQTT", "topic_head");
    // TODO возможно сделать regexp
    h->valid_case = {"sensor", "energy"};
    string s;
    for (auto const& e : h->valid_case)     {
        s += e; s += ',';
    }
    sysloger->debug("+json:{0}", s.c_str());
}

/////////////////////////////////////////////////////////////////////////////
void HandlerFactory::set_config(Config *c, shared_ptr<ReflectHandler> &h) {
    shared_ptr<IniSection> ini = c->getSection("reflect");
    string head = c->getOpt("MQTT", "topic_head");
    for (auto &p : *ini) {
        string t = p.first;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        h->reflect[head + t] = head + p.second;
        sysloger->debug("+reflect:{0}-{1}", p.first.c_str(), p.second.c_str());
    }
}

/**
 * @brief split
 * @param s
 * @param delim
 * @return
 */
vector<string> split(const string &s, const char delim) {
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;
    while ((pos = s.find(delim, pos)) != std::string::npos) {
        std::string substring(s.substr(prev_pos, pos - prev_pos));
        output.push_back(substring);
        prev_pos = ++pos;
    }
    output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

    return output;
}

/**
 * @brief ReflectHandler::request
 * отражаем и размножаем сообщение.
 * требуется новая обработка порожденных сообщений
 *
 *
 * @param m
 * @return
 */
bool ReflectHandler::request(const SendMessge &m) {
    string topic(m.topic);
    if (reflect.find(topic) != reflect.end()) {
        vector<string> strs;
        sysloger->trace("relf:{0}::{1}={3}", topic.c_str(), reflect[topic].c_str(), m.payload.c_str());
        strs = split(reflect[topic], ',');
        string payload = m.payload;
        for (auto &s : strs) {
            SendMessge mn(s, payload);
            send_msg(mn);
            // ограничить число новых сообщений и число циклов decorator
            static int stack_count = 0;
            stack_count++;
            if (stack_count > MAX_REFLECT_DEPTH) {
                sysloger->error("reflect loop:{0}", m.topic.c_str());
                stack_count = 0;
                break;
            }
            HandlerFactory::request(mn);
            stack_count--;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// \brief DomotizcHandler::set_config
/// \param c
///
void HandlerFactory::set_config(Config *c, shared_ptr<DomotizcHandler> &h) {
    shared_ptr<IniSection> ini = c->getSection("Domotizc");
    string head = c->getOpt("MQTT", "topic_head");
    for (auto p : *ini) {
        string topic = p.first;
        std::transform(topic.begin(), topic.end(), topic.begin(), ::tolower);
        char *pend;
        h->domotizc[head + topic] = std::strtol(p.second.c_str(), &pend, 10);
        sysloger->debug("+domo:{0}-{1}", p.first.c_str(), p.second.c_str());
    }
}

/**
 * @brief DomotizcHandler::request
 * перекодируем сообщение для domotizc
 *
 * никакой дальнейшей обработки этих сообщений не нужно
 * @param m
 * @return
 */
bool DomotizcHandler::request(const SendMessge &m) {
    string topic(m.topic);
    if (domotizc.count(topic) > 0) {
        char buf[100];
        string payload = m.payload;
        std::snprintf(buf, 100, R"({ "idx" : %d, "nvalue" : 0, "svalue": "%s" })",
                      domotizc[topic], payload.c_str());
        std::string tval = buf;
        sysloger->trace("domotizc:{0}",  tval.c_str());
        send_msg(SendMessge("domoticz/in", tval));
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/**
 * @brief mqtt_loop
 * @param cli
 * @return
 */
int mqtt_loop() {
    Config *cfg = Config::getInstance();
    vector<string> TOPICS{"stat/#", "tele/#", "rs485/#"};
    const vector<int> QOS{0, 0, 0};
    HandlerFactory::makeAll(cfg);
    string head = cfg->getOpt("MQTT", "topic_head");
    if (!head.empty()) {
        for (auto ix = 0; ix < TOPICS.size(); ++ix) {
            TOPICS[ix] = head + TOPICS[ix];
        }
    }

    const string CLIENT_ID{"sync_consume_cpp"};

    mqtt::client cli(cfg->getOpt("MQTT", "server"), CLIENT_ID);
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try {
        sysloger->info("Connecting to the MQTT server... {0}", cfg->getOpt("MQTT", "server").c_str());
        cli.connect(connOpts);
        cli.subscribe(TOPICS, QOS);

        // Consume messages
        uint16_t cnt = 0;
        while (true) {
            auto msg = cli.consume_message();
            // восстанавливаем соединение
            if (!msg) {
                if (!cli.is_connected()) {
                    sysloger->error("Lost connection. Attempting reconnect");
                    if (try_reconnect(cli)) {
                        cli.subscribe(TOPICS, QOS);
                        sysloger->warn("Reconnected");
                        continue;
                    } else {
                        sysloger->critical("Reconnect failed.");
                        break;
                    }
                } else
                    break;
            }
            sysloger->trace("{0}:{1}", msg->get_topic().c_str(), msg->to_string().c_str());
            // TODO в этот моменту очередь @var mqtt_mag_queue должна быть пустой
            // проверить что содержимое - печатное
            if (is_valid_payload(msg->get_payload_str())) {
                SendMessge mn(msg->get_topic(), msg->get_payload_str());
                HandlerFactory::request(mn);
                for (auto &m : HandlerFactory::mqtt_mag_queue) {
                    const int mQOS = 0;
                    sysloger->trace("publ:{0}::{1}", m.topic.c_str(),  m.payload.c_str());
                    cli.publish(m.topic, m.payload.c_str(), m.payload.size());
                }
                HandlerFactory::mqtt_mag_queue.clear();
            }
        }
        // Disconnect
        sysloger->info("Disconnecting from the MQTT server...");
        cli.disconnect();
    }
    catch (const mqtt::exception &exc) {
        cerr << exc.what();
        return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
    cxxopts::Options options("mqttreflect", "reflect events on MQTT bus");
    options.add_options()
      ("d,debug", "Enable debugging");
    auto result = options.parse(argc, argv);

    string ident = "mqttreflect";
    sysloger = spdlog::syslog_logger_mt("syslog", ident, LOG_PID);
    if(result["debug"].as<bool>())
        sysloger->set_level(spdlog::level::trace);
    else
        sysloger->set_level(spdlog::level::info);
    Config *cfg = Config::getInstance();
    {
        char cwd[256];
        getcwd(cwd, sizeof(cwd));
        sysloger->info("mqttreflect is starting, using:{0}/{1}", cwd, config_file);
    }
    cfg->open(config_file);
    mqtt_loop();
    sysloger->info("mqttreflect is stopped");
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
std::list<std::shared_ptr<Handler>> HandlerFactory::handler_list;

void HandlerFactory::makeAll(Config *cfg) {
    std::shared_ptr<DomotizcHandler> domotizc;
    std::shared_ptr<ReflectHandler> reflect;
    std::shared_ptr<DecodeJsonHandler> json;

    domotizc = std::make_shared<DomotizcHandler>();
    set_config(cfg, domotizc);
    handler_list.push_front(domotizc);

    reflect = std::make_shared<ReflectHandler>();
    set_config(cfg, reflect);
    handler_list.push_front(reflect);

    json = std::make_shared<DecodeJsonHandler>();
    set_config(cfg, json);
    handler_list.push_front(json);
    return;
}
