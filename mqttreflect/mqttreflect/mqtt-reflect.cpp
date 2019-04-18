#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/log/trivial.hpp>
//#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <mqtt/client.h>

#include "config.hpp"
#include "serial.hpp"

#include "mqtt-reflect.hpp"

const int MAX_REFLECT_DEPTH = 10;
using namespace std;
using namespace std::chrono;

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
bool HandlerFactory::request(mqtt::const_message_ptr m) {
    for (auto &h : handler_list)
        if (!h->request(m))
            return false;
    return true;
}

void Handler::send_msg(mqtt::const_message_ptr msg) {
    HandlerFactory::mqtt_mag_queue.push_front(msg);
}

/////////////////////////////////////////////////////////////////////////////
void DecodeJsonHandler::recursive_dump_json(int level, boost::property_tree::ptree &pt, const string &from) {
    if (pt.empty()) {
    } else {
        for (ptree::iterator pos = pt.begin(); pos != pt.end();) {
            std::string topic(from + "/" + pos->first);
            if (pos->second.empty())
                recursive_dump_json(++level, pos->second, topic);
            else {
                auto mm = std::make_shared<const mqtt::message>(topic, pos->second.data());
                send_msg(mm);
            }
            ++pos;
        }
    }
}

bool DecodeJsonHandler::request(mqtt::const_message_ptr m) {
    int ix = m->get_topic().rfind('/');
    if (ix == -1)
        return false;
    string sens((m->get_topic().begin() + ix), m->get_topic().end());
    if (valid_case.find(sens) != valid_case.end()) {
        stringstream ss(m->get_payload_str());
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);
        // traverse tree
        recursive_dump_json(0, pt, m->get_topic());
    }
    return true;
}

void HandlerFactory::set_config(Config *c, shared_ptr<DecodeJsonHandler> &h) {
    string head = c->getOpt("MQTT.topic_head");
    // TODO посмотреть какие еще ключи надо смотреть
    // TODO возможно сделать regexp
    h->valid_case = {"SENSOR", "ENERGY"};
}

/////////////////////////////////////////////////////////////////////////////
void HandlerFactory::set_config(Config *c, shared_ptr<ReflectHandler> &h) {
    shared_ptr<IniSection> ini = c->getSection("reflect");
    string head = c->getOpt("MQTT.topic_head");
    for (auto &p : *ini) {
        auto t = boost::algorithm::to_lower_copy(p.first);
        h->reflect[head + t] = head + boost::algorithm::to_lower_copy(p.second);
        ////cout << "reflect+:" << p.first << '-' << p.second << endl;
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
bool ReflectHandler::request(mqtt::const_message_ptr m) {
    string topic(m->get_topic());
    std::transform(topic.begin(), topic.end(), topic.begin(), ::tolower);
    if (reflect.count(topic)) {
        vector<string> strs;
        //cout << "relf:" << topic << "::";
        strs = split(reflect[topic], ',');
        string payload = m->get_payload();
        for (auto &s : strs) {
            //cout << ">" << s << "< ";
            auto mn = mqtt::message::create(s, payload, 0, true);
            send_msg(mn);
            // ограничить число новых сообщений и число циклов decorator
            static int stack_count = 0;
            stack_count++;
            if (stack_count > MAX_REFLECT_DEPTH) {
                cerr << "reflect loop:" << m->get_topic() << endl;
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
    string head = c->getOpt("MQTT.topic_head");
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
bool DomotizcHandler::request(mqtt::const_message_ptr m) {
    string topic(m->get_topic());
    std::transform(topic.begin(), topic.end(), topic.begin(), ::tolower);
    if (domotizc.count(topic) > 0) {
        char buf[100];
        string payload = m->get_payload_str();
        std::snprintf(buf, 100, R"({ "idx" : %d, "nvalue" : 0, "svalue": "%s" })",
                      domotizc[topic], payload.c_str());
        std::string tval = buf;
        //cout << "domotizc:" << tval << endl;
        send_msg(std::make_shared<mqtt::message>("domoticz/in", tval));
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
    string head = cfg->getOpt("MQTT.topic_head");
    if (!head.empty()) {
        for (auto ix = 0; ix < TOPICS.size(); ++ix) {
            TOPICS[ix] = head + TOPICS[ix];
        }
        //std::transform(TOPICS.begin(), TOPICS.end(),TOPICS.begin(),  [head](string s) -> string { return head + s; });
    }

    const string CLIENT_ID{"sync_consume_cpp"};

    mqtt::client cli(cfg->getOpt("MQTT.server"), CLIENT_ID);
    mqtt::connect_options connOpts(cfg->getOpt("MQTT.user"), cfg->getOpt("MQTT.pass"));
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
            if (!is_valid_payload(msg->get_payload_str())) {
                HandlerFactory::request(msg);
                for (auto &m : HandlerFactory::mqtt_mag_queue) {
                    const int mQOS = 0;
                    cli.publish(std::make_shared<mqtt::message>(m->get_topic(), m->get_payload(), mQOS, false));
                }
                HandlerFactory::mqtt_mag_queue.clear();
            }
#ifndef NDEBUG
            if (cnt++ > 8000)
                break;
#endif
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
#if 0
static volatile bool rs232_looping = true;
int read_serial(const char *dev)
{
    char mode[]={'8','N','1',0};
    std::vector<char> buf(4096);
    int port = RS232_GetPortnr(dev);
    if(RS232_OpenComport(port, 38400, mode)) {
      printf("Can not open comport\n");
      return(2);
    }

    Config *cfg = Config::getInstance();
    std::string head = cfg->getOpt("COM.topic_head");

    while(rs232_looping)     {
      int n = RS232_PollComport(port, (unsigned char*)buf.data(), buf.size() - 1);

      if(n > 0)       {
        buf[n] = '\0';   /* always put a "null" at the end of a string! */
        auto ix_start = ::boost::find(buf, ':');
        auto ix_end = ::boost::find(buf, '\n');
        if(ix_end != buf.end() && ix_start != buf.end() && ix_start < (ix_end - 6)) {
            int cs = 0;
            char *e;
            int csorg = strtol(&*(ix_end-3), &e, 16);
            for(auto it = ix_start; it < ix_end - 4; ++it)
               { cs += *it;  }
            if((cs & 0xff) != csorg)
                ;// //cout << " err cs:" << hex << cs << " res:" << csorg << ':' << *(ix_end-3)  << *(ix_end-2)<< endl;
            else {
                std::string str(ix_start+3, ix_end-4);
                auto it = boost::find(str, '=');
                //cout << " rs:" << str << " end:" << *it << endl;
                if(it == str.end()) {
                    //cout << "***com!:" <<  str << endl;
                    continue;
                }
                std::string topic(str.begin(), it);
                std::string payload(it+1, str.end());
                mqtt::message & m(head + topic, payload);
                msg_to_send.push_front(m);
            }
        } else if(n < 0)
            return 1;
      }
      usleep(100000);  /* sleep for 100 milliSeconds */
    }
}
#endif
/////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    std::setlocale(LC_ALL, "ru_RU.UTF-8");
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.ini");

    mqtt_loop();
#if 0
    thread t1(read_serial, "ttyUSB0");
    rs232_looping = false;
    t1.join();
#endif
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
    return;

    json = std::make_shared<DecodeJsonHandler>();
    set_config(cfg, json);
    handler_list.push_front(reflect);
}
