#include <Arduino.h>
#include <Wire.h>
#include <Bounce2.h>
#include <RtcDS1307.h>
#include <TM1637Display.h>

#include "ardudev.h"

/// открыть закрыть кран раз в 2 недели ночью
#define DAYS_REFRESHING_TAP (3600L*24*14)
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

//------------------------------------------------------------
class SerialComm {
char s_buf[MAX_OUT_BUFF];
public:
  ////////////////////////////////////////////////////////
  void sendToServer()
  {
  char buf[4];
    uint8_t sum = 0;
    for(const char *str = s_buf; *str; ++str)
      sum += *str;
    sum = 0xff - sum;
    snprintf(buf, sizeof(buf), ";%02x", sum);
    Serial.println(buf);
  }  
  void init()
  {
    Serial.begin(38400);
  }
  template <typename H, typename... T>
  void send_srv(H fmt, T... t)
  {
    snprintf(s_buf, sizeof(s_buf), fmt, t...);
    sendToServer();
  }
  void sendDate(const char *str, RtcDateTime &d)
  {
    char datestring[20]; 
    snprintf_P(datestring, 
        sizeof(datestring),
        PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
        d.Month(), 
        d.Day(),
        d.Year(),
        d.Hour(),
        d.Minute(),
        d.Second() );
    send_srv(ADDR_STR "/%s=%s", str, datestring);
  }
  void sendDeviceConfig(const char *dev)
  {
      send_srv(LOG_MESSAGE SENSOR_NAME DEVICE_NO "%s" LOG_MESSAGE_END, dev);
  }

} s_comm;


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


bool isButtonChanged(Bounce &bounce, const char *title)
{
  boolean changed = bounce.update(); 
  if ( changed ) {
      uint8_t value = !bounce.read();
      s_comm.send_srv(ADDR_STR "%s=%d", title, value);
    }
    return changed;  
}


struct ICallback {
  virtual void done(uint8_t error) = 0;
};
/////////////////////////////////////////////////////////
/**
 * fsm для закрывания крана
 */
class CTapFSM {
  // состояния fsm
  enum eTapState {
    eTapNone = 100,
    eTapOpening,
    eTapClosing,
    eTapStop
  };
  int m_fsm_tap_state = eTapNone;
  uint32_t m_fsm_timeout = 0;
  ICallback *m_callback = nullptr;

  public:
  // состояния крана
  enum eState {
    eTapWorking = 0,   // промежуточное
    eTapOpen, 
    eTapClose,
  } m_tap_state;
  bool m_opened, m_closed;

  // состояние крана по датчикам
  enum eRealState {
    eUndefined = 0, // промежуточное
    eOpened = 0x1,
    eClosed = 0x2,
    eBroken = 0x4 // поломано или оборваны провода
  };

  // состояния ошибок - кран был поставлен на открытие/закрытие но концевик не сработал
  enum eError {
    eOk,
    eErrorOpen,
    eErrorClose
  } m_error;

  /**
   * перевести кран в другое состояние
   */
  void change(eState s, struct ICallback *c = nullptr) {
      m_fsm_tap_state = s;
      updateTapDateEPROM();
      s_comm.send_srv(ADDR_STR "/wtapcnmd=%d", s);
      m_callback = c;
      m_error = eOk;
      m_tap_state = eTapWorking;
      m_fsm_timeout = millis();
      // команды открытия закрытия крана - работают только если предидущая работа завершена
      switch (s)
      {
      case eTapClose:
        digitalWrite(OPEN_TAP_CMD, HIGH);
        digitalWrite(CLOSE_TAP_CMD, LOW);      
        m_fsm_tap_state = eTapClosing;
        break;

      case eTapOpen:
        digitalWrite(OPEN_TAP_CMD, LOW);
        digitalWrite(CLOSE_TAP_CMD, HIGH);      
        m_fsm_timeout = millis();
        m_fsm_tap_state = eTapOpening;
        break;    
      default:
        break;
      }
  };

  /**
   * инициировать состояние, 
   * прочитать датчики
   * отправить состояние на сервер
   */
  void init() {
    iniInputPin(isTapOpen, IS_TAP_OPEN);
    iniInputPin(isTapClosed, IS_TAP_CLOSE);
    
    pinMode(OPEN_TAP_CMD, OUTPUT);
    pinMode(CLOSE_TAP_CMD, OUTPUT);
    pinMode(WATER_PRESSURE, INPUT);
    digitalWrite(OPEN_TAP_CMD, HIGH);
    digitalWrite(CLOSE_TAP_CMD, HIGH);   
    m_error = eOk;
    if(getRealState() == eUndefined)      {
      change(water_is_full.read()?  eTapOpen : eTapClose);
      m_tap_state = eTapWorking;
    } else 
      m_tap_state = m_opened ? eTapOpen: eTapClose;
    m_callback = nullptr;
  }

  /**
   * вернуть состояние кранов
   * отправить на сервера состояние
   */
  eRealState getRealState() {
    m_opened = !isTapClosed.read();
    s_comm.send_srv(ADDR_STR "/tap_closed=%d", m_opened);
    m_closed = !isTapOpen.read();
    s_comm.send_srv(ADDR_STR "/tap_open=%d", m_closed);
    static eRealState tab_recode[2][2] = {{eUndefined, eOpened}, {eClosed, eBroken}};
    return tab_recode[m_closed][m_opened];
  }

  /**
   * собственно рабочй цикл fsm
  // послать ошибку серверу если по коменде датчики не 
  //      показали изменения - кран закис или неисправность
   */
  void loop() {
    switch (m_fsm_tap_state) {
      case eTapOpening: {
        uint32_t delay = millis() - m_fsm_timeout;
        // проверить состояние открыто 
        m_opened = !isTapOpen.read();
        if(m_opened || delay > TAP_TIMEOUT) {
          // должно быть состояние открыто - иначе ошибка
          if(!m_opened) {
            m_error = eErrorOpen;
            s_comm.send_srv(ADDR_STR "/tap_error=%d", m_error);
          }
          m_tap_state = eTapOpen;
          m_fsm_tap_state = eTapStop;
          if(m_callback != nullptr) {
            m_callback->done(m_error);
          }
        }
      }
      break;

      case eTapClosing: {
        uint32_t delay = millis() - m_fsm_timeout;
        //  проверить состояние закрыто
        m_closed = !isTapClosed.read();
        if(m_closed || delay > TAP_TIMEOUT) {
          //  должно быть состояние закрыто - иначе ошибка
          if(!m_closed) {
            m_error = eErrorClose;
            s_comm.send_srv(ADDR_STR "/tap_error=%d", m_error);
          }
          m_tap_state = eTapClose;
          m_fsm_tap_state = eTapStop;
          if(m_callback != nullptr)
            m_callback->done(m_error);
        }
      }
      break;

      case eTapStop:
        digitalWrite(OPEN_TAP_CMD, HIGH);
        digitalWrite(CLOSE_TAP_CMD, HIGH);    
        m_fsm_tap_state = eTapNone;
        m_callback = nullptr;
        break;

      case eTapNone:
      default:
        break;
    }
  }
} s_tap_fsm;

//------------------------------------------------------------
/**
 * fsm для открывания и закрывания крана раз в неделю
 * заносим в eprom последнюю дату освежения
 * не отслеживаю здесь таймауты - callback
 */ 
class CRefreshTapFSM : public ICallback
{
  enum eState {
    eNone,
    eChange,  // это не собственно открытие а фаза 1
    eBack     // фаза 2
  } s_fsm_state;
  // вернуть кран в это состояние
  CTapFSM::eState m_start_state;  

  void toggle_tap(bool from)
  {
    if (from)
      s_tap_fsm.change(CTapFSM::eTapClose, this);
    else
      s_tap_fsm.change(CTapFSM::eTapOpen, this);
    s_comm.send_srv(ADDR_STR "/tap_refresh=%d", from);        
  }

  public:

  // провернуть, от текущего состояния по датчикам
  void change() {
    s_fsm_state = eChange;    
    CTapFSM::eRealState state = s_tap_fsm.getRealState();
    m_start_state = (state == CTapFSM::eUndefined || state == CTapFSM::eOpened) ? 
      CTapFSM::eTapOpen: CTapFSM::eTapClose;
    toggle_tap(m_start_state == CTapFSM::eTapOpen);
  }

  // callback - по завершению операции
  virtual void done(uint8_t error) override {
    switch (s_fsm_state)
    {
      case eChange:
        if(error != 0)
          s_tap_fsm.getRealState();        
        s_fsm_state = eBack;
        toggle_tap(m_start_state == CTapFSM::eTapClose);
        break;
      case eBack:
        if(error != 0) 
          s_tap_fsm.getRealState();        
        s_fsm_state = eNone;
        break;  
      default:
        break;
    }
  }

  void init() {
      s_fsm_state = eNone;
  }
  /**
   * проверить надо ли поварачивать кран
   */
  void check() {
      // сравнить время открывания и текущее
      RtcDateTime cur = Rtc.GetDateTime();
      cur -= DAYS_REFRESHING_TAP;
      if(cur > s_epromm.last_water_tap_time)
        change();
  }
} s_refresh_fsm;

//-----------------------------------------------------------------
class Burrel : public ICallback {
  public:
  bool m_full, m_empty;

  virtual void done(uint8_t error) override 
  {

  }

  void check() {
    m_empty = !water_is_empty.read();
    if(isButtonChanged(water_is_empty, "/barrel_empty")) {
        if(m_empty  && s_automatic_open_enabled)
          s_tap_fsm.change(CTapFSM::eTapOpen, this);
      }
    // при появлении 1цы дать команду закрыть кран
    m_full = !water_is_full.read();
    if(isButtonChanged(water_is_full, "/barrel_full")) {
      if(m_full)
        s_tap_fsm.change(CTapFSM::eTapClose, this);
    }

  }
} s_burrel;
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
    s_comm.send_srv(ADDR_STR "/water=%ld", s_water_count);
  }

  // переключить экранчик
  if(checkButtonChanged(btn_display)) {
      s_displayMode ^= true;
  }

  // запустить FSM на изменение ветниля
  if(checkButtonChanged(btn_open_water)) {
    s_tap_fsm.change(s_tap_fsm.m_tap_state == CTapFSM::eTapClose ? 
      CTapFSM::eTapOpen: CTapFSM::eTapClose);
  }
  // при появлении 1цы дать команду открыть кран - водзможно сделать блокировку этого
  if(isButtonChanged(water_is_empty, "/barrel_empty") &&
    !isTapClosed.read() &&  !water_is_empty.read() && s_automatic_open_enabled)
      s_tap_fsm.change(CTapFSM::eTapOpen);
  // при появлении 1цы дать команду закрыть кран
  if(isButtonChanged(water_is_full, "/barrel_full") && !water_is_full.read())
    s_tap_fsm.change(CTapFSM::eTapClose);
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
  s_comm.send_srv(ADDR_STR "/wpress=%s", str_press);
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
  s_comm.init();
  
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
  s_comm.send_srv( ADDR_STR "/water=%ld", s_water_count);

  // вывести текущее время, вывести время последнего поворота
  RtcDateTime now = Rtc.GetDateTime();
  s_comm.sendDate("rtc_time", now);
  s_comm.sendDate("refresh_time", s_epromm.last_water_tap_time);

  s_comm.sendDeviceConfig("/water");
  s_comm.sendDeviceConfig("/barrel_empty");
  s_comm.sendDeviceConfig("/barrel_full");
  s_comm.sendDeviceConfig("/tap_closed");
  s_comm.sendDeviceConfig("/tap_open");

  s_tap_fsm.init();
  s_refresh_fsm.init();
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
  /// TODO сделать цепочку из rs232 устройств получает в ALtSerial отправляет в Serial и назад
  /// TODO cmd занести в NVRAM значение для воды из serial, 
  /// TODO cmd занести в NVRAM текущее время  из serial
  /// TODO сделать управление командами команды не ко мне форвардить
}
#if 0
class ForwardSerial
{
SoftwareSerial altSerial(8, 9);
char buffer[2][MAX_OUT_BUFF];
  void loop(void)
  {
    // TODO получить строку, проверить мне ли, переправить дальше
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