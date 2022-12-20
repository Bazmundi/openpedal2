/*
OpenPeddle
Copyright (C) 2022 Asterion Daedalus

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>
#include <esp_now.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
// On an ESP32:             21(SDA), 22(SCL) from: https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/ 

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ESP-NOW message for pump mac, change to your pump mac
uint8_t pumpAddress[] = {0x84, 0xCC, 0xA8, 0x2C, 0x0D, 0xA8};

#define PUMP_ON true;
#define PUMP_OFF false;
bool pumpState = PUMP_OFF;

// pump will either respond to a message or not and the pressence flag will be set if it successfully receives a message
#define PUMP_PRESENT true;
#define PUMP_UNAVAILABLE false;
bool pumpPresence = PUMP_UNAVAILABLE;

// message will either be herald message 'false' to find pump, or pedal down message 'true' to tell pump to toggle on/off
#define PEDAL_PRESSED true;
#define PUMP_CONNECT false;
typedef struct struct_message {
  bool pumpFlag_msg;
} struct_message;

struct_message pumpMessage;
esp_now_peer_info_t peerInfo;

// callback to check on success of message send
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Prints it on Serial Monitor
  Serial.println("Sent to pump"); 

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println( "Delivery Success");
    pumpPresence = PUMP_PRESENT;
  } else {
    Serial.println( "Delivery Fail");
    pumpPresence = PUMP_UNAVAILABLE;
  }
}

// display a set of display updates, dependant upon two flags
void displayUpdate(bool updown, bool pumpthere) {
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(10,1);             // Start at top-left corner
  display.print(updown ? F("Peddle down") : F("Peddle up"));
  display.setCursor(10,20);             // Start at top-left corner
  if (!pumpthere) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
    display.fillRect(8,16,112,22,SSD1306_INVERSE);
    display.print(F("Pump disconnected!"));
  } else {
    
    display.print( F("Pump connected!") );
  }
  
  display.display();
}

// setup led to blink
#define LED_PIN LED_BUILTIN


// pedal input processing
#define BOUNCE_PIN 33
Bounce bounce = Bounce();
int pressCount = 0;  // pedal press count for debugging


// start ESP-NOW
void InitESPNow(void) {
  //If the initialization was successful
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  //If there was an error
  else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

void registerCallback (void) {

  esp_err_t cbstatus = esp_now_register_send_cb(OnDataSent);

  switch (cbstatus)
  {
  case ESP_OK:
    Serial.println("cb registered");
    break;

  case ESP_ERR_ESPNOW_NOT_INIT:
    Serial.println("ESP NOW not actually initiated");
    break;

  case ESP_ERR_ESPNOW_INTERNAL:
    Serial.println("ESP NOW internal error");
    break;

  default:
    Serial.println("Unknown error in cb registration");
    break;
  }

}

void registerPeer (void) {
  memcpy(peerInfo.peer_addr, pumpAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  esp_err_t status = esp_now_add_peer(&peerInfo);

  switch (status)
    {
    case ESP_OK:
      Serial.println("Peer registered");
      break;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESP NOW not actually initiated");
      break;

    case ESP_ERR_ESPNOW_ARG:
      Serial.println("ESP NOW error in argument");
      break;

    case ESP_ERR_ESPNOW_FULL:
      Serial.println("ESP NOW peer list is full");
      break;

    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("ESP NOW no memory");
      break;

    case ESP_ERR_ESPNOW_EXIST:
      Serial.println("ESP NOW peer has existed");
      break;   
    default:
      Serial.println("Unknown error peer registration");
      break;
    }
}

// ping pump till it connects
void findPump (void) {

  while (!pumpPresence) {
    pumpMessage.pumpFlag_msg = PUMP_CONNECT;
    pumpPresence = PUMP_UNAVAILABLE;
    

    esp_err_t result = esp_now_send(pumpAddress, (uint8_t *) &pumpMessage, sizeof(pumpMessage));  
   
    switch (result)
    {
    case ESP_OK:
      Serial.println("Connect sent");
      pumpPresence = PUMP_PRESENT;
      break;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESP NOW not actually initiated");
      break;

    case ESP_ERR_ESPNOW_ARG:
      Serial.println("ESP NOW error in argument");
      break;

    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("ESP NOW internal error");
      break;

    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("ESP NOW no memory");
      break;

    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("ESP NOW peer not found");
      break;   

    case ESP_ERR_ESPNOW_IF:
      Serial.println("ESP NOW peer wifi mismatch");
      break; 

    default:
      Serial.println("Unknown error in send");
      break;
    }
    delay(1000);
    }
}

// setup device
void setup() {
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  }
  
  display.clearDisplay();
  
  // setup access point

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  
  delay(1500);
  Serial.print("\nMAC: ");
  Serial.println(WiFi.macAddress());

  // start ESP-NOW
  InitESPNow();
  registerCallback();

  // setup pedal input
  bounce.attach( BOUNCE_PIN ,  INPUT );
  bounce.interval(5); // interval in ms
  displayUpdate(0,false);

  // LED SETUP
  int ledState = HIGH;
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);

  // add peer
  registerPeer();
  findPump();
  displayUpdate(0,pumpPresence);
}



// send a pedal press message, but flag pump lost if message is not successful
void pedalPressed (void) {
  pumpMessage.pumpFlag_msg = PEDAL_PRESSED;

  esp_err_t status = esp_now_send(pumpAddress, (uint8_t *) &pumpMessage, sizeof(pumpMessage));
  pumpPresence = PUMP_UNAVAILABLE;

  switch (status)
  {
  case ESP_OK:
    Serial.println("Pedal press sent");
    pumpPresence = PUMP_PRESENT;
    break;
  case ESP_ERR_ESPNOW_NOT_INIT:
    Serial.println("ESP NOW not actually initiated");
    break;

  case ESP_ERR_ESPNOW_ARG:
    Serial.println("ESP NOW error in argument");
    break;

  case ESP_ERR_ESPNOW_INTERNAL:
    Serial.println("ESP NOW internal error");
    break;

  case ESP_ERR_ESPNOW_NO_MEM:
    Serial.println("ESP NOW no memory");
    break;

  case ESP_ERR_ESPNOW_NOT_FOUND:
    Serial.println("ESP NOW peer not found");
    break;   

  case ESP_ERR_ESPNOW_IF:
    Serial.println("ESP NOW peer wifi mismatch");
    break; 

  default:
    Serial.println("Unknown error in send");
    break;
  }
}




// forever do
void loop() {

  bounce.update();

  // <Bounce>.changed() RETURNS true IF THE STATE CHANGED (FROM HIGH TO LOW OR LOW TO HIGH)
  if ( bounce.changed() ) {
    // THE STATE OF THE INPUT CHANGED
    // GET THE STATE
    int deboucedInput = bounce.read();
    
    // IF THE CHANGED VALUE IS HIGH
    if ( deboucedInput == HIGH ) {
      pedalPressed () ;
      digitalWrite(LED_PIN, LOW); 
      Serial.println("button press " + String(pressCount)); 
    } else {

      digitalWrite(LED_PIN, HIGH); 
      Serial.println("button release " + String(pressCount));
      pressCount += 1; 
    }
    
    displayUpdate(deboucedInput,pumpPresence);
  } 
  
}

