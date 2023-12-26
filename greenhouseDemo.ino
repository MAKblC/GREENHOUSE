/*
**********************************************************************
  тестовый код для  проверки всех устройств "Умной теплицы" ЙоТик М2
  смотреть в локальной сети по адресу 192.168.4.4
  Ядро esp32 v2.0.1 GyverPortal v3.5.3
  Created by Dmitry Baev
  //********************************************************************
*/

#include <GyverPortal.h>
GyverPortal ui;
// Задаем статический IP-адрес:
IPAddress local_IP(192, 168, 4, 4);
// Задаем IP-адрес сетевого шлюза:
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

#include <EEPROM.h>
#define EEPROM_SIZE 100

#include <Adafruit_ADS1015.h>
#include <Wire.h>
#include <ESP32_Servo.h>  // конфигурация сервомотора // servo configuration

#include <FastLED.h>  // конфигурация матрицы // LED matrix configuration
#include <FastLED_GFX.h>
#include <FastLEDMatrix.h>
#define NUM_LEDS 64      // количество светодиодов в матрице // number of LEDs
CRGB leds[NUM_LEDS];     // определяем матрицу (FastLED библиотека) // defining the matrix (fastLED library)
#define LED_PIN 18       // пин к которому подключена матрица // matrix pin
#define COLOR_ORDER GRB  // порядок цветов матрицы // color order
#define CHIPSET WS2812   // тип светодиодов // LED type

#define pump 17  // пин насоса // pump pin
#define wind 16  // пин вентилятора // cooler pin

Servo myservo;
int pos = 1;        // начальная позиция сервомотора // servo start position
int prevangle = 1;  // предыдущий угол сервомотора // previous angle of servo

#include <BH1750.h>    // добавляем библиотеку датчика освещенности // adding Light intensity sensor library
BH1750 LightSensor_1;  // BH1750

#include <Adafruit_Sensor.h>  // добавляем библиотеку датчика температуры, влажности и давления // adding Temp Hum Bar sensor library
#include <Adafruit_BME280.h>  // BME280
Adafruit_BME280 bme280;       //

#include "MCP3221.h"
#include "SparkFun_SGP30_Arduino_Library.h"
#include <VEML6075.h>

// Выберите плату расширения вашей сборки (ненужные занесите в комментарии)
#define MGB_D1015 1
//#define MGB_P8 1
//----------------------------------------------------------------НАСТРОЙТЕ ТУТ СВОЙ ДАТЧИК--------------------------------------------------------------------------
// Выберите датчик вашей сборки (ненужные занесите в комментарии)
//#define MGS_GUVA 1
#define MGS_CO30 1
//#define MGS_UV60 1

#ifdef MGS_CO30
SGP30 mySensor;
#endif
#ifdef MGS_GUVA
const byte DEV_ADDR = 0x4F;  // 0x5С , 0x4D (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
MCP3221 mcp3221(DEV_ADDR);
#endif
#ifdef MGS_UV60
VEML6075 veml6075;
#endif

#ifdef MGB_D1015
Adafruit_ADS1015 ads(0x48);
const float air_value = 83900.0;
const float water_value = 45000.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;  // настройка АЦП на плате расширения I2C MGB-D10 // I2C MGB-D10 ADC configuration
#endif
#ifdef MGB_P8
#define SOIL_MOISTURE 34     // A6
#define SOIL_TEMPERATURE 35  // A7
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;
#endif

//Переменные для GP
int widowStat = 0;
bool windStat = 0;
bool pumpStat = 0;
int co2 = 0;
int uvIndex = 0;
int temp = 0;
int hum = 0;
int light = 0;
GPcolor colorState;

char* AP_SSID;
int number = 0;
//////////////////////////////////////////НАСТРОЙКИ/CONFIGURATION/////////////////////////////////////////////////////////////////

void build() {
  GP.BUILD_BEGIN(900);  // ширина колонки

  GP.THEME(GP_DARK);
  //GP.THEME(GP_LIGHT);

  GP.UPDATE("co2GH,lightGH,tGH,humGH,uvInd");
  GP_MAKE_BLOCK_TAB(
    "Умная теплица",
    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Форточка",
                             GP_MAKE_BOX(GP_CENTER, GP.SLIDER("window", widowStat, 0, 100);););

                GP_MAKE_BLOCK_THIN_TAB(
                  "Насос",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL("Выкл"); GP.SWITCH("pump", pumpStat); GP.LABEL("Вкл"););););

    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Вентилятор",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL("Выкл"); GP.SWITCH("wind", windStat); GP.LABEL("Вкл");););

                GP_MAKE_BLOCK_THIN_TAB(
                  "Освещение",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL("Выбрать цвет"); GP.COLOR("colorGH", colorState););););

    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Освещенность",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "lightGH"); GP.LABEL("lx");););

#ifdef MGS_UV60
                GP_MAKE_BLOCK_THIN_TAB(
                  "Датчик UV",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "uvInd"); GP.LABEL(" index");););
#endif
#ifdef MGS_GUVA

                GP_MAKE_BLOCK_THIN_TAB(
                  "Датчик UV",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "uvInd"); GP.LABEL(" index");););
#endif
#ifdef MGS_CO30
                GP_MAKE_BLOCK_THIN_TAB(
                  "Датчик CO2",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "co2GH"); GP.LABEL(" ppm\tTVOC: ");););
#endif

    );

    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Температура",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "tGH"); GP.LABEL("°C");););
                GP_MAKE_BLOCK_THIN_TAB(
                  "Влажность",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "humGH"); GP.LABEL("%");););););

  GP.BUILD_END();
}
void action() {
  if (ui.click()) {
    if (ui.clickInt("window", widowStat)) {
      myservo.write(widowStat);
      Serial.println("servo position: ");
      Serial.println(widowStat);
      Serial.println("°");
    }

    if (ui.clickBool("pump", pumpStat)) {
      Serial.println(pumpStat);
      digitalWrite(pump, pumpStat);
    }

    if (ui.clickBool("wind", windStat)) {
      Serial.println(windStat);
      digitalWrite(wind, windStat);
    }

    if (ui.clickColor("colorGH", colorState)) {
      fill_solid(leds, NUM_LEDS, CRGB(colorState.r, colorState.g, colorState.b));  // заполнить всю матрицу выбранным цветом
      FastLED.show();
    }
  }

  if (ui.update()) {
#ifdef MGS_UV60
    if (ui.update("uvInd")) {
      ui.answer(uvIndex);
    }
#endif
#ifdef MGS_GUVA
    if (ui.update("uvInd")) {
      ui.answer(uvIndex);
    }
#endif
#ifdef MGS_CO30
    if (ui.update("co2GH")) {
      mySensor.measureAirQuality();
      ui.answer(co2);
    }
#endif
    if (ui.update("lightGH")) {
      ui.answer(light);
    }
    if (ui.update("tGH")) {
      ui.answer(temp);
    }
    if (ui.update("humGH")) {
      ui.answer(hum);
    }
  }
}

void setup() {
  myservo.attach(19);  // пин сервомотора // servo pin
  Wire.begin();
  pinMode(pump, OUTPUT);
  pinMode(wind, OUTPUT);    // настройка пинов насоса и вентилятора на выход // pump and cooler pins configured on output mode
  digitalWrite(pump, LOW);  // устанавливаем насос и вентилятор изначально выключенными // turn cooler and pump off
  digitalWrite(wind, LOW);

  Serial.begin(115200);
  delay(512);

  EEPROM.begin(EEPROM_SIZE);
  checkNamber();

  // Инициируем WiFi
  WiFi.mode(WIFI_AP);
  Serial.print("Настоить статический IP: ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Удалось" : "Не удалось!");
  WiFi.softAP(AP_SSID);
  Serial.print("IP для подключения: ");
  Serial.println(WiFi.softAPIP());

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);  // конфигурация матрицы // matrix configuration

  LightSensor_1.begin();  // запуск датчика освещенности // turn the light intensity sensor on

  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }
  
#ifdef MGS_UV60
  if (!veml6075.begin())
    Serial.println("VEML6075 not found!");
#endif
#ifdef MGS_GUVA
  mcp3221.setVinput(VOLTAGE_INPUT_5V);
#endif
#ifdef MGS_CO30
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
  }
  mySensor.initAirQuality();
#endif

#ifdef MGB_D1015
  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();  // включем АЦП // turn the ADC on
#endif

  // подключаем конструктор и запускаем
  ui.attachBuild(build);
  ui.attach(action);
  ui.start();
  Serial.println("Веб страница запущена");
}

void checkNamber() {
  EEPROM.get(95, number);
  Serial.println(number);
  if (number == -1) {
    number = random(10000);
    EEPROM.put(95, number);
    EEPROM.commit();
  }

  String n = "GreenHouse_№-";
  n = n + String(number);

  AP_SSID = strcpy(new char[n.length() + 1], n.c_str());
}

static uint32_t tmr;
void loop() {
  ui.tick();

  if (millis() - tmr >= 500) {
    Serial.println("Wifi: " + String(AP_SSID) + " / Адрес: " + WiFi.softAPIP().toString());
    tmr = millis();
    light = LightSensor_1.readLightLevel();
    // Вывод измеренных значений в терминал
    Serial.println(String(light) + " lx");
    temp = bme280.readTemperature();
    Serial.println("Air temperature = " + String(temp) + " *C");
    hum = bme280.readHumidity();
    Serial.println("Air humidity = " + String(hum) + " %");
#ifdef MGS_UV60
    veml6075.poll();
    uvIndex = veml6075.getUVIndex();
    Serial.println("UV INDEX = " + String(uvIndex) + "   ");
#endif
#ifdef MGS_GUVA
    float sensorVoltage;
    float sensorValue;
    sensorValue = mcp3221.getVoltage();
    sensorVoltage = 1000 * (sensorValue / 4096 * 5.0);  // напряжение на АЦП
    uvIndex = 370 * sensorVoltage / 200000;             // Индекс УФ (эмпирическое измерение)
    Serial.println("UV index = " + String(uvIndex, 1));
#endif
#ifdef MGS_CO30
    mySensor.measureAirQuality();
    co2 = mySensor.CO2;
    Serial.println("CO2 = " + String(co2) + " ppm\tTVOC: ");
#endif
  }
}
