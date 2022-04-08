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

const unsigned long BOT_MTBS = 1000; // период обновления сканирования новых сообщений 

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; 

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280; // Датчик температуры, влажности и атмосферного давления

#include <BH1750FVI.h>
BH1750FVI bh1750; // Датчик освещенности

#include <VEML6075.h>         // добавляем библиотеку датчика ультрафиолета // adding Ultraviolet sensor library        
VEML6075 veml6075;            // VEML6075

Adafruit_ADS1015 ads(0x48);
const float air_value    = 83900.0;
const float water_value  = 45000.0;
const float moisture_0   = 0.0;
const float moisture_100 = 100.0;  // настройка АЦП на плате расширения I2C MGB-D10 // I2C MGB-D10 ADC configuration

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

  bh1750.begin();
  bh1750.setMode(Continuously_High_Resolution_Mode); // датчик освещенности

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS); // настройки матрицы

  bool bme_status = bme280.begin(); // датчик Т, В и Д
  if (!bme_status)
    Serial.println("Could not find a valid BME280 sensor, check wiring!");

  if (!veml6075.begin())
    Serial.println("VEML6075 not found!");   // проверка работы датчика ультрафиолета  // checking the UV sensor

  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();    // включем АЦП // turn the ADC on
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
      veml6075.poll();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();

      float light = bh1750.getAmbientLight();

      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;

      float adc0 = (float)ads.readADC_SingleEnded(0) * 6.144 * 16;
      float adc1 = (float)ads.readADC_SingleEnded(1) * 6.144 * 16;

      float t1 = (adc1 / 1000); //1023.0 * 5.0) - 0.5) * 100.0;
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);

      String welcome = "Показания датчиков:\n";
      welcome += "Temp: " + String(t, 1) + " C\n";
      welcome += "Hum: " + String(h, 0) + " %\n";
      welcome += "Press: " + String(p, 0) + " hPa\n";
      welcome += "Light: " + String(light, 0) + " Lx\n";
      welcome += "Soil temp: " + String(t1, 0) + " C\n";
      welcome += "Soil hum: " + String(h1, 0) + " %\n";
      welcome += "UVA: " + String(uva, 0) + " mkWt/cm2\n";
      welcome += "UVB: " + String(uvb, 0) + " mkWt/cm2\n";
      welcome += "UV Index: " + String(uv_index, 1) + " \n";
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
      fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
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