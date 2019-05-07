#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <map>
#include <boost/algorithm/string.hpp>
#include <mqtt/client.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include "config.hpp"

#include "mqtt-reflect.hpp"

const int MAX_REFLECT_DEPTH = 10;
using namespace std;
using namespace rapidjson;

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
            this_thread::sleep_for(seconds(1));
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
        //cout << key << " >>";
        for (auto p = it->value.MemberBegin(); p != it->value.MemberEnd(); ++p) {
            string k = p->name.GetString();
            recursive(k, p, t, h);
        }
    } else {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        it->value.Accept(writer);
        SendMessge mm(t, buffer.GetString());
        //cout << t << " : " << mm.payload << "\n";
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
            //cout << p->name.GetString() << "\n";
            // traverse tree
            recursive(key, p, m.topic, this);
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////
void HandlerFactory::set_config(Config *c, shared_ptr<DecodeJsonHandler> &h) {
    string head = c->getOpt("MQTT", "topic_head");
    // TODO посмотреть какие еще ключи надо смотреть
    // TODO возможно сделать regexp
    h->valid_case = {"sensor"};
}

/////////////////////////////////////////////////////////////////////////////
void HandlerFactory::set_config(Config *c, shared_ptr<ReflectHandler> &h) {
    shared_ptr<IniSection> ini = c->getSection("reflect");
    string head = c->getOpt("MQTT", "topic_head");
    for (auto &p : *ini) {
        auto t = boost::algorithm::to_lower_copy(p.first);
        h->reflect[head + t] = head + boost::algorithm::to_lower_copy(p.second);
        //cout << "reflect+:" << p.first << '-' << p.second << endl;
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
        //cout << "relf:" << topic << "::";
        strs = split(reflect[topic], ',');
        string payload = m.payload;
        for (auto &s : strs) {
            //cout << ">" << s << "< ";
            SendMessge mn(s, payload);
            send_msg(mn);
            // ограничить число новых сообщений и число циклов decorator
            static int stack_count = 0;
            stack_count++;
            if (stack_count > MAX_REFLECT_DEPTH) {
                cerr << "reflect loop:" << m.topic << endl;
                stack_count = 0;
                break;
            }
            HandlerFactory::request(mn);
            stack_count--;
        }
        //cout << "=>" << payload << endl;
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
        string topic = boost::algorithm::to_lower_copy(p.first);
        char *pend;
        h->domotizc[head + topic] = std::strtol(p.second.c_str(), &pend, 10);
        //cout << "domo+:" << p.first << '-' << p.second << endl;
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
        //cout << "domotizc:" << tval << endl;
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
        //std::transform(TOPICS.begin(), TOPICS.end(),TOPICS.begin(),  [head](string s) -> string { return head + s; });
    }

    const string CLIENT_ID{"sync_consume_cpp"};

    mqtt::client cli(cfg->getOpt("MQTT", "server"), CLIENT_ID);
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try {
        //cout << "Connecting to the MQTT server..." << flush;
        cli.connect(connOpts);
        cli.subscribe(TOPICS, QOS);
        //cout << "OK\n" << endl;

        // Consume messages
        uint16_t cnt = 0;
        while (true) {
            auto msg = cli.consume_message();
            // восстанавливаем соединение
            if (!msg) {
                if (!cli.is_connected()) {
                    //cout << "Lost connection. Attempting reconnect" << endl;
                    if (try_reconnect(cli)) {
                        cli.subscribe(TOPICS, QOS);
                        //cout << "Reconnected" << endl;
                        continue;
                    } else {
                        //cout << "Reconnect failed." << endl;
                        break;
                    }
                } else
                    break;
            }
            //cout << msg->get_topic() << ": " << msg->to_string() << endl;
            // TODO в этот моменту очередь @var mqtt_mag_queue должна быть пустой
            // проверить что содержимое - печатное
            if (is_valid_payload(msg->get_payload_str())) {
                SendMessge mn(msg->get_topic(), msg->get_payload_str());
                std::transform(mn.topic.begin(), mn.topic.end(), mn.topic.begin(), ::tolower);
                HandlerFactory::request(mn);
                for (auto &m : HandlerFactory::mqtt_mag_queue) {
                    const int mQOS = 0;
                    cli.publish(m.topic, m.payload.c_str(), m.payload.size());
                }
                HandlerFactory::mqtt_mag_queue.clear();
            }
        }
        // Disconnect
        //cout << "\nDisconnecting from the MQTT server..." << flush;
        cli.disconnect();
        //cout << "OK" << endl;
    }
    catch (const mqtt::exception &exc) {
        cerr << exc.what() << endl;
        return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.json");
    mqtt_loop();
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
