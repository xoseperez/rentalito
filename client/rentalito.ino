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
#define LDR_PIN 1
#define AD22103_PIN 0
#define ERROR_LED 13

// Any delay will cause the chances to loose connection to raise
#define SCROLL_DELAY 20
#define AFTER_ERROR_DELAY 5000

#define SMALL_FONT 2
#define MEDIUM_FONT 16
#define BIG_FONT 21
#define BRIGHTNESS 10
#define LINE_TOP 1
#define LINE_BOTTOM 2
#define LINE_FULLSIZE 3

#define TEMPERATURE_OFFSET -3.0
#define REPORT_INTERVAL 300000

#define MQTT_MAX_PACKET_SIZE 168
#define MQTT_KEEPALIVE 300

void callback(char* topic, byte* payload, unsigned int length);

// MQTT parameters
byte mqttServer[] = { 192, 168, 1, 10 };
int mqttPort = 1883;
char mqttClientId[] = "rentalito";
char publishTopic[] = "/benavent/rentalito/temperature";
char subscribeTopic[] = "/client/rentalito";

uint16_t position;
boolean wifi_connected = false;
int current_brightness = BRIGHTNESS;

WiFlyClient wifiClient;
SoftwareSerial wifiSerial(RX_PIN, TX_PIN);
PubSubClient mqttClient(mqttServer, mqttPort, callback, wifiClient);
ht1632c matrix = ht1632c(&PORTD, MATRIX_DATA_PIN, MATRIX_WR_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, GEOM_32x16, 2);

unsigned long last_report = 0;

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

void debug(const __FlashStringHelper * console_text, uint8_t color) {
    Serial.println(console_text);
    matrix.plot(0, 0, color);
    matrix.sendframe();
}

void check_temperature() {
    int val = analogRead(AD22103_PIN);
    float temperature = (5000.0 * val / 1023.0 - 250.0) / 28.0 + TEMPERATURE_OFFSET;
    char * payload = "00.0";
    dtostrf(temperature, 4, 1, payload);
    mqttClient.publish(publishTopic, (byte *) payload, 5);
    Serial.print(F("Temperature: "));
    Serial.println(payload);
}

void check_brightness() {
    int val = analogRead(LDR_PIN);
    int brightness = 16 * val / 1023;
    if (brightness != current_brightness) {
        matrix.pwm(brightness);
        current_brightness = brightness;
        Serial.print(F("Brightness set to: "));
        Serial.println(brightness);
    }
}

void callback(char* topic, byte* payload, unsigned int length) {

    // Copy the payload to the new buffer
    char * message = (char *) malloc(length+1);
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.print(topic);
    Serial.print(F(": "));
    Serial.println(message);

    uint16_t position;
    boolean found = false;
    for (position=0; position<length; position++) {
        if (message[position] == '|') {
            message[position] = '\0';
            ++position;
            found = true;
            break;
        }
    }

    matrix.clear();
    if (found) {
        display(LINE_TOP, message);
        display(LINE_BOTTOM, message + position);
    } else {
        display(LINE_FULLSIZE, message);
    }
    free(message);

    // The available-flush pair is a double security to ensure any
    // incoming message is discarded.
    // Apparently, cued messages or half formed messages cause the
    // PubSubClient library to reset.
    while (wifiClient.available()) {
        wifiClient.flush();
    }

}

void connectWifi() {

    debug(F("Initialising"), RED);
    WiFly.begin();

    debug(F("Joining"), ORANGE);
    if (!WiFly.join(ssid, passphrase, true)) {
        digitalWrite(ERROR_LED, HIGH);
        debug(F("Failed"), RED);
        delay(AFTER_ERROR_DELAY);
    } else {
        wifi_connected = true;
    }

}

void connectMQTT() {

    wifi_connected = false;
    connectWifi();

    if (wifi_connected) {
        // MQTT client setup
        mqttClient.disconnect();
        debug(F("Connecting"), GREEN);
        if (mqttClient.connect(mqttClientId)) {
            mqttClient.subscribe(subscribeTopic);
            digitalWrite(ERROR_LED, LOW);
            matrix.plot(0, 0, BLACK);
            matrix.sendframe();

        } else {
            digitalWrite(ERROR_LED, HIGH);
            debug(F("Failed"), RED);
            delay(AFTER_ERROR_DELAY);
        }
    }

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

    last_report = millis();

}

void loop() {

    if (!mqttClient.loop()) {
        connectMQTT();
    }

    /*
    unsigned long time = millis();
    if (time - last_report > REPORT_INTERVAL or time < last_report ) {
        check_temperature();
        last_report = time;
    }
    */

    check_brightness();

}

