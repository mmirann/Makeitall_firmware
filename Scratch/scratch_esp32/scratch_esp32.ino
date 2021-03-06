#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "dht.h"


// BLUETOOTH 통신을 위해 센서 동작마다 UUID를 설정
// 추가 UUID가 필요한 경우: https://www.uuidgenerator.net/ 접속
//주석  

#define SET_SERVICE_UUID                      "c005"
#define SET_NEOPIXEL_UUID                     "d895d61e-902e-11eb-a8b3-0242ac130003"
#define SET_PIN_UUID                          "d895d7cc-902e-11eb-a8b3-0242ac130003"
#define SET_BUZZER_UUID                       "d895d952-902e-11eb-a8b3-0242ac130003"
#define SET_LCD_UUID                          "d895dc2c-902e-11eb-a8b3-0242ac130003"
#define SET_OLED_UUID                         "d895dd30-902e-11eb-a8b3-0242ac130003"
#define MISC_CHARACTERISTIC_STATUS_INFO_UUID  "34443c3b-3356-11e9-b210-d663bd873d93"

#define GET_SERVICE_UUID     "c006"
#define SET_DIGITAL_UUID     "d895dea2-902e-11eb-a8b3-0242ac130003"
#define SET_ANALOG_UUID      "7afd83e8-a335-11eb-bcbc-0242ac130002"
#define SET_ULTRASONIC_UUID  "7afd7d76-a335-11eb-bcbc-0242ac130002"
#define SET_DHT_UUID         "7afd84b0-a335-11eb-bcbc-0242ac130002"
#define SET_GYRO_UUID        "7afd8564-a335-11eb-bcbc-0242ac130002"
#define SET_TOUCH_UUID       "7afd7f88-a335-11eb-bcbc-0242ac130002"
#define SET_BUTTON_UUID      "7afd8078-a335-11eb-bcbc-0242ac130002"
#define SET_BUTTON_PU_UUID   "7afd8140-a335-11eb-bcbc-0242ac130002"
#define GET_BUTTON_UUID      "d895d704-902e-11eb-a8b3-0242ac130003"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

char ble_mac_addr[6] = {0, 0, 0, 0, 0, 0};

uint8_t value_misc_status_info[4] = {0, 0, 0, 0};

// get 동작 후 값을 보내기 위하여 센서별로 저장하는 변수 
uint8_t digital_value = 0;
uint16_t analog_value = 0;
float dht_value[2] = {0, 0};
float ultrasonic_value = 0;
uint16_t gyro_value[3] = {0, 0, 0};
int touch_value = 0;
int button_value = 0;
int buttonpu_value = 0;
float gyroX, gyroY, gyroZ;

uint8_t value_sensor_all_data[30] = {0, };

uint8_t status_led_count = 0;
uint8_t status_update_info_count = 0;
uint8_t status_update_sensors_count = 0;
uint8_t status_update_all_count = 0;
bool device_connected = false;

bool status_text_displayed = false;

BLECharacteristic *mCharMiscStatusInfo = NULL;
BLECharacteristic *mCharSensorAllData = NULL;

//********************************************************


//********************************************************
//SENSOR

//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
//NEOPIXEL
Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, 2, NEO_GRB + NEO_KHZ800);
//OLED
Adafruit_SSD1306 display_oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//아날로그 채널 분배 
int analogChannel[12] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13};
int analog_cnt = 0;
int servoChannel[4] = {6, 7, 14, 15};
int servo_cnt = 0;

//OLED 디폴트 셋팅 - 폰트 크기, 색상
int oled_text_size = 1;
int oled_text_color = 0;
bool isStartOled = false;

// Float 변수를 uint8_t로 변환하기 위한 union 
union
{
    uint8_t intVal[4];
    float floatVal;
} val;


class MyBLEServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      device_connected = true;
    }

    void onDisconnect(BLEServer* pServer) {
      device_connected = false;
    }
};

/*
  0. init: cmd, pin, led_num
  1. brightness: cmd, pin, brightness
  2. no_color: cmd, pin, r, g, b, num
  3. all_color: cmd, pin, r, g, b
*/
class MyMiscSetNEOCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      int cmd = value[0];
      int pin = value[1];
      int led_num = 4;
      strip.begin();

      if (cmd == 0) { //init
        led_num = value[2];
        strip.updateLength(led_num);
        strip.setPin(pin);

      } else if (cmd == 1) { //brightness
        int brightness = value[2];
        strip.setBrightness(brightness);

      } else {
        int r = value[2];
        int g = value[3];
        int b = value[4];

        if (cmd == 2) { // num color
          int num = value[5];
          strip.setPixelColor(num, r, g, b);
          strip.show();
          strip.show();

        } else if (cmd == 3) { //all color

          for (int i = 0; i < led_num; i++)
            strip.setPixelColor(i, r, g, b);
          strip.show();
          strip.show();
        }
      }
    Serial.println("NEOPIXEL");
    }
};

// cmd pin value
// analog 채널 분배를 위하여 cnt를 증가시키며 채널을 할당함
// 서보모터와 아날로그는 해상도가 달라 동일한 타이머를 가진 채널 사용 불가능
// 따라서 analog_channel와 servo_channel를 분리 
class MyMiscSetPINCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      int cmd = value[0];
      int pin = value[1];
      int _value = value[2];

      if (cmd == 0) { //digital output
        pinMode(pin, OUTPUT);
        digitalWrite(pin, _value);

      } else if (cmd == 1) { //pwm output
        ledcSetup(analogChannel[analog_cnt], 5000, 8);
        ledcAttachPin(pin, analogChannel[analog_cnt]);
        ledcWrite(analogChannel[analog_cnt], _value);

        analog_cnt >= 12 ? analog_cnt = 0 : analog_cnt++;

      } else if (cmd == 2) { //servo pwm
        int duty = _value * 18.2 + 3277;
        ledcSetup(servoChannel[servo_cnt], 50, 16);
        ledcAttachPin(pin, servoChannel[servo_cnt]);
        ledcWrite(servoChannel[servo_cnt], duty);

        servo_cnt >= 4 ? servo_cnt = 0 : servo_cnt++;

      }
    }
};

// 부저와 아날로그는 동일한 해상도를 갖기 때문에 같은 채널 사용 가능 
class MyMiscSetBUZZERCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      int pin = value[0];
      int note = value[1];
      int beats = value[2];
      int duration = 2000 / beats;
      unsigned long beepTime = 0;

      ledcSetup(analogChannel[analog_cnt], 5000, 8);
      ledcAttachPin(pin, analogChannel[analog_cnt]);

      ledcWrite(analogChannel[analog_cnt], 0);
      ledcWriteTone(analogChannel[analog_cnt], note);
      delay(beats * 1000);
      ledcWrite(analogChannel[analog_cnt], 0);


      analog_cnt >= 12 ? analog_cnt = 0 : analog_cnt++;

    }
};

// [LCD] 0: INIT, 1: CLEAR, 2: PRINT
class MyMiscSetLCDCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      String str = "";
      int cmd = value[0];

      if (cmd == 0) {
        int column = value[1];
        int row = value[2];
        lcd = LiquidCrystal_I2C(0x27, column, row);
        lcd.init();
        lcd.backlight();
        lcd.clear();

      } else if (cmd == 1) {
        lcd.clear();

      } else if (cmd == 2) {
        int column = value[1];
        int row = value[2];
        int textLen = value[3];
        for (int i = 4; i < 4 + textLen; i++) {
          str += (char)value[i];
        }
        lcd.setCursor(column, row);
        lcd.print(str);
        Serial.print(str);

      }
    }
};

// 0: print, 1: clear
// 2: text theme(size, color)
// 3: text (column, row, textLen, str)
class MyMiscSetOLEDCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      String str = "";
      int cmd = value[0];
      if (!isStartOled)
      {
        display_oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        display_oled.clearDisplay();
        isStartOled = true;
      }

      if (cmd == 0) {
        display_oled.display();
      } else if (cmd == 1) {
        display_oled.clearDisplay();
        display_oled.display();

      } else if (cmd == 2) {
        oled_text_size = value[1];
        oled_text_color = value[2];

      } else if (cmd == 3) {
        int column = value[1];
        int row = value[2];
        int textLen = value[3];
        Serial.println(column);
        Serial.println(row);
        Serial.println(textLen);
        for (int i = 4; i < 4 + textLen; i++) {
          str += (char)value[i];
        }
        Serial.println(str);

        display_oled.setTextSize(oled_text_size);
        if (oled_text_color == 0)
          display_oled.setTextColor(WHITE);
        else if (oled_text_color == 1)
          display_oled.setTextColor(BLACK, WHITE);
        display_oled.setCursor(column, row);
        display_oled.println(str);
        Serial.println(str);

      }
    }
};

class MyMiscSetDigitalCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
    
        pinMode(value[1], INPUT);
        digital_value = digitalRead(value[1]);
    }
};

class MyMiscSetAnalogCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      int cmd = value[0];

        analog_value = analogRead(value[1]);
        Serial.println(analog_value);
    }
  
};

class MyMiscSetUltrasonicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      int trigPin=value[1];
      int echoPin=value[2];
       pinMode(trigPin, OUTPUT);
        pinMode(echoPin, INPUT);

        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite( echoPin, LOW);

        ultrasonic_value = pulseIn( echoPin, HIGH, 30000) / 29.0 / 2.0;
        Serial.println(ultrasonic_value);
    }

};

class MyMiscSetDhtCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
     
        dht myDHT11;
        myDHT11.read11(value[1]);
        dht_value[0] = myDHT11.temperature;
        dht_value[1] = myDHT11.humidity;
        Serial.println(dht_value[0]);
        Serial.println(dht_value[1]);

    }

};

class MyMiscSetGyroCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
     
        Wire.beginTransmission(0b1101000); //I2C address of the MPU
        Wire.write(0x43); //Starting register for Gyro Readings
        Wire.endTransmission();
        Wire.requestFrom(0b1101000, 6); //Request Gyro Registers (43 - 48)
        while (Wire.available() < 6);
        gyroX = Wire.read() << 8 | Wire.read(); //Store first two bytes into accelX
        gyroY = Wire.read() << 8 | Wire.read(); //Store middle two bytes into accelY
        gyroZ = Wire.read() << 8 | Wire.read(); //Store last two bytes into accelZ
        gyroX = gyroX / 131.0;
        gyroY = gyroY / 131.0;
        gyroZ = gyroZ / 131.0;
    }

};
class MyMiscSetTouchCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
        int touchPin=value[1];
        switch (touchPin)
        {
          case 4:
            touch_value = touchRead(T0);
            break;
          case 13:
           touch_value  = touchRead(T4);
            break;
          case 14:
            touch_value  = touchRead(T6);
            break;
          case 32:
            touch_value  = touchRead(T9);
            break;
          case 33:
           touch_value  = touchRead(T8);
            break;
        }
        }
  
};
class MyMiscSetButtonCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
     
        pinMode(value[1], INPUT);}
    
};
class MyMiscSetbutton_puCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
   
        pinMode(value[1], INPUT_PULLUP);
        buttonpu_value = !digitalRead(value[1]);
    }
};


void setupMPU()
{
  Wire.beginTransmission(0b1101000); //This is the I2C address of the MPU (b1101000/b1101001 for AC0 low/high datasheet sec. 9.2)
  Wire.write(0x6B);                  //Accessing the register 6B - Power Management (Sec. 4.28)
  Wire.write(0b00000000);            //Setting SLEEP register to 0. (Required; see Note on p. 9)
  Wire.endTransmission();
  Wire.beginTransmission(0b1101000); //I2C address of the MPU
  Wire.write(0x1B);                  //Accessing the register 1B - Gyroscope Configuration (Sec. 4.4)
  Wire.write(0x00000000);            //Setting the gyro to full scale +/- 250deg./s
  Wire.endTransmission();
  Wire.beginTransmission(0b1101000); //I2C address of the MPU
  Wire.write(0x1C);                  //Accessing the register 1C - Acccelerometer Configuration (Sec. 4.5)
  Wire.write(0b00000000);            //Setting the accel to +/- 2g
  Wire.endTransmission();
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //Serial 모니터의 보드레이트를 동일하게 설정 시에, 시리얼 창 확인 가능 

  //************lcd**************
  lcd.init();                      // initialize the lcd
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
  lcd.clear();
  //*****************************

  Serial.println("===============\nStarting BLE work!");

  BLEDevice::init("JIKKO BOARD");
  BLEServer *mServer = BLEDevice::createServer();
  mServer->setCallbacks(new MyBLEServerCallbacks());

  BLEAddress addr = BLEDevice::getAddress();
  memcpy(ble_mac_addr, *addr.getNative(), 6);

  //************************************************


  // 스크래치로부터 블루투스 UUID가 도착하면 위에서 선언한 해당하는 콜백함수를 실행함 
  // Misc Service
  BLEService *mServiceMisc = mServer->createService(BLEUUID(SET_SERVICE_UUID), 40);

  // SET PIN
  BLECharacteristic *mCharMiscSetPIN = mServiceMisc->createCharacteristic(
                                         SET_PIN_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetPIN = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetPIN->setValue("SET PIN WITH CMD");
  mCharMiscSetPIN->addDescriptor(mDescMiscSetPIN);
  mCharMiscSetPIN->setCallbacks(new MyMiscSetPINCallbacks());

  // SET NEO PIXEL
  BLECharacteristic *mCharMiscSetNEO = mServiceMisc->createCharacteristic(
                                         SET_NEOPIXEL_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetNEO = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetNEO->setValue("SET NEOPIXEL WITH CMD");
  mCharMiscSetNEO->addDescriptor(mDescMiscSetNEO);
  mCharMiscSetNEO->setCallbacks(new MyMiscSetNEOCallbacks());

  BLECharacteristic *mCharMiscSetBUZZER = mServiceMisc->createCharacteristic(
      SET_BUZZER_UUID,
      BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetBUZZER = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetBUZZER->setValue("SET BUZZER WITH CMD");
  mCharMiscSetBUZZER->addDescriptor(mDescMiscSetBUZZER);
  mCharMiscSetBUZZER->setCallbacks(new MyMiscSetBUZZERCallbacks());

  BLECharacteristic *mCharMiscSetLCD = mServiceMisc->createCharacteristic(
                                         SET_LCD_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetLCD = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetLCD->setValue("SET LCD WITH CMD");
  mCharMiscSetLCD->addDescriptor(mDescMiscSetLCD);
  mCharMiscSetLCD->setCallbacks(new MyMiscSetLCDCallbacks());

  BLECharacteristic *mCharMiscSetOLED = mServiceMisc->createCharacteristic(
                                          SET_OLED_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetOLED = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetOLED->setValue("SET OLED WITH CMD");
  mCharMiscSetOLED->addDescriptor(mDescMiscSetOLED);
  mCharMiscSetOLED->setCallbacks(new MyMiscSetOLEDCallbacks());

  BLECharacteristic *mCharMiscSetDigital = mServiceMisc->createCharacteristic(
                                          SET_DIGITAL_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetDigital = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetDigital->setValue("SET Digital WITH CMD");
  mCharMiscSetDigital->addDescriptor(mDescMiscSetDigital);
  mCharMiscSetDigital->setCallbacks(new MyMiscSetDigitalCallbacks());

    BLECharacteristic *mCharMiscSetAnalog = mServiceMisc->createCharacteristic(
                                          SET_ANALOG_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetAnalog = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetAnalog->setValue("SET Analog WITH CMD");
  mCharMiscSetAnalog->addDescriptor(mDescMiscSetAnalog);
  mCharMiscSetAnalog->setCallbacks(new MyMiscSetAnalogCallbacks());

    BLECharacteristic *mCharMiscSetDht = mServiceMisc->createCharacteristic(
                                          SET_DHT_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetDht = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetDht->setValue("SET Dht WITH CMD");
  mCharMiscSetDht->addDescriptor(mDescMiscSetDht);
  mCharMiscSetDht->setCallbacks(new MyMiscSetDhtCallbacks());

    BLECharacteristic *mCharMiscSetUltrasonic = mServiceMisc->createCharacteristic(
                                          SET_ULTRASONIC_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetUltrasonic = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetUltrasonic->setValue("SET Ultrasonic WITH CMD");
  mCharMiscSetUltrasonic->addDescriptor(mDescMiscSetUltrasonic);
  mCharMiscSetUltrasonic->setCallbacks(new MyMiscSetUltrasonicCallbacks());


    BLECharacteristic *mCharMiscSetGyro = mServiceMisc->createCharacteristic(
                                          SET_GYRO_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetGyro = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetGyro->setValue("SET Gyro WITH CMD");
  mCharMiscSetGyro->addDescriptor(mDescMiscSetGyro);
  mCharMiscSetGyro->setCallbacks(new MyMiscSetGyroCallbacks());

    BLECharacteristic *mCharMiscSetTouch = mServiceMisc->createCharacteristic(
                                          SET_TOUCH_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetTouch = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetTouch->setValue("SET Touch WITH CMD");
  mCharMiscSetTouch->addDescriptor(mDescMiscSetTouch);
  mCharMiscSetTouch->setCallbacks(new MyMiscSetTouchCallbacks());

    BLECharacteristic *mCharMiscSetButton = mServiceMisc->createCharacteristic(
                                          SET_BUTTON_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetButton = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetButton->setValue("SET Button WITH CMD");
  mCharMiscSetButton->addDescriptor(mDescMiscSetButton);
  mCharMiscSetButton->setCallbacks(new MyMiscSetButtonCallbacks());

    BLECharacteristic *mCharMiscSetbutton_pu = mServiceMisc->createCharacteristic(
                                          SET_BUTTON_PU_UUID,
                                          BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetbutton_pu = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetbutton_pu->setValue("SET button_pu WITH CMD");
  mCharMiscSetbutton_pu->addDescriptor(mDescMiscSetbutton_pu);
  mCharMiscSetbutton_pu->setCallbacks(new MyMiscSetbutton_puCallbacks());
  // // Status Information
  mCharMiscStatusInfo = mServiceMisc->createCharacteristic(
                          MISC_CHARACTERISTIC_STATUS_INFO_UUID,
                          BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  mCharMiscStatusInfo->setValue((uint8_t*)&value_misc_status_info, 1);
  BLEDescriptor *mDescMiscStatusInfo = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscStatusInfo->setValue("Status Info");
  mCharMiscStatusInfo->addDescriptor(mDescMiscStatusInfo);
  mCharMiscStatusInfo->addDescriptor(new BLE2902());


  mServiceMisc->start();
  //************************************************



  //************************************************
  // Sensor Service
  BLEService *mServiceSensor = mServer->createService(BLEUUID(GET_SERVICE_UUID), 20);

  // All Data
  mCharSensorAllData = mServiceSensor->createCharacteristic(
                         GET_BUTTON_UUID,
                         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  //코드의 마지막 부분에서 value_sensor_all_data에 집어넣은 센서데이터 값들을 전송하는 부분

  mCharSensorAllData->setValue((uint8_t*)&value_sensor_all_data, 30);
  BLEDescriptor *mDescSensorAllData = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescSensorAllData->setValue("Update All Data for Scratch");
  mCharSensorAllData->addDescriptor(mDescSensorAllData);
  mCharSensorAllData->addDescriptor(new BLE2902());

  mServiceSensor->start();
  //************************************************

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(GET_SERVICE_UUID);
  pAdvertising->addServiceUUID(SET_SERVICE_UUID);

  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Ready.!!");

}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(10);

  // Status LED
  status_led_count++;
  if (status_led_count > 50) {
    if (device_connected) {

      if (status_text_displayed) {

        status_text_displayed = false;
      }
    }
    else {

      if (!status_text_displayed) {
        status_text_displayed = true;
      }
    }
    status_led_count = 0;
  }


  // Status Info
  status_update_info_count++;
  if (status_update_info_count > 5) {

    //4->1
    mCharMiscStatusInfo->setValue((uint8_t*)&value_misc_status_info, 1);
    if (device_connected) {
      mCharMiscStatusInfo->notify();
    }
    status_update_info_count = 0;
  }

  status_update_all_count++;
  if (status_update_all_count > 5) {

    
    value_sensor_all_data[0] = digital_value;
    // analog의 값은 1바이트를 초과, 0-4096을 가짐
    // 2바이트로 나눠서 전송
    value_sensor_all_data[1]= analog_value;
    value_sensor_all_data[2]= analog_value>>8;
    
    //dht 온,습도와 초음파 센서는 Float이기 대문에 Union을 사용해서 uint8_t로 바꿔서 전송
    val.floatVal=dht_value[0];
    memcpy(&value_sensor_all_data[3], val.intVal, 4);
     val.floatVal=dht_value[1];
    memcpy(&value_sensor_all_data[7], val.intVal, 4);
    val.floatVal=ultrasonic_value;
    memcpy(&value_sensor_all_data[11], val.intVal, 4);

    value_sensor_all_data[15]=touch_value;
    gyro_value[0]=(int16_t)(gyroX* 100.0);
    gyro_value[1]=(int16_t)(gyroY* 100.0);
    gyro_value[2]=(int16_t)(gyroZ* 100.0);
    // // 자이로 센서  값 추가 필요
     memcpy(&value_sensor_all_data[15], gyro_value, 6);

    mCharSensorAllData->setValue((uint8_t*)&value_sensor_all_data, 30);
    if (device_connected) {
      mCharSensorAllData->notify();
    }

    status_update_all_count = 0;
  }
}
