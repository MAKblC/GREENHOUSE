/*
Не забудьте поправить настройки и адреса устройств в зависимости от комплектации!  
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32_Servo.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>
#include <FastLED.h>                   // конфигурация матрицы // LED matrix configuration   
#include <FastLED_GFX.h>
#include <FastLEDMatrix.h>
#define NUM_LEDS 64                    // количество светодиодов в матрице // number of LEDs 
CRGB leds[NUM_LEDS];                   // определяем матрицу (FastLED библиотека) // defining the matrix (fastLED library)
#define LED_PIN             18         // пин к которому подключена матрица // matrix pin
#define COLOR_ORDER         GRB        // порядок цветов матрицы // color order 
#define CHIPSET             WS2812     // тип светодиодов // LED type            

#define  pump   17                     // пин насоса // pump pin             
#define  wind   16                     // пин вентилятора // cooler pin          

// параметры сети
#define WIFI_SSID "ХХХХХХХХХ"
#define WIFI_PASSWORD "ХХХХХХХХХ"
// токен вашего бота
#define BOT_TOKEN "ХХХХХХ:ХХХХХХХХХХХХХХХХХХХХХХХ"

Servo myservo;
int pos = 1;            // начальная позиция сервомотора // servo start position
int prevangle = 1;      // предыдущий угол сервомотора // previous angle of servo

// Выберите плату расширения вашей сборки (ненужные занесите в комментарии)
#define MGB_D1015 1
//#define MGB_P8 1

const unsigned long BOT_MTBS = 1000; // период обновления сканирования новых сообщений

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280; // Датчик температуры, влажности и атмосферного давления

#include <BH1750.h>
BH1750 lightMeter; // Датчик освещенности

#include "MCP3221.h"
#include "SparkFun_SGP30_Arduino_Library.h"
#include <VEML6075.h>         // добавляем библиотеку датчика ультрафиолета // adding Ultraviolet sensor library 

// Выберите датчик вашей сборки (ненужные занесите в комментарии)
#define MGS_GUVA 1
//#define MGS_CO30 1
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
const float air_value    = 83900.0;
const float water_value  = 45000.0;
const float moisture_0   = 0.0;
const float moisture_100 = 100.0;  // настройка АЦП на плате расширения I2C MGB-D10
#endif
#ifdef MGB_P8
#define SOIL_MOISTURE    34 // A6
#define SOIL_TEMPERATURE 35 // A7
const float air_value    = 1587.0;
const float water_value  = 800.0;
const float moisture_0   = 0.0;
const float moisture_100 = 100.0;
#endif

// ссылка для поста фотографии
String test_photo_url = "https://mgbot.ru/upload/logo-r.png";

// отобразить кнопки перехода на сайт с помощью InlineKeyboard
String keyboardJson1 = "[[{ \"text\" : \"Ваш сайт\", \"url\" : \"https://mgbot.ru\" }],[{ \"text\" : \"Перейти на сайт IoTik.ru\", \"url\" : \"https://www.iotik.ru\" }]]";

void setup()
{
  Serial.begin(115200);
  delay(512);
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  myservo.attach(19);
  Wire.begin();

  pinMode( pump, OUTPUT );
  pinMode( wind, OUTPUT );       // настройка пинов насоса и вентилятора на выход // pump and cooler pins configured on output mode
  digitalWrite(pump, LOW);       // устанавливаем насос и вентилятор изначально выключенными // turn cooler and pump off
  digitalWrite(wind, LOW);

  lightMeter.begin(); // датчик освещенности

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS); // настройки матрицы

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
  ads.begin();    // включем АЦП
#endif
}

// функция обработки новых сообщений
void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    // выполняем действия в зависимости от пришедшей команды
    if ((text == "/sensors") || (text == "sensors")) // измеряем данные
    {
#ifdef MGS_UV60
      veml6075.poll();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();
#endif
#ifdef MGS_GUVA
      float sensorVoltage;
      float sensorValue;
      float UV_index;
      sensorValue = mcp3221.getVoltage();
      sensorVoltage = 1000 * (sensorValue / 4096 * 5.0); // напряжение на АЦП
      UV_index = 370 * sensorVoltage / 200000; // Индекс УФ (эмпирическое измерение)
#endif
#ifdef MGS_CO30
      mySensor.measureAirQuality();
#endif

      float light = lightMeter.readLightLevel();

      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;

#ifdef MGB_D1015
      float adc0 = (float)ads.readADC_SingleEnded(0) * 6.144 * 16;
      float adc1 = (float)ads.readADC_SingleEnded(1) * 6.144 * 16;
      float t1 = (adc1 / 1000); //1023.0 * 5.0) - 0.5) * 100.0;
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
#ifdef MGB_P8
      float adc0 = analogRead(SOIL_MOISTURE);
      float adc1 = analogRead(SOIL_TEMPERATURE);
      float t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0; // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
#endif
      String welcome = "Показания датчиков:\n-------------------------------------------\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      welcome += "☁ Атмосферное давление: " + String(p, 0) + " мм рт.ст.\n";
      welcome += "☀ Освещенность: " + String(light) + " Лк\n\n";
      welcome += "🌱 Температура почвы: " + String(t1, 0) + " °C\n";
      welcome += "🌱 Влажность почвы: " + String(h1, 0) + " %\n\n";
#ifdef MGS_UV60
      welcome += "🅰 Ультрафиолет-А " + String(uva, 0) + " mkWt/cm2\n";
      welcome += "🅱 Ультрафиолет-В: " + String(uvb, 0) + " mkWt/cm2\n";
      welcome += "🔆 Индекс УФ: " + String(uv_index, 1) + " \n";
#endif
#ifdef MGS_GUVA
      welcome += "📊 Уровень УФ: " + String(sensorVoltage, 1) + " mV\n";
      welcome += "🔆 Индекс УФ: " + String(UV_index, 1) + " \n";
#endif
#ifdef MGS_CO30
      welcome += "🌬 Концентрация СО2: " + String(mySensor.CO2) + " ppm\n";
      welcome += "☢ Концентрация ЛОС: " + String(mySensor.TVOC) + " ppb\n";
#endif
      bot.sendMessage(chat_id, welcome, "Markdown");

    }

    if (text == "/photo") { // пост фотографии
      bot.sendPhoto(chat_id, test_photo_url, "а вот и фотка!");
    }

    if ((text == "/pumpon") || (text == "pumpon"))
    {
      digitalWrite(pump, HIGH);
      delay(1000);
      bot.sendMessage(chat_id, "Насос включен на 1 сек", "");
      digitalWrite(pump, LOW);
    }
    if ((text == "/pumpoff") || (text == "pumpoff"))
    {
      digitalWrite(pump, LOW);
      fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
      FastLED.show();
      bot.sendMessage(chat_id, "Насос выключен", "");
    }
    if ((text == "/windon") || (text == "windon"))
    {
      digitalWrite(wind, HIGH);
      bot.sendMessage(chat_id, "Вентилятор включен", "");
    }
    if ((text == "/windoff") || (text == "windoff"))
    {
      digitalWrite(wind, LOW);
      bot.sendMessage(chat_id, "Вентилятор выключен", "");
    }
    if ((text == "/light") || (text == "light"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(255, 255, 255));
      FastLED.show();
      bot.sendMessage(chat_id, "Свет включен", "");
    }
    if ((text == "/off") || (text == "off"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
      FastLED.show();
      bot.sendMessage(chat_id, "Свет выключен", "");
    }
    if ((text == "/color") || (text == "color"))
    {
      fill_solid( leds, NUM_LEDS, CRGB(random(0, 255), random(0, 255), random(0, 255)));
      FastLED.show();
      bot.sendMessage(chat_id, "Включен случайный цвет", "");
    }
    if (text == "/site") // отобразить кнопки в диалоге для перехода на сайт
    {
      bot.sendMessageWithInlineKeyboard(chat_id, "Выберите действие", "", keyboardJson1);
    }
    if (text == "/options") // клавиатура для управления теплицей
    {
      String keyboardJson = "[[\"/light\", \"/off\"],[\"/color\",\"/sensors\"],[\"/pumpon\", \"/pumpoff\",\"/windon\", \"/windoff\"],[\"/open\",\"/close\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }

    if ((text == "/start") || (text == "start") || (text == "/help") || (text == "help")) // команда для вызова помощи
    {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Команды смотрите в меню слева от строки ввода", "");
      String sms = "Команды:\n";
      sms += "/options - пульт управления теплицей\n";
      sms += "/site - перейти на сайт\n";
      sms += "/photo - запостить фото\n";
      sms += "/help - вызвать помощь\n";
      bot.sendMessage(chat_id, sms, "Markdown");
    }

    if (text == "/open")
    {
      myservo.write(100);
      bot.sendMessage(chat_id, "форточка открыта", "");
    }
    if (text == "/close")
    {
      myservo.write(0);
      bot.sendMessage(chat_id, "форточка закрыта", "");
    }
  }
}



void loop() // вызываем функцию обработки сообщений через определенный период
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}
