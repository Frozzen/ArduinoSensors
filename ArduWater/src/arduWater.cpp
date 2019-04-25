#include <Arduino.h>
#include <Wire.h>
#include <Bounce2.h>
#include <RtcDS1307.h>
#include <TM1637Display.h>

#include "ardudev.h"
/// TODO cmd занести в NVRAM значение для воды из serial, 
/// TODO cmd занести в NVRAM текущее время  из serial

/// открыть закрыть кран раз в 2 недели ночью
#define DAYS_REFRESHING_TAP 14
#define TIME_TO_RELAX 5
#define LOG_MESSAGE ":01log={\"type\":\"device_connected\",\"message\":\""
#define LOG_MESSAGE_END "\"}"

#define s_automatic_open_enabled 1

// Setup  instancees to communicate with devices I2C
RtcDS1307<TwoWire> Rtc(Wire);

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);
extern Bounce s_input_pin[IN_PIN_COUNT];

Bounce  btn_open_water,    // кнопка открыть воду
        water_cnt,                // счетчик воды
        btn_display,           // кнопка показать значения        
        water_is_empty,
        water_is_full,
        isTapOpen,
        isTapClosed;

// значение счетчика воды в 10 литрах
struct sEPROMConfig {
  uint32_t water_count_rtc = 0; // показания счетчика
  RtcDateTime last_water_tap_time;
} s_epromm;

uint32_t s_water_count;
// показывать на дисплее счетчик
bool s_displayMode = true;

char s_buf[MAX_OUT_BUFF];


void updateTapDateEPROM();
/// настроили кнопки которые нажимают
void iniInputPin(Bounce &bounce, uint8_t pin)
{
  pinMode(pin, INPUT); 
  digitalWrite(pin, HIGH);
  bounce.attach(pin); // Настраиваем Bouncer
  bounce.interval(TIME_TO_RELAX); // и прописываем ему интервал дребезга
}

/// проверили что кнопку отпустили
bool checkButtonChanged(Bounce &bounce)
{
  boolean changed = bounce.update(); 
  int value = HIGH;
  if ( changed ) {
      value = bounce.read();
      // Если значение датчика стало ЗАМКНУТО
      if ( value == LOW) {
        return true;
      }
    }  
  return false;
}

////////////////////////////////////////////////////////
void sendToServer(const char *r)
{
char buf[4];
  Serial.print(r);
  uint8_t sum = 0;
  for(const char *str = r; *str; ++str)
    sum += *str;
  sum = 0xff - sum;
  snprintf(buf, sizeof(buf), ";%02x", sum);
  Serial.println(buf);
}

bool isButtonChanged(Bounce &bounce, const char *title)
{
  boolean changed = bounce.update(); 
  if ( changed ) {
      uint8_t value = !bounce.read();
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "%s=%d", title, value);
      sendToServer(s_buf);
    }
    return changed;  
}

void sendDeviceConfig(const char *dev)
{
    snprintf(s_buf, sizeof(s_buf), LOG_MESSAGE SENSOR_NAME DEVICE_NO "%s" LOG_MESSAGE_END, dev);
    sendToServer(s_buf);
}

struct ICallback {
  virtual void done(uint8_t error) {
  }
};
/////////////////////////////////////////////////////////
/**
 * fsm для закрывания крана
 */
class CTapFSM {
  enum eTapStates {
    eTapNone = 100,
    eTapOpening,
    eTapClosing,
    eTapStop
  };
  int s_fsm_tap_state = eTapNone;
  public:

  enum eStates {
    eTapOpen, 
    eTapClose
  };
  uint32_t s_fsm_timeout = 0;
  // состояния ошибок
  enum eError {
    eOk,
    eError
  } m_error;

  ICallback *m_callback = nullptr;

  void change(eStates s, struct ICallback *c = nullptr) {
      s_fsm_tap_state = s;
      updateTapDateEPROM();
      snprintf(s_buf, sizeof(s_buf), ADDR_STR "/wtapcnmd=%d", s);
      sendToServer(s_buf);
      m_callback = c;
      m_error = eOk;
  };

  void init() {
    iniInputPin(isTapOpen, IS_TAP_OPEN);
    iniInputPin(isTapClosed, IS_TAP_CLOSE);
    
    pinMode(OPEN_TAP_CMD, OUTPUT);
    pinMode(CLOSE_TAP_CMD, OUTPUT);
    pinMode(WATER_PRESSURE, INPUT);
    digitalWrite(OPEN_TAP_CMD, HIGH);
    digitalWrite(CLOSE_TAP_CMD, HIGH);   
    if(getRealState() == eUndef)      
      change(water_is_full.read()?  eTapOpen : eTapClose);
    m_error = eOk;
  }

  enum eRealState {
    eUndef = 0,   // промежуточное
    eOpened = 0x1,
    eClosed = 0x2,
    eBroken = 0x4 // поломано или оборваны провода
  };

  /**
   * вернуть состояние кранов
   */
  static eRealState getRealState() {
    uint8_t close = !isTapClosed.read();
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/tap_closed=%d", close);
    sendToServer(s_buf);
    uint8_t open = !isTapOpen.read();
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/tap_open=%d", open);
    sendToServer(s_buf);

    uint8_t state = (open ? eOpened : 0) | (close ? eClosed:0);
    return state == (eOpened | eClosed) ? eBroken : (eRealState)state;
  }
  /**
   * собственно рабочй цикл fsm
   */
  void loop() {
  switch (s_fsm_tap_state)
    {
    case eTapOpening: {
      uint32_t delay = millis() - s_fsm_timeout;
      // проверить состояние открыто 
      if(!isTapOpen.read() || delay > TAP_TIMEOUT)
        // должно быть состояние открыто - иначе ошибка
        if(!isTapOpen.read())
          m_error = eError;
        s_fsm_tap_state = eTapStop;
        if(m_callback != nullptr)
          m_callback->done(m_error);
      }
      break;

    case eTapClosing: {
      uint32_t delay = millis() - s_fsm_timeout;
      //  проверить состояние закрыто
      if(!isTapClosed.read() || delay > TAP_TIMEOUT)
        //  должно быть состояние закрыто - иначе ошибка
        if(!isTapClosed.read())
          m_error = eError;
        s_fsm_tap_state = eTapStop;
        if(m_callback != nullptr)
          m_callback->done(m_error);
      }
      break;

    case eTapStop:
      digitalWrite(OPEN_TAP_CMD, HIGH);
      digitalWrite(CLOSE_TAP_CMD, HIGH);      
      s_fsm_tap_state = eTapNone;
      break;

    // команды открытия закрытия крана - работают только если предидущая работа завершена
    case eTapClose:
      digitalWrite(OPEN_TAP_CMD, HIGH);
      digitalWrite(CLOSE_TAP_CMD, LOW);      
      s_fsm_timeout = millis();
      s_fsm_tap_state = eTapClosing;
      break;

    case eTapOpen:
      digitalWrite(OPEN_TAP_CMD, LOW);
      digitalWrite(CLOSE_TAP_CMD, HIGH);      
      s_fsm_timeout = millis();
      s_fsm_tap_state = eTapOpening;
      break;    
    case eTapNone:
    default:
      break;
    }
  }
} s_tap_fsm;

/**
 * fsm для открывания и закрывания крана раз в неделю
 * заносим в eprom последнюю дату освежения
 * не отслеживаю здесь таймауты - callback
 */ 
class CRefreshTapFSM : public ICallback
{
  enum eState {
    eNone,
    eOpening, // это не собственно открытие а фаза 1
    eClosing  // фаза 2
  } s_fsm_state;
  public:

  // провернуть
  void change() {
      s_fsm_state = eOpening;    
  }

  // callback - по завершению операции
  virtual void done(uint8_t error) override {
    switch (s_fsm_state)
    {
      case eOpening:
        if(error == 0)
          s_fsm_state = eClosing;
        else {
          s_tap_fsm.getRealState();        
          s_fsm_state = eNone;
        }
        break;
      case eClosing:
        if(error != 0) {
          s_tap_fsm.getRealState();        
        }
        s_fsm_state = eNone;
        break;  
      default:
        break;
    }
  }

  void init() {
      s_fsm_state = eNone;
  }

  void loop() {
    uint16_t rval = 0;
    switch(s_fsm_state) {
    case eOpening: 
      rval = 0;
      break;    
    case eClosing:
      rval = 1;
      break;    
    case eNone:
    default:
      return;
    }
    CTapFSM::eRealState state = s_tap_fsm.getRealState();
    if(state == CTapFSM::eOpened)
      s_tap_fsm.change(CTapFSM::eTapClose, this);
    else if(state == CTapFSM::eClosed)
      s_tap_fsm.change(CTapFSM::eTapOpen, this);
    else 
      // послали ошибку
      rval = 3;
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/tap_refresh=%d", rval);
    sendToServer(s_buf);
  }
  /**
   * проверить надо ли поварачивать кран
   */
  void check() {
      // сравнить время открывания и текущее
      RtcDateTime cur = Rtc.GetDateTime();
      cur -= 3600L*24*DAYS_REFRESHING_TAP;
      if(cur > s_epromm.last_water_tap_time)
        change();
  }
} s_refresh_fsm;

/**
 *  обновляем время когда двигали кран
 */
void updateTapDateEPROM()
{
  s_epromm.last_water_tap_time = Rtc.GetDateTime();
  Rtc.SetMemory(0, (uint8_t*)&s_epromm, sizeof(s_epromm));
}


//------------------------------------------------------
/// послать в шину изменения в контактах
void   doTestWater() {

  if(checkButtonChanged(water_cnt)) {
    ++s_water_count;
    snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/water=%ld", s_water_count);
    sendToServer(s_buf);
  }

  // переключить экранчик
  if(checkButtonChanged(btn_display)) {
      s_displayMode ^= true;
  }

  // запустить FSM на изменение ветниля
  if(checkButtonChanged(btn_open_water)) {
    bool open = !isTapClosed.read();
    s_tap_fsm.change(open ? CTapFSM::eTapOpen: CTapFSM::eTapClose);
  }

  // при появлении 1цы дать команду открыть кран - водзможно сделать блокировку этого
  if(isButtonChanged(water_is_empty, "/barrel_empty") &&
    !isTapClosed.read() &&  !water_is_empty.read() && s_automatic_open_enabled)
      s_tap_fsm.change(CTapFSM::eTapOpen);
  // при появлении 1цы дать команду закрыть кран
  if(isButtonChanged(water_is_full, "/barrel_full") && !water_is_full.read())
    s_tap_fsm.change(CTapFSM::eTapClose);

  isButtonChanged(isTapClosed, "/tap_closed");
  isButtonChanged(isTapOpen, "/tap_open");
}

/**
 * отправить на хоста давление
 * Использовал программу, которую предоставил продавец. 
 * Заметил, что в покое он показывает отрицательное значение, 
 * равное -3.35 кПа. я не могу понять, с чем связана данная цифра.
*/
void doWaterPressure()
{

  int raw = analogRead(WATER_PRESSURE);
  float voltage = (float) raw * 5.0 / 1024.0;     // voltage at the pin of the Arduino
  float pressure_kPa = (voltage - 0.5) / 4.0 * 1.200;          // voltage to pressure
  char str_press[6];
  dtostrf((float)pressure_kPa, 4, 1, str_press);
  snprintf(s_buf, sizeof(s_buf), ADDR_STR "/wpress=%s", str_press);
  sendToServer(s_buf);
}


/// показать текущее значение на дисплее. показываем ddd.ddd. 
/// если захотите учидеть старшие цифры - на mqtt или в подвал
void doDisplayValue()
{
  if(s_displayMode) {
    int16_t val = (s_water_count / 10) & 0x3fff;
    display.showNumberDec(val, true); 
  } else 
    display.clear();
}

//------------------------------------
void setup(void)
{
  display.clear();
  display.setBrightness(0x0f);
  Serial.begin(38400);

  iniInputPin(btn_open_water, BTN_OPEN_WATER);
  iniInputPin(water_cnt, WATER_COUNTER_PIN);
  iniInputPin(btn_display, BTN_DISPLAY);
  iniInputPin(water_is_full, IS_BURREL_FULL);
  iniInputPin(water_is_empty, IS_BURREL_EMPTY);
  digitalWrite(WATER_PRESSURE, HIGH); // pullup
  
  Rtc.Begin();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed s_time_cnt
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 
  Rtc.GetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_epromm, sizeof(s_epromm));
  s_water_count = s_epromm.water_count_rtc;
  sendDeviceConfig("/water");
  sendDeviceConfig("/barrel_empty");
  sendDeviceConfig("/barrel_full");
  sendDeviceConfig("/tap_closed");
  sendDeviceConfig("/tap_open");

  s_tap_fsm.init();
  s_refresh_fsm.init();

  snprintf(s_buf, sizeof(s_buf),  ADDR_STR "/water=%ld", s_water_count);
  sendToServer(s_buf);
}

/*
   Main function, calls the temperatures in a loop.
*/
void loop(void)
{
  static uint32_t s_last_loop;
  uint32_t delay = millis() - s_last_loop;
  // обновляем дисплей 10 раз в сек
  doDisplayValue();
  doTestWater();

  // раз в 50 sec послать давление
  if(delay > DO_MSG_RATE) {
    doWaterPressure();
    // write in eeprom
    if(s_epromm.water_count_rtc != s_water_count) {
      s_epromm.water_count_rtc = s_water_count;
      Rtc.SetMemory(EERPOM_ADDR_COUNT, (uint8_t*)&s_epromm, (uint8_t)sizeof(s_epromm));
    }
    s_refresh_fsm.check();
    s_last_loop = millis();
  }
  s_tap_fsm.loop();
  s_refresh_fsm.loop();
  /// TODO сделать цепочку из rs232 устройств получает в ALtSerial отправляет в Serial и назад
  /// TODO сделать управление командами команды не ко мне форвардить
}
#if 0
class ForwardSerial
{
SoftwareSerial altSerial(8, 9);
char buffer[2][MAX_OUT_BUFF];
  void loop(void)
  {
    // todo получить строку, проверить мне ли, переправить дальше
    // выделить строку для себя от отправить в исполнение
    if(altSerial.available()) {
      char c = altSerial.read();
    }
    if(Serial.available()) {
      char c = altSerial.read();
    }
  }
};
#endif