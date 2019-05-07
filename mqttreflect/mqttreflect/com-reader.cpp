//
// Created by vovva on 07.05.19.
// https://github.com/dpkoch/async_comm
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <map>
#include <mqtt/client.h>
#include "config.hpp"
#include "serial.hpp"

using namespace std;
/////////////////////////////////////////////////////////////////////////////
static volatile bool rs232_looping = true;

int read_serial(const char *dev) {
    Config *cfg = Config::getInstance();

    const string CLIENT_ID{"sync_consume_cpp_2"};

    mqtt::client cli(cfg->getOpt("MQTT", "server"), CLIENT_ID);
    mqtt::connect_options connOpts(cfg->getOpt("MQTT", "user"), cfg->getOpt("MQTT", "pass"));
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    std::vector<char> buf(4096);
    int port = RS232_GetPortnr(dev);
    if (RS232_OpenComport(port, 38400, mode)) {
        printf("Can not open comport\n");
        return (2);
    }

    std::string head = cfg->getOpt("COM", "topic_head");

    try {
        //cout << "Connecting to the MQTT server..." << flush;
        cli.connect(connOpts);

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

            int n = RS232_PollComport(port, (unsigned char *) buf.data(), buf.size() - 1);

            if (n > 0) {
                buf[n] = '\0';   /* always put a "null" at the end of a string! */
                auto ix_start = buf.find(':');
                auto ix_end = buf.find('\n');
                if (ix_end != buf.end() && ix_start != buf.end() && ix_start < (ix_end - 6)) {
                    int cs = 0;
                    char *e;
                    int csorg = strtol(&*(ix_end - 3), &e, 16);
                    for (auto it = ix_start; it < ix_end - 4; ++it) { cs += *it; }
                    if ((cs & 0xff) !=
                        csorg);// //cout << " err cs:" << hex << cs << " res:" << csorg << ':' << *(ix_end-3)  << *(ix_end-2)<< endl;
                    else {
                        std::string str(ix_start + 3, ix_end - 4);
                        size_t ix = str.find('=');
                        //cout << " rs:" << str << " end:" << *ix << endl;
                        if (ix < 0) {
                            //cout << "***com!:" <<  str << endl;
                            continue;
                        }
                        std::string topic(str.begin(), str.begin() + ix);
                        std::string payload(str.begin() + ix + 1, str.end());
                        mqtt::message m(head + topic, payload);
                        cli.publish(m.topic, m.payload.c_str(), m.payload.size());
                    }
                } else if (n < 0)
                    return 1;
            }
            usleep(100000);  /* sleep for 100 milliSeconds */
        }
        if (is_valid_payload(msg->get_payload_str())) {
            SendMessge mn(msg->get_topic(), msg->get_payload_str());
            std::transform(mn.topic.begin(), mn.topic.end(), mn.topic.begin(), ::tolower);
            HandlerFactory::request(mn);
            for (auto &m : HandlerFactory::mqtt_mag_queue) {
                const int mQOS = 0;
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


int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage " << argv[0] << " /dev/ttyAMA0" << endl;
        return 1;
    }
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.json");
    mqtt_loop(argv[1]);
    return 0;
}