#include <cstdio>
#include <iostream>
#include "config.hpp"

using namespace std;

int main(int, char *[]) {
    Config *cfg = Config::getInstance();
    cfg->open("mqttfrwd.json");
    string str = cfg->getOpt("MQTT", "server");
    cout << "Server:" << str << endl;
    auto ini = move(cfg->getSection("reflect").release());
    cout << "reflect:" << endl;
    for (auto i : *ini)
        cout << " " << i.first << ":" << i.second << endl;
    return 0;
}