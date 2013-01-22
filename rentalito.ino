#include <SPI.h>
#include <WiFly.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ht1632c.h>
#include "config.h"

#define RX_PIN  2
#define TX_PIN  3
#define MATRIX_DATA_PIN 7
#define MATRIX_WR_PIN 6
#define MATRIX_CLK_PIN 4
#define MATRIX_CS_PIN 5
#define ERROR_LED 13

#define UPDATE_DELAY 5000
#define SCROLL_DELAY 20
#define SMALL_FONT 2
#define MEDIUM_FONT 16
#define BIG_FONT 16
#define BRIGHTNESS 10
#define LINE_TOP 1
#define LINE_BOTTOM 2
#define LINE_FULLSIZE 3

void callback(char* topic, byte* payload, unsigned int length);

WiFlyClient wifiClient;
SoftwareSerial wifiSerial(RX_PIN, TX_PIN);
PubSubClient mqttClient(mqttServer, mqttPort, callback, wifiClient);
ht1632c matrix = ht1632c(&PORTD, MATRIX_DATA_PIN, MATRIX_WR_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, GEOM_32x16, 2);

void display(int position, char * text) {
    int y, color;
    boolean scroll;
    switch (position) {
        case LINE_BOTTOM:
            matrix.setfont(MEDIUM_FONT);
            y = matrix.y_max - matrix.font_height + 1;
            color = GREEN;
            scroll = true;
            break;
        case LINE_TOP:
            matrix.setfont(SMALL_FONT);
            y = 0;
            color = RED;
            scroll = false;
            break;
        case LINE_FULLSIZE:
            matrix.setfont(BIG_FONT);
            y=0;
            color = TRICOLOR;
            scroll = true;
    }
    byte len = strlen(text);
    if (scroll && (len > (byte) ((matrix.x_max + 1) / matrix.font_width))) {
        matrix.hscrolltext(y, text, color, SCROLL_DELAY, 1, LEFT);
    } else {
        matrix.puttext(0, y, text, color, CENTER);
    }
}

void debug(char * message) {
    Serial.println(message);
    matrix.clear();
    display(LINE_TOP, message);
}

void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    Serial.print((char *) payload);
    matrix.clear();
    display(LINE_TOP, "Power (W)");
    display(LINE_BOTTOM, (char *) payload);
}

void setup() {

    // Debug LED
    pinMode(ERROR_LED, OUTPUT);
    digitalWrite(ERROR_LED, LOW);

    // matrix setup
    matrix.clear();
    matrix.pwm(BRIGHTNESS);

    // Wifi setup
    Serial.begin(9600);
    wifiSerial.begin(9600);
    WiFly.setUart(&wifiSerial);
    debug("Initializing");
    WiFly.begin();

    debug("Joining");
    if (!WiFly.join(ssid, passphrase)) {
        digitalWrite(ERROR_LED, HIGH);
        debug("Failed");
        while (1) {
            // Hang on failure.
        }
    }
    
    debug("Connecting");
    // MQTT client setup
    if (mqttClient.connect("rentalito")) {
        
        debug("Subscribing");
        mqttClient.subscribe("/benavent/general/power");

    } else {
        debug("Failed");
        digitalWrite(ERROR_LED, HIGH);
    }

}

void loop() {
    mqttClient.loop();
}

