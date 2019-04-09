
#define MAX_OUT_BUFF 100
#define HOST_ADDR ":01"

void sendToServer(const char *r, bool send=false);
#ifndef NO_TEMP
const char *getAddrString(DeviceAddress &dev);
void setupArduSens();
void doTestContacts();
bool doSendTemp();
#endif
void doAlive();
extern uint16_t s_time_cnt;

#define ADDR_STR HOST_ADDR SENSOR_NAME DEVICE_NO
extern char s_buf[MAX_OUT_BUFF];  
