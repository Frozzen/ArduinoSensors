#include <stdint.h>

// послать команду и получить ответ от устройства
class JDY40 {
  public:
    JDY40();
    void setup();
    void init();
    void getConfig(const char*str);
    void send(const char* str);
    void put(char c);
    char get();
    uint8_t status();
};
extern JDY40 jdy;
