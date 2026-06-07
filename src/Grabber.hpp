#include "SmartServoBus.hpp"
#include "RBCX.h"
#include <Arduino.h>
#include <memory>

// pro spravne fungovani SmartServoBus.hpp
using namespace lx16a;
extern std::unique_ptr<SmartServoBus> servoBus;

// struktura grabber obsahuje vsechny funkce pro ovladani grabberu
struct Grabber
{
    // funkce pro vypocet o jaky uhel se ma otocit prave servo
    Angle RightAngle(Angle angle)
    {
        angle = 240_deg - angle;
        return angle;
    }

    // mozne pozice grabberu
    enum grabber_state
    {
        closed,
        open,
        grab,
        half_open
    };

    grabber_state last_state = closed; // vychozi stav je zavreno

    // Inicializace obou serv s limity a parametry AutoStop
    void Init()
    {
        if (!servoBus) return;

        // Servo 0 (levé klepeto): limity 0°-150°
        servoBus->setAutoStop(0, false);
        servoBus->limit(0, Angle::deg(0), Angle::deg(150));
        servoBus->setAutoStopParams(
            SmartServoBus::AutoStopParams{
                .max_diff_centideg = 660,
                .max_diff_readings = 4,
            });
        Serial.println("Smart Servo 0 (levé klepeto) inicializovano (limity: 0-150)");

        // Servo 1 (pravé klepeto): limity 90°-240°
        servoBus->setAutoStop(1, false);
        servoBus->limit(1, Angle::deg(90), Angle::deg(240));
        servoBus->setAutoStopParams(
            SmartServoBus::AutoStopParams{
                .max_diff_centideg = 660,
                .max_diff_readings = 4,
            });
        Serial.println("Smart Servo 1 (pravé klepeto) inicializovano (limity: 90-240)");
    }

    // --- Obecné smart servo funkce ---
    void SmartServoMove(int id, Angle angle, int speed = 200)
    {
        if (!servoBus) return;
        servoBus->setAutoStop(id, false);
        servoBus->set(id, angle, speed);
        Serial.printf("Smart Servo %d move na %.1f stupňů rychlostí %d\n", id, angle.deg(), speed);
    }


    void SmartServoSoftMove(int id, Angle angle, int speed = 200)
    {
        if (!servoBus) return;
        servoBus->setAutoStop(id, true);
        servoBus->set(id, angle, speed);
        Serial.printf("Smart Servo %d soft_move na %.1f stupňů rychlostí %d\n", id, angle.deg(), speed);
    }

    // nastavi grabber na open pozici
    void Open(){
        if (!servoBus) return;
        if(last_state == closed){
            SmartServoMove(0, 0_deg);
            delay(1000); // nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            SmartServoMove(1, RightAngle(0_deg));
        }
        else { // grab
            SmartServoMove(0, 0_deg); 
            SmartServoMove(1, RightAngle(0_deg)); 
        }
        last_state = open;
    }

    // nastavi grabber na close pozici
    void Close(){
        if (!servoBus) return;
        if(last_state == open){
            SmartServoMove(1, RightAngle(140_deg)); 
            delay(1000); // nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            SmartServoMove(0, 145_deg); 
        }
        else { // grab
            SmartServoMove(0, 50_deg); 
            delay(1000);
            SmartServoMove(1, RightAngle(140_deg));
            delay(1000); // nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            SmartServoMove(0, 145_deg); 
        }
        last_state = closed;
    }

    // nastavi grabber na grab pozici
    void Grab(){
        if (!servoBus) return;
        if(last_state == open){
            SmartServoMove(0, 86_deg);
            SmartServoMove(1, RightAngle(81_deg)); 
        }
        else { // close
            SmartServoMove(0, 86_deg); 
            delay(1000); // nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            SmartServoMove(1, RightAngle(81_deg)); 
        }
        last_state = grab;
    }
    
    // nastavi grabber na polovicni open pozici (o 45 stupnu mene nez full open)
    void HalfOpen(){
        if (!servoBus) return;
        
        SmartServoMove(0, 45_deg);
        if(last_state == closed){
            delay(1000); // ochrana pri otevirani z uplneho zavreni
        }
        SmartServoMove(1, RightAngle(45_deg)); 
        
        last_state = half_open;
    }

    void znacka(bool znovu) {
        if (!servoBus) return;
        
        if(znovu){
            SmartServoSoftMove(0, 90_deg); 
            SmartServoSoftMove(1, RightAngle(85_deg)); 
            last_state = grab;
        }
        else{
            SmartServoSoftMove(0, 86_deg); 
            SmartServoSoftMove(1, RightAngle(81_deg)); 
            last_state = grab;
        }
    }

    // Vypíše aktuální fyzické a logické pozice obou smart serv a uvolní jejich torque
    void PrintAndRelease() {
        if (!servoBus) {
            Serial.println("[GRABBER] servoBus je NULL!");
            return;
        }
        
        // Načtení pozic
        Angle p0 = servoBus->pos(0);
        Angle p1 = servoBus->pos(1);
        
        Serial.println("\n=== KALIBRACE CHYTRÝCH SERV (ZNAČKA) ===");
        if (p0.isNaN()) {
            Serial.println("Chyba: Nepodařilo se přečíst pozici Leva (ID 0)!");
        } else {
            Serial.printf("Levy (ID 0): Fyzicky = %.1f° | Logicky = %.1f°\n", p0.deg(), p0.deg());
        }
        
        if (p1.isNaN()) {
            Serial.println("Chyba: Nepodařilo se přečíst pozici Prava (ID 1)!");
        } else {
            // RightAngle(angle) = 240 - angle  => angle = 240 - RightAngle(angle)
            float log1 = 240.0f - p1.deg();
            Serial.printf("Pravy (ID 1): Fyzicky = %.1f° | Logicky = %.1f°\n", p1.deg(), log1);
        }
        
        if (!p0.isNaN() && !p1.isNaN()) {
            float log1 = 240.0f - p1.deg();
            Serial.println("\nNávrh kódu pro funkci znacka():");
            Serial.printf("SmartServoSoftMove(0, %.0f_deg);\n", p0.deg());
            Serial.printf("SmartServoSoftMove(1, RightAngle(%.0f_deg));\n", log1);
        }
        Serial.println("=========================================\n");
        
        // Uvolnění točivého momentu (torque OFF)
        rb::Manager::get().smartServoBusBackend().send(lw::Packet(0, lw::Command::SERVO_LOAD_OR_UNLOAD_WRITE, (uint8_t)0));
        rb::Manager::get().smartServoBusBackend().send(lw::Packet(1, lw::Command::SERVO_LOAD_OR_UNLOAD_WRITE, (uint8_t)0));
        Serial.println("[GRABBER] Serva uvolněna (torque OFF). Nyní je můžete ručně otočit.");
    }
};

