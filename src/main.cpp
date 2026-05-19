#include <Arduino.h>
#include "RBCX.h"

void setup() {
    // USB pro vypis do pocitace (Serial Monitor)
    
    // Inicializace RBCX (manager, LEDky atd.)
    auto &man = rb::Manager::get();
    man.install();

    // Inicializace UART1 pro komunikaci s Raspberry Pi
    // RX = pin 16, TX = pin 17
    Serial1.begin(115200, SERIAL_8N1, 16, 17);
    Serial.begin(115200);
    
    
    Serial.println("[STAV: USPECH] UART test pripraven. Cekam na zpravy od Raspberry Pi...");
}

void loop() {
    static unsigned long last_print_time = 0;
    
    // Kdyz prijdou nejaka data pres UART1 od Raspberry Pi
    if (Serial1.available() > 0) {
        String data = Serial1.readStringUntil('\n');
        data.trim(); // Orezani pripadnych mezer a koncu radku (\r)
        
        if (data.length() > 0) {
            // Vypsat prijatou zpravu na pocitac pres USB
            Serial.print("[STAV: USPECH] Prijato z Raspberry: ");
            Serial.println(data);
            
            // Odpovedet Raspberry Pi pres UART1
            size_t bytesSent = Serial1.println("Ahoj Raspberry PI");
            if (bytesSent > 0) {
                Serial.println("[STAV: ODESLANO] Odpoved uspesne odeslana.");
            } else {
                Serial.println("[STAV: CHYBA] Nepodarilo se odeslat odpoved!");
            }
            
            // Zablikat zelenou LED na znak uspesne komunikace
            rb::Manager::get().leds().green(true);
            delay(50);
            rb::Manager::get().leds().green(false);
        } else {
            Serial.println("[STAV: VAROVANI] Prijat prazdny retezec.");
        }
    } else {
        // Pokud data nejsou, kazde 2 vteriny vypiseme ze cekame
        if (millis() - last_print_time > 2000) {
            Serial.println("[STAV: CEKAM] Zadne data z Raspberry Pi zatim nedorazily...");
            last_print_time = millis();
        }
    }
}