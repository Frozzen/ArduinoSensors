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
#include <boost/property_tree/json_parser.hpp>
#include <mqtt/client.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include "config.hpp"
#include "serial.hpp"

#include "mqtt-reflect.hpp"

const int MAX_REFLECT_DEPTH = 10;
using namespace std;
using namespace std::chrono;
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
/*
 * https://stackoverflow.com/questions/30896857/iterate-and-retrieve-nested-object-in-json-using-rapidjson
void enter(const Value &obj, size_t indent = 0) { //print JSON tree

    if (obj.IsObject()) { //check if object
        for (Value::ConstMemberIterator itr = obj.MemberBegin(); itr != obj.MemberEnd(); ++itr) {   //iterate through object
            const Value& objName = obj[itr->name.GetString()]; //make object value

            for (size_t i = 0; i != indent; ++i) //indent
                //cout << " ";

            //cout << itr->name.GetString() << ": "; //key name

            if (itr->value.IsNumber()) //if integer
                std:://cout << itr->value.GetInt() ;

            else if (itr->value.IsString()) //if string
                std:://cout << itr->value.GetString();


            else if (itr->value.IsBool()) //if bool
                std:://cout << itr->value.GetBool();

            else if (itr->value.IsArray()){ //if array

                for (SizeType i = 0; i < itr->value.Size(); i++) {
                    if (itr->value[i].IsNumber()) //if array value integer
                        std:://cout << itr->value[i].GetInt() ;

                    else if (itr->value[i].IsString()) //if array value string
                        std:://cout << itr->value[i].GetString() ;

                    else if (itr->value[i].IsBool()) //if array value bool
                        std:://cout << itr->value[i].GetBool() ;

                    else if (itr->value[i].IsObject()){ //if array value object
                        //cout << "\n  ";
                        const Value& m = itr->value[i];
                        for (auto& v : m.GetObject()) { //iterate through array object
                            if (m[v.name.GetString()].IsString()) //if array object value is string
                                //cout << v.name.GetString() << ": " <<   m[v.name.GetString()].GetString();
                            else //if array object value is integer
                                //cout << v.name.GetString() << ": "  <<  m[v.name.GetString()].GetInt();

                            //cout <<  "\t"; //indent
                        }
                    }
                    //cout <<  "\t"; //indent
                }
            }

            //cout << endl;
            enter(objName, indent + 1); //if couldn't find in object, enter object and repeat process recursively
        }
    }
}

 Value v = document.GetObject();
Value& m= v;
enter(m);
*/
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
    if (reflect.count(topic)) {
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
#ifndef NDEBUG
            if (cnt++ > 3000)
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
    // std::setlocale(LC_ALL, "ru_RU.UTF-8");
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.json");

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

    json = std::make_shared<DecodeJsonHandler>();
    set_config(cfg, json);
    handler_list.push_front(json);
    return;
}
