#pragma once
#include <Arduino.h>
#include "RBCX.h"

#define ESP32P4_SYNC0 0xAA
#define ESP32P4_SYNC1 0x55

typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
    bool camera;
    bool on;
    int16_t angle;
    uint16_t distance;
    uint16_t max_distance;
} Esp32p4Message;

struct Communication {
    int x_distance = 0;
    int y_distance = 0;

    Esp32p4Message msg = {0, 0, false, false, 0, 0, 0};

    enum RxState { WAIT_SYNC0, WAIT_SYNC1, READ_PAYLOAD, READ_CHECKSUM };
    RxState state = WAIT_SYNC0;

    static const size_t PAYLOAD_SIZE = sizeof(Esp32p4Message);
    uint8_t buf[sizeof(Esp32p4Message)];
    size_t idx = 0;
    uint8_t checksum = 0;

    void setup() {
        Serial1.begin(115200, SERIAL_8N1, 16, 17);
    }

    bool receiveMessage() {
        bool received = false;
        while (Serial1.available()) {
            uint8_t c = Serial1.read();
            switch (state) {
                case WAIT_SYNC0:
                    if (c == ESP32P4_SYNC0) state = WAIT_SYNC1;
                    break;
                case WAIT_SYNC1:
                    if (c == ESP32P4_SYNC1) {
                        state = READ_PAYLOAD;
                        idx = 0;
                        checksum = 0;
                    } else {
                        state = (c == ESP32P4_SYNC0) ? WAIT_SYNC1 : WAIT_SYNC0;
                    }
                    break;
                case READ_PAYLOAD:
                    buf[idx++] = c;
                    checksum += c;
                    if (idx >= PAYLOAD_SIZE) {
                        state = READ_CHECKSUM;
                    }
                    break;
                case READ_CHECKSUM:
                    if (c == checksum) {
                        memcpy(&msg, buf, PAYLOAD_SIZE);
                        received = true;
                        // Use received data
                        x_distance = msg.x;
                        y_distance = msg.y;
                    } else {
                        Serial.printf("Checksum error! (got 0x%02X, expected 0x%02X)\n", c, checksum);
                    }
                    state = WAIT_SYNC0;
                    break;
            }
        }
        return received;
    }

    //ceka nez dojde zprava z Raspberry Pi ze je pripraveno
    void WaitForReadyMessage() {
        while (true) {
            if (receiveMessage()) {
                if (msg.camera || msg.on) {
                    rb::Manager::get().leds().green(true);
                    break;
                }
            }
            delay(10);
        }
    }

    //posle zpravu do Raspberry Pi ze je na pozici pro vyfoceni fotky
    void SendInPosstionMessage() {
        Serial1.println("inposition");
    }

    void WaitingForBearPosData() {
        // Ceka na x a y distance medveda
        while (true) {
            if (receiveMessage()) {
                // Pockame az prijdou nenulova data
                if (msg.x != 0 || msg.y != 0 || msg.distance != 0) {
                    x_distance = msg.x;
                    y_distance = msg.y;
                    break;
                }
            }
            delay(10);
        }
    }
};
