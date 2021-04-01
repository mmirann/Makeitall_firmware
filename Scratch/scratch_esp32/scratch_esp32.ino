#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>


#define SET_SERVICE_UUID     "c005"
#define SET_NEOPIXEL_UUID    "d895d61e-902e-11eb-a8b3-0242ac130003"
#define SET_PIN_UUID "d895d7cc-902e-11eb-a8b3-0242ac130003"
#define MISC_CHARACTERISTIC_STATUS_INFO_UUID         "34443c3b-3356-11e9-b210-d663bd873d93"

#define GET_SERVICE_UUID     "c006"
#define GET_BUTTON_UUID      "d895d704-902e-11eb-a8b3-0242ac130003"

char ble_mac_addr[6] = {0, 0, 0, 0, 0, 0};

uint8_t value_misc_status_info[4] = {0, 0, 0, 0};
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
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, 2, NEO_GRB + NEO_KHZ800);


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
  2. clear_all: cmd, pin
  3. no_color: cmd, pin, r, g, b, num
  4. all_color: cmd, pin, r, g, b
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
        //return;

      } else if (cmd == 1) { //brightness
        int brightness = value[2];
        strip.setBrightness(brightness);

        //return ;

      } else if (cmd == 2) { //clear
        strip.clear();
        strip.show();
      } else {
        int r = value[2];
        int g = value[3];
        int b = value[4];

        if (cmd == 3) { // num color
          int num = value[5];
          strip.setPixelColor(num, r, g, b);
          strip.show();
        } else if (cmd == 4) { //all color

          for (int i = 0; i < led_num; i++)
            strip.setPixelColor(i, r, g, b);
          strip.show();

        }
      }
    }
};

// cmd pin value
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
        ledcAttachPin(pin, 0);
        ledcSetup(0, 5000, 8);
        ledcWrite(0, _value);
      }
    }
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //************lcd**************
  lcd.init();                      // initialize the lcd
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
  lcd.clear();
  //*****************************

  Serial.println("===============\nStarting BLE work!");

  BLEDevice::init("OROCA EduBot");
  BLEServer *mServer = BLEDevice::createServer();
  mServer->setCallbacks(new MyBLEServerCallbacks());

  BLEAddress addr = BLEDevice::getAddress();
  memcpy(ble_mac_addr, *addr.getNative(), 6);

  //************************************************

  // Misc Service
  BLEService *mServiceMisc = mServer->createService(BLEUUID(SET_SERVICE_UUID), 20);

  // SET PIN
  BLECharacteristic *mCharMiscSetPIN = mServiceMisc->createCharacteristic(
                                         SET_PIN_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetPIN = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetPIN->setValue("SET PIN WITH CMD");
  mCharMiscSetPIN->addDescriptor(mDescMiscSetPIN);
  mCharMiscSetPIN->setCallbacks(new MyMiscSetPINCallbacks());

  // NEO PIXEL
  BLECharacteristic *mCharMiscSetNEO = mServiceMisc->createCharacteristic(
                                         SET_NEOPIXEL_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetNEO = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetNEO->setValue("SET NEOPIXEL WITH CMD");
  mCharMiscSetNEO->addDescriptor(mDescMiscSetNEO);
  mCharMiscSetNEO->setCallbacks(new MyMiscSetNEOCallbacks());


  // Status Information
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

  //30->1
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
      //   edubot.ledOn();

      if (status_text_displayed) {
        // lcd.setCursor(0, 0);
        // lcd.clear();
        // lcd.print("Ready for BLE");
        status_text_displayed = false;
      }
    }
    else {
      //   edubot.ledToggle();

      if (!status_text_displayed) {
        // lcd.clear();
        // lcd.setCursor(0,0);
        // lcd.print("JIKKO");
        //char buf[20];
        //sprintf(buf, "%X:%X:%X:%X:%X:%X", ble_mac_addr[0], ble_mac_addr[1], ble_mac_addr[2], ble_mac_addr[3], ble_mac_addr[4], ble_mac_addr[5]);
        //lcd.setCursor(1,0);
        // lcd.print(std::string(buf).c_str());
        status_text_displayed = true;
      }
    }
    status_led_count = 0;
  }


  // Status Info
  status_update_info_count++;
  if (status_update_info_count > 5) {

    // value_misc_status_info[0] = (uint8_t)edubot.motor.isBusy();


    // value_misc_status_info[1] = edubot.batteryGetVoltage();
    // if(value_misc_status_info[1] < 32) {
    //   value_misc_status_info[2] = 1;
    // }
    // else {
    //   value_misc_status_info[2] = 0;
    // }
    pinMode(2, INPUT_PULLUP);

    if (digitalRead(2) == 0) {
      // lcd.setCursor(0,0);
      // lcd.print("push");
      value_misc_status_info[0] = 1;
    }
    else {

      // lcd.setCursor(0,0);
      // lcd.print("pull");
      value_misc_status_info[0] = 0;
    }

    //4->1
    mCharMiscStatusInfo->setValue((uint8_t*)&value_misc_status_info, 1);
    if (device_connected) {
      mCharMiscStatusInfo->notify();
    }
    status_update_info_count = 0;
  }

  status_update_all_count++;
  if (status_update_all_count > 5) {
    memcpy(&value_sensor_all_data[0], value_misc_status_info, 1);
    // memcpy(&value_sensor_all_data[4], value_sensor_floor_sensors, 4);
    // memcpy(&value_sensor_all_data[8], value_sensor_distance_sensors, 4);
    // memcpy(&value_sensor_all_data[12], value_sensor_imu_sensor, 18);
    //30->1
    mCharSensorAllData->setValue((uint8_t*)&value_sensor_all_data, 1);
    if (device_connected) {
      mCharSensorAllData->notify();
    }

    status_update_all_count = 0;
  }
}
