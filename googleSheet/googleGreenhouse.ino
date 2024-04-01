#include <WiFi.h>
#include <HTTPClient.h>
/********************************************************************************/
//Логин/пароль WiFI, ID скрипта
const char* ssid = "XXXXXXX";
const char* password = "XXXXXXXXXX";
String GOOGLE_SCRIPT_ID = "AKfXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXD0EE";  // Google Script ID
const int sendInterval = 5000;                                                                         // период запроса
WiFiClientSecure client;
#include "time.h"
const char* ntpServer = "pool.ntp.org";  // отсюда считается текущие дата/время
const long gmtOffset_sec = 10800;        // отклонение от Гринвича в 3 часа
const int daylightOffset_sec = 0;
/********************************************************************************/
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <ESP32_Servo.h>
#include <FastLED.h>
#define NUM_LEDS 64
CRGB leds[NUM_LEDS];
#define LED_PIN 18
#define COLOR_ORDER GRB
#define CHIPSET WS2812
#define pump 17
#define wind 16
Servo myservo;
int pos = 1;
int prevangle = 1;

#include <BH1750.h>
BH1750 lightMeter;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280;

#include "MCP3221.h"
const byte DEV_ADDR = 0x4F;  // 0x5С , 0x4D
MCP3221 mcp3221(DEV_ADDR);

Adafruit_ADS1015 ads(0x48);
const float air_value = 83900.0;
const float water_value = 45000.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

void setup() {
  Serial.begin(115200);
  myservo.attach(19);
  Wire.begin();
  pinMode(pump, OUTPUT);
  pinMode(wind, OUTPUT);
  digitalWrite(pump, LOW);
  digitalWrite(wind, LOW);
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

  lightMeter.begin();

  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }

  mcp3221.setVinput(VOLTAGE_INPUT_5V);

  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Старт");
  Serial.print("Подсоединяюсь");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Готово!");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  sheet_read();
  sheet_write();
  delay(sendInterval);
}

// чтение ячеек для управления исполнительными устройствами
void sheet_read(void) {
  HTTPClient http;
  String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?read";
  //   Serial.print(url);
  http.begin(url.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();  // по этому запросу выполнится doGet() в скрипте
  String payload;
  if (httpCode > 0) {  // Если не ошибка в запросе
    payload = http.getString();
    // Serial.println(httpCode);
    //Serial.println(payload);
    // Serial.println(payload.length());
    decoder(payload);  // расшифровываем пришедшее сообщение
  } else {
    Serial.println("Ошибка при HTTP запросе");
  }
  http.end();
}

// запись данных с датчиков
void sheet_write(void) {
  float sensorValue = mcp3221.getVoltage();
  float sensorVoltage = 1000 * (sensorValue / 4096 * 5.0);
  float UV_index = 370 * sensorVoltage / 200000;
  float t = bme280.readTemperature();
  float h = bme280.readHumidity();
  float p = bme280.readPressure() / 133.3F;
  float l = lightMeter.readLightLevel();
  float adc0 = (float)ads.readADC_SingleEnded(0) * 6.144 * 16;
  float adc1 = (float)ads.readADC_SingleEnded(1) * 6.144 * 16;
  float t1 = (adc1 / 1000);
  float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);

  if (WiFi.status() == WL_CONNECTED) {
    static bool flag = false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %m %Y %H:%M:%S", &timeinfo);  // получение даты и времени
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    HTTPClient http;
    http.begin("https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // собираем сообщение для публикации
    String message = "date=" + asString;
    message += "&light=" + String(l, 0);
    message += "&temp=" + String(t, 1);
    message += "&hum=" + String(h, 0);
    message += "&press=" + String(p, 0);
    message += "&tempH=" + String(t1, 1);
    message += "&humH=" + String(h1, 0);
    message += "&uvi=" + String(UV_index, 0);
    // данный запрос вызывает функцию doPost() в скрипте
    int httpResponseCode = http.POST(message);
    //Serial.print("HTTP Status Code: ");
    //Serial.println(httpResponseCode);
    //---------------------------------------------------------------------
    String payload;
    if (httpResponseCode > 0) {
      payload = http.getString();
      //  Serial.println("Payload: " + payload);
    }
    //---------------------------------------------------------------------
    http.end();
  }
}

// функция расшифровки сообщения и управления устройствами
void decoder(String payload) {
  String data[6];  // создаем массив из 6-ти строк
  int k = 0;
  // перебираем каждый символ сообщения
  for (int i = 0; i < payload.length(); i++) {
    // если это не запятая
    if (payload[i] != ',') {
      // добавляем в строку
      data[k] += payload[i];
    } else {
      // если запятая, то переходим к следующей строке массива и пропускаем запятую
      k++;
      continue;
    }
  }
  Serial.println(data[0]);
  Serial.println(data[1]);
  Serial.println(data[2]);
  Serial.println(data[3]);
  Serial.println(data[4]);
  Serial.println(data[5]);
  // управление ИУ в зависимости от пришедших данных
  if (data[0] == "on") {
    digitalWrite(wind, HIGH);
  } else {
    digitalWrite(wind, LOW);
  }
  if (data[1] == "on") {
    digitalWrite(pump, HIGH);
  } else {
    digitalWrite(pump, LOW);
  }
  myservo.write(data[2].toInt());
  fill_solid(leds, NUM_LEDS, CRGB(data[3].toInt(), data[4].toInt(), data[5].toInt()));  // заполнить всю матрицу выбранным цветом
  FastLED.show();
}
