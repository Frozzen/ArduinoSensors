// sync_consume.cpp
//
// This is a Paho MQTT C++ client, sample application.
//
// This application is an MQTT consumer/subscriber using the C++ synchronous
// client interface, which uses the queuing API to receive messages.
//
// The sample demonstrates:
//  - Connecting to an MQTT server/broker
//  - Subscribing to multiple topics
//  - Receiving messages through the queueing consumer API
//  - Recieving and acting upon commands via MQTT topics
//  - Manual reconnects
//  - Using a persistent (non-clean) session
//

/*******************************************************************************
 * Copyright (c) 2013-2017 Frank Pagliughi <fpagliughi@mindspring.com>
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Frank Pagliughi - initial implementation and documentation
 *******************************************************************************/

#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>
#include <boost/range/algorithm/find.hpp>
#include <boost/log/trivial.hpp>
//#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <mqtt/client.h>

#include "config.hpp"
#include "serial.hpp"

#include "mqtt-reflect.hpp"

using namespace std;
using namespace std::chrono;

//const string SERVER_ADDRESS	{ "tcp://172.20.110.2:1883" };
const string CLIENT_ID		{ "sync_consume_cpp" };
const int QOS = 0;
// --------------------------------------------------------------------------
// Simple function to manually reconect a client.

bool try_reconnect(mqtt::client& cli)
{
    constexpr int N_ATTEMPT = 30;

    for (int i=0; i<N_ATTEMPT && !cli.is_connected(); ++i) {
        try {
            cli.reconnect();
            return true;
        }
        catch (const mqtt::exception&) {
            this_thread::sleep_for(seconds(1));
        }
    }
    return false;
}

void Handler::send_msg(mqtt::message &msg)
{
    if(queue)
        queue->push_front(msg);
    else
        if(next)
            next->send_msg(msg);
}

/////////////////////////////////////////////////////////////////////////////
bool DecodeEnergyHandler::request(mqtt::message & m)
{
    int ix = m.get_topic().rfind('/');
    if(ix != -1) {
        string sens((m.get_topic().begin()+ix), m.get_topic().end());
        if(sens == "SENSOR") {
            stringstream ss(m.get_payload_str());
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(ss, pt);
            return true;
        }
    }
    Handler::request(m);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
void ReflectHandler::set_reflect(Config *c)
{
    IniSection ini = c->getSection("reflect");
    for(auto p : ini) {
        reflect[p.first] = p.second;
        cout << "reflect+:" << p.first << '-' << p.second << endl;
    }
}

vector<string> split(const string &s, const char delim)
{
    stringstream ss(s);
    string item;
    vector<string> elems;
    while(getline(ss, item, delim))
        elems.push_back(move(item));
    return elems;
}
bool ReflectHandler::request(mqtt::message & m)
{
    if(reflect.count(m.get_topic())) {
#if 1
        vector<string> strs;
        strs = split(reflect[m.get_topic()], ',');
        for(auto s : strs) {
            mqtt::message mn(s, m.get_payload_str());
            send_msg(mn);
        }
#else
        mqtt::message mn(reflect[m.topic], m.payload);
        send_msg(mn);
#endif
        return true;
    }
    Handler::request(m);
    return false;
}

/**
 * @brief ReflectHandler::send_msg
 * рекурсия раскручиваем - цепочки конверсии
 * @param msg
 */
void ReflectHandler::send_msg(mqtt::message & msg)
{
    Handler::send_msg(msg);
    request(msg);
}

/////////////////////////////////////////////////////////////////////////////
void DomotizcHandler::set_reflect(Config *c)
{
    IniSection ini = c->getSection("Domotizc");
    for(auto p : ini) {
        domotizc[p.first] = std::atoi(p.second.c_str());
        cout << "domo+:" << p.first << '-' << p.second << endl;
    }
}

bool DomotizcHandler::request(mqtt::message & m)
{
    if(domotizc.count(m.get_topic()) > 0) {
        char buf[100];
        std::snprintf(buf, 100, "{ \"idx\" : %d, \"nvalue\" : 0, \"svalue\": \"%s\" }",
                                                domotizc[m.get_topic()], m.get_payload_str().c_str());
        std::string tval = buf;
        cout << "domotizc:" << tval << endl;
        mqtt::message mm("domoticz/in", tval);
        send_msg(m);
        return true;
    }
    Handler::request(m);
    return false;
}

CSendQueue msg_to_send;

/////////////////////////////////////////////////////////////////////////////

int mqtt_loop(mqtt::client &cli)
{
    Config *cfg = Config::getInstance();
    const vector<string> TOPICS { "stat/#", "tele/#" };
    const vector<int> QOS { 0, 0 };
    Handler handler(&msg_to_send);

    DomotizcHandler domotizc;
    domotizc.set_reflect(cfg);
    handler.pushNextHandler(&domotizc);

    ReflectHandler reflect;
    reflect.set_reflect(cfg);
    handler.pushNextHandler(&reflect);

    mqtt::connect_options connOpts(cfg->getOpt("MQTT.user"), cfg->getOpt("MQTT.pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try {
        cout << "Connecting to the MQTT server..." << flush;
        cli.connect(connOpts);
        cli.subscribe(TOPICS, QOS);
        cout << "OK\n" << endl;

        // Consume messages

        while (true) {
            auto msg = cli.consume_message();

            if (!msg) {
                if (!cli.is_connected()) {
                    cout << "Lost connection. Attempting reconnect" << endl;
                    if (try_reconnect(cli)) {
                        cli.subscribe(TOPICS, QOS);
                        cout << "Reconnected" << endl;
                        continue;
                    }
                    else {
                        cout << "Reconnect failed." << endl;
                        break;
                    }
                }
                else
                    break;
            }
            mqtt::message mqtt(msg->get_topic(), msg->get_payload());
            handler.request(mqtt);
            for(auto it : msg_to_send) {
                const int mQOS = 0;
                cli.publish(mqtt::message(it.get_topic(), it.get_payload(), mQOS, false));
            }
            cout << msg->get_topic() << ": " << msg->to_string() << endl;
        }

        // Disconnect

        cout << "\nDisconnecting from the MQTT server..." << flush;
        cli.disconnect();
        cout << "OK" << endl;
    }
    catch (const mqtt::exception& exc) {
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
                ;// cout << " err cs:" << hex << cs << " res:" << csorg << ':' << *(ix_end-3)  << *(ix_end-2)<< endl;
            else {
                std::string str(ix_start+3, ix_end-4);
                auto it = boost::find(str, '=');
                cout << " rs:" << str << " end:" << *it << endl;
                if(it == str.end()) {
                    cout << "***com!:" <<  str << endl;
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

int main(int argc, char* argv[])
{
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.ini");

    mqtt::client cli(cfg->getOpt("MQTT.server"), CLIENT_ID);

    mqtt_loop(cli);
#if 0
    thread t1(read_serial, "ttyUSB0");
    rs232_looping = false;
    t1.join();
#endif
    return 0;
}

