/*
  Wireless Motor Control, with ESP as Access Point

  Usage: 
    Connect phone or laptop to "ESP Whee" wireless network
    Go to 192.168.4.1. 
    A webpage with four buttons should appear. Click them to move the robot.

  Installation: 
    In Arduino, go to Tools > ESP8266 Sketch Data Upload to upload the files from ./data to the ESP
    Then, in Arduino, compile and upload sketch to the ESP

  Requirements:
    Arduino support for ESP8266 board
      In Arduino, add URL to Files > Preferences > Additional Board Managers URL.
      See https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/installing-the-esp8266-arduino-addon

    Websockets library
      To install, Sketch > Include Library > Manage Libraries... > Websockets > Install
      https://github.com/Links2004/arduinoWebSockets
    
    ESP8266FS tool
      To install, create "tools" folder in Arduino, download, and unzip. See 
      https://github.com/esp8266/Arduino/blob/master/doc/filesystem.md#uploading-files-to-file-system


  Note that one of the motor pins overlaps with LED pin D4, so when the left wheel is going CW, the LED light close the ESP chip will turn on
  Note that we opted to swap the motor wires on the right motor so that LOW is backwards for both motors
  
  We define arbitrarily Left CW = backwards
  
  Hardware: 
  * NodeMCU Amica DevKit Board (ESP8266 chip)
  * Motorshield for NodeMCU 
  * 2 motors + 2 wheels + motor chassis with caster
  * Left motor connected to D3 (BLK/A- and RED/A+ on the shield)
  * Right motor connected to D4, with wires swapped (RED/B- and BLK/B+ on the shield)

  modified Nov 2016
  Nancy Ouyang
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>

#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <FS.h>

const int LED_PIN = D0;

const int MOTOR_PWM_LEFT = D1;
const int MOTOR_PWM_RIGHT = D2;

const int MOTOR_DIR_LEFT = D3;
const int MOTOR_DIR_RIGHT = D4;

#define MOTOR_BACK LOW
#define MOTOR_FWD HIGH


// WiFi parameters
const char* ssid = "ESP Whee";
const char* password = "my_password";

ESP8266WebServer server = ESP8266WebServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

String html_home;
String css_style;

void setup() {
    WiFi.softAP(ssid, password);

    Serial.begin(115200);

    setupServer();

    setupPins();
}

void loop() {
    webSocket.loop();
    server.handleClient();
}



//
// Movement Functions //
//

int stop() {
  //digitalWrite(LED_PIN, HIGH);
  Serial.println("stop");
  analogWrite(MOTOR_PWM_LEFT, 512);
  analogWrite(MOTOR_PWM_RIGHT, 512);
}

int forward() {
  //digitalWrite(LED_PIN, LOW);
  Serial.println("forward");
  digitalWrite(MOTOR_DIR_LEFT, MOTOR_FWD);
  digitalWrite(MOTOR_DIR_RIGHT, MOTOR_FWD);

  analogWrite(MOTOR_PWM_LEFT, 1023);
  analogWrite(MOTOR_PWM_RIGHT, 1023);
}

int backward() {
  Serial.println("backward");
  digitalWrite(MOTOR_DIR_LEFT, MOTOR_BACK);
  digitalWrite(MOTOR_DIR_RIGHT, MOTOR_BACK);

  analogWrite(MOTOR_PWM_LEFT, 0);
  analogWrite(MOTOR_PWM_RIGHT, 0);
}

int left() {
  Serial.println("left");
  digitalWrite(MOTOR_DIR_LEFT, MOTOR_BACK);
  digitalWrite(MOTOR_DIR_RIGHT, MOTOR_FWD);

  analogWrite(MOTOR_PWM_LEFT, 0);
  analogWrite(MOTOR_PWM_RIGHT, 1023);
}

int right() {
  Serial.println("right");
  digitalWrite(MOTOR_DIR_LEFT, MOTOR_FWD);
  digitalWrite(MOTOR_DIR_RIGHT, MOTOR_BACK);

  analogWrite(MOTOR_PWM_LEFT, 1023);
  analogWrite(MOTOR_PWM_RIGHT, 0);
}



//
// Setup Methods //
//
void prepareFile() {
  
    Serial.println("Prepare file system");
    SPIFFS.begin();
  
    File file = SPIFFS.open("/controls.html", "r");
    if (!file) {
        Serial.println("HTML file open failed");  
    } 
    else {
        Serial.println("HTML file open success");

        html_home = "";
        while (file.available()) {
            //Serial.write(file.read());
            String line = file.readStringUntil('\n');
            html_home += line + "\n";
        }
        file.close();

        //Serial.print(html_home);
    }

    File css_file = SPIFFS.open("/style.css", "r");
    if (!css_file) {
        Serial.println("CSS file open failed");  
    } 
    else {
        Serial.println("CSS file open success");

        css_style = "";
        while (css_file.available()) {
            //Serial.write(file.read());
            String line = css_file.readStringUntil('\n');
            css_style += line + "\n";
        }
        css_file.close();

        //Serial.print(html_home);
    }



}

//
// Setup //
//

void setupPins() {
    // setup LEDs and Motors
    Serial.println("Setup LED and motor pins");
    pinMode(LED_PIN, OUTPUT);    //Pin D0 is LED
    digitalWrite(LED_PIN, HIGH); //Initial state is HIGH (OFF)

    pinMode(MOTOR_PWM_LEFT, OUTPUT);
    pinMode(MOTOR_DIR_LEFT, OUTPUT);
    pinMode(MOTOR_PWM_RIGHT, OUTPUT);
    pinMode(MOTOR_DIR_RIGHT, OUTPUT);

    digitalWrite(MOTOR_PWM_LEFT, HIGH); //Initial state is HIGH (OFF)
    digitalWrite(MOTOR_PWM_RIGHT, HIGH); //Initial state is HIGH (OFF)

    // Set initial speed to 0
    analogWrite(MOTOR_PWM_LEFT, 0);
    analogWrite(MOTOR_PWM_RIGHT, 0);
}

void setupServer() {
    for(uint8_t t = 4; t > 0; t--) {
        Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    // read the html code to html_homes
    prepareFile();


    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // start webSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if(MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    // handle index
    server.on("/", []() {
        // send home.html
        server.send(200, "text/html", html_home);
    });

    server.on("/style.css", []() {
        // send home.html
        server.send(200, "text/css", css_style);
    });

    server.begin();

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);

    Serial.printf("Server Start\n");
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: 
        {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
            break;
        }
        case WStype_TEXT:
            Serial.printf("[%u] get text: %s\n", num, payload);

            if (payload[0] == '#') {
                if(payload[1] == 'F') {
                  forward();
                }
                else if(payload[1] == 'B') {
                  backward();
                }
                else if(payload[1] == 'L') {
                  left();
                }
                else if(payload[1] == 'R') {
                  right();
                }
                else {
                  stop();
                }
            }

            break;
    }
}
