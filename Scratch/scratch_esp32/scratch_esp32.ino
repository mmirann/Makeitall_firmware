#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>


#define SET_SERVICE_UUID     "c005"
#define SET_NEOPIXEL_UUID    "d895d61e-902e-11eb-a8b3-0242ac130003"
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

Adafruit_NeoPixel strip = Adafruit_NeoPixel(4, 13, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display


class MyBLEServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    device_connected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    device_connected = false;
  }
};

class MyMiscSetColorLEDCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if(value.length() == 3) {     
    //   uint8_t left_r = value[0];
    //   uint8_t left_g = value[1];
    //   uint8_t left_b = value[2];
    //   uint8_t right_r = value[3];
    //   uint8_t right_g = value[4];
    //   uint8_t right_b = value[5];

    int r=value[0];
    int g=value[1];
    int b=value[2];

    //   // reduce brightness from original rgb color
    //   //1. rgb to hsv
    //   float h, s, v;
    //   rgbTohsv(left_r, left_g, left_b, h, s, v);
    //   v = v / 20.0;
    //   hsvTorgb(h, s, v, left_r, left_g, left_b);
      
    //   rgbTohsv(right_r, right_g, right_b, h, s, v);
    //   v = v / 20.0;
    //   hsvTorgb(h, s, v, right_r, right_g, right_b);
      
    //   edubot.led.leftBright(left_r, left_g, left_b);
    //   edubot.led.rightBright(right_r, right_g, right_b);
        strip.begin();

        strip.setPixelColor(0, r, g, b);
        strip.setPixelColor(1, r, g, b);
        strip.setPixelColor(2, r, g, b);
        strip.setPixelColor(3, r, g, b);

        strip.show();
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
//*****************************
  lcd.setCursor(0,0);
  lcd.print("Start begin");

  Serial.println("===============\nStarting BLE work!");

  BLEDevice::init("OROCA EduBot");
  BLEServer *mServer = BLEDevice::createServer();
  mServer->setCallbacks(new MyBLEServerCallbacks());
  
  BLEAddress addr = BLEDevice::getAddress();  
  memcpy(ble_mac_addr, *addr.getNative(), 6);

  //************************************************

  // Misc Service
  BLEService *mServiceMisc = mServer->createService(BLEUUID(SET_SERVICE_UUID), 20);

  // SetColorLED
  BLECharacteristic *mCharMiscSetColorLED = mServiceMisc->createCharacteristic(
                                         SET_NEOPIXEL_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  BLEDescriptor *mDescMiscSetColorLED = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description
  mDescMiscSetColorLED->setValue("NeoPixel Color RGB");  
  mCharMiscSetColorLED->addDescriptor(mDescMiscSetColorLED);
  mCharMiscSetColorLED->setCallbacks(new MyMiscSetColorLEDCallbacks());

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
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SETUP FINISH");

}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(10);
  lcd.setCursor(0,0);
  lcd.print("1");
  // Status LED
  status_led_count++;
  if(status_led_count > 50) {
    if(device_connected) {
    //   edubot.ledOn();

       if(status_text_displayed) {
        lcd.setCursor(0, 0);
        lcd.clear();
        lcd.print("Ready for BLE");
        status_text_displayed = false;
      }
    }
    else {
    //   edubot.ledToggle();

      if(!status_text_displayed) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("JIKKO");
        //char buf[20];
        //sprintf(buf, "%X:%X:%X:%X:%X:%X", ble_mac_addr[0], ble_mac_addr[1], ble_mac_addr[2], ble_mac_addr[3], ble_mac_addr[4], ble_mac_addr[5]);
        //lcd.setCursor(1,0);
       // lcd.print(std::string(buf).c_str());
        status_text_displayed = true;
      } 
    }
    status_led_count = 0;
  }

  lcd.setCursor(0,0);
  lcd.print("3");
  // Status Info
  status_update_info_count++;
  if(status_update_info_count > 5) {

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
        lcd.setCursor(0,0);
        lcd.print("push");
      value_misc_status_info[0] = 1;
    }
    else {

        lcd.setCursor(0,0);
        lcd.print("pull");
      value_misc_status_info[0] = 0;
    }

    //4->1
    mCharMiscStatusInfo->setValue((uint8_t*)&value_misc_status_info, 1);
    if(device_connected) {
      mCharMiscStatusInfo->notify();
    }
    status_update_info_count = 0;
  }

  status_update_all_count++;
  if(status_update_all_count > 5) {
    memcpy(&value_sensor_all_data[0], value_misc_status_info, 1);
    // memcpy(&value_sensor_all_data[4], value_sensor_floor_sensors, 4);
    // memcpy(&value_sensor_all_data[8], value_sensor_distance_sensors, 4);
    // memcpy(&value_sensor_all_data[12], value_sensor_imu_sensor, 18);
  //30->1
    mCharSensorAllData->setValue((uint8_t*)&value_sensor_all_data,1);
    if(device_connected) {
      mCharSensorAllData->notify();
    }
    
    status_update_all_count = 0;
  }
}


