#include "SmartServoBus.hpp"
#include "RBCX.h"
#include<Arduino.h>
#include <Arduino.h>

struct Movemennt{

double mm_to_ticks = 0.215; // konstanta pro prepocet tics enkoderu na mm
int wheel_base = 165; // rovor  kol robota
int last_ticks_M3 = 0; // pravy motor
int last_ticks_M2 = 0; // llevy motor


/**
 * Fuknkce pro pohyb robota v oblouku doprava
 * @param angle: jaky uhel oblouku robot ujede
 * @param radius: polomer oblouku vnitrniho kola robota
 *
 * @return None
 */
  void ArcRight(int angle, int radius)
{
  //Nastaveni pocitadla tics na 0 pro oba motory
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  //auto &man = rb::Manager::get();
  double inner_lenght = (((2 * PI * radius) / 360) * angle) / mm_to_ticks; //vzdalenost, kterou musi vnitrni kolo v oblouku ujet v tics enkoderu
  double outer_lenght = (((2 * 3.14 * (radius + wheel_base)) / 360) * angle) / mm_to_ticks; //vzdalenost, kterou musi vnejsi kolo v oblouku ujet v tics enkoderu
  //Serial.println(inner_lenght);
  int outer_sped = 3200; //rychlost vnejsiho kola
  int inner_speed = -(outer_sped * inner_lenght) / outer_lenght; // vypocet pro rychlost vnitrniho kola
  //Serial.println(inner_speed);
  int tics_M2 = 0;
  int tics_M3 = 0;
  //Serial.println(outer_lenght);
  while ((inner_lenght > tics_M3) && (outer_lenght > tics_M2))
  { 
    man.motor(rb::MotorId::M2).speed(-outer_sped);
    man.motor(rb::MotorId::M3).speed(-inner_speed);
    man.motor(rb::MotorId::M2).requestInfo([&tics_M2](rb::Motor &info)
                                           {
            tics_M2 = info.position(); });
    man.motor(rb::MotorId::M3).requestInfo([&tics_M3](rb::Motor &info)
                                           {
            tics_M3 = info.position(); });
    delay(10);
  }
}

void Arcleft(int angle, int radius)
{
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  auto &man = rb::Manager::get();
  double inner_lenght = (((2 * PI * radius) / 360) * angle) / mm_to_ticks;
  double outer_lenght = (((2 * 3.14 * (radius + wheel_base)) / 360) * angle) / mm_to_ticks;
  Serial.println(inner_lenght);
  int outer_sped = -3200;
  int inner_speed = -(outer_sped * inner_lenght) / outer_lenght;
  Serial.println(inner_speed);
  int tics_M2 = 0;
  int tics_M3 = 0;
  Serial.println(outer_lenght);
  while ((inner_lenght > tics_M2) && (outer_lenght > tics_M3))
  {
    man.motor(rb::MotorId::M3).speed(-outer_sped);
    man.motor(rb::MotorId::M2).speed(-inner_speed);
    man.motor(rb::MotorId::M3).requestInfo([&tics_M3](rb::Motor &info)
                                           {
            //printf("M3: position:%d\n", info.position());
            tics_M3 = info.position(); });
    man.motor(rb::MotorId::M2).requestInfo([&tics_M2](rb::Motor &info)
                                           {
            tics_M2 = info.position(); });
    delay(10);
  }
}

void Acceleration(int speed_from, int speed_to, int distance_mm)
{
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  double distance_ticks = distance_mm / mm_to_ticks;
  double acc_const = speed_to / distance_ticks;
  int ticks_M2 = 0;
  int ticks_M3 = 0;
  int error = 0;
  while ((ticks_M2 < distance_ticks) && (ticks_M3 < distance_ticks))
  {
    error = 10 * (ticks_M2 - ticks_M3);
    man.motor(rb::MotorId::M2).speed(-(acc_const * ticks_M2 + speed_from - error));
    man.motor(rb::MotorId::M3).speed(acc_const * ticks_M3 + speed_from + error);
    man.motor(rb::MotorId::M3).requestInfo([&ticks_M3](rb::Motor &info)
                                           {
            //printf("M3: position:%d\n", info.position());
            ticks_M3 = info.position(); });
    man.motor(rb::MotorId::M2).requestInfo([&ticks_M2](rb::Motor &info)
                                           {
            //printf("M2: position:%d\n", info.position());
            ticks_M2 = -info.position(); });

    delay(10);
  }
}

void TurnRight(int angle)
{
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  int ticks_M2 = 0;
  int ticks_M3 = 0;
  int distance = ((PI * wheel_base) / 360) * angle / mm_to_ticks;
  while (distance > ticks_M3)
  {
    man.motor(rb::MotorId::M2).speed(-500);
    man.motor(rb::MotorId::M3).speed(-500);
    man.motor(rb::MotorId::M3).requestInfo([&ticks_M3](rb::Motor &info)
                                           {
            ticks_M3 = -info.position(); });
    man.motor(rb::MotorId::M2).requestInfo([&ticks_M2](rb::Motor &info)
                                           {
            ticks_M2 = info.position(); });

    delay(10);
  }
  man.motor(rb::MotorId::M3).speed(0);
  man.motor(rb::MotorId::M2).speed(0);
}

void TurnLeft(int angle)
{
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  int ticks_M2 = 0;
  int ticks_M3 = 0;
  int distance = ((PI * wheel_base) / 360) * angle / mm_to_ticks;
  while (distance > ticks_M3)
  {
    man.motor(rb::MotorId::M2).speed(500);
    man.motor(rb::MotorId::M3).speed(500);
    man.motor(rb::MotorId::M3).requestInfo([&ticks_M3](rb::Motor &info)
                                           {
            ticks_M3 = info.position(); });
    man.motor(rb::MotorId::M2).requestInfo([&ticks_M2](rb::Motor &info)
                                           {
            ticks_M2 = -info.position(); });

    delay(10);
  }
  man.motor(rb::MotorId::M3).speed(0);
  man.motor(rb::MotorId::M2).speed(0);
}

void BackwardUntillWall(){
  unsigned long start_time = millis();
  bool first_hit = false;
  unsigned long hit_time = 0;

  while (man.buttons().left() == 0 || man.buttons().right() == 0)
  {
    bool left_pressed = (man.buttons().left() == 1);
    bool right_pressed = (man.buttons().right() == 1);

    if (left_pressed || right_pressed) {
      if (!first_hit) {
        first_hit = true;
        hit_time = millis();
        Serial.println("[TLACITKA] Detekován první náraz! Zkracuji čas na srovnání na 2 sekundy.");
      }
    }

    // Vyhodnocení timeoutu
    if (first_hit) {
      if (millis() - hit_time >= 2000) {
        Serial.println("[TLACITKA] Srovnání vypršelo (limit 2 sekundy od prvního nárazu). Končím.");
        break;
      }
    } else {
      if (millis() - start_time >= 5000) {
        Serial.println("[TLACITKA] Hledání stěny vypršelo (limit 5 sekund). Končím.");
        break;
      }
    }

    if (left_pressed) {
      man.motor(rb::MotorId::M3).speed(-3000);
      man.motor(rb::MotorId::M2).speed(1000);
    }
    else if (right_pressed) {
      man.motor(rb::MotorId::M2).speed(3000);
      man.motor(rb::MotorId::M3).speed(-1000);  
    }
    else {
      man.motor(rb::MotorId::M2).speed(2500);
      man.motor(rb::MotorId::M3).speed(-2500);
    }
    delay(10); // Odlehčení procesoru v každém průchodu smyčky
  }
  man.motor(rb::MotorId::M2).speed(0);
  man.motor(rb::MotorId::M3).speed(0);
}

/**
 * Funkce pro pohyb robota rovne 
 * @param speed: rychlost kterou robot pojede
 * @param distance: vzdalenost kterou ma robot ujet v mm
 * @param timeout: po jake dobe se ma funkce ukoncit i kdyz neujede pozadovanou vzdalenost
 * @return None
 */
void Straight(int speed, int distance, int timeout)
{
  man.motor(rb::MotorId::M2).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  int time = 0;
  int ticks_M2 = 0;
  int ticks_M3 = 0;
  int target_ticks = abs(distance / mm_to_ticks);
  Serial.printf("[POHYB] Jízda rovně: rychlost=%d, cíl_tiků=%d\n", speed, target_ticks);
  while (abs(ticks_M2) < target_ticks && time < timeout)
  { 
    man.motor(rb::MotorId::M2).speed(-speed);
    man.motor(rb::MotorId::M3).speed(speed);
    man.motor(rb::MotorId::M3).requestInfo([&ticks_M3](rb::Motor &info)
                                           {
            ticks_M3 =  info.position(); });
    man.motor(rb::MotorId::M2).requestInfo([&ticks_M2](rb::Motor &info)
                                           {
            ticks_M2 =   -info.position(); });

    delay(10);
    time = time + 10;
  }
}

void Stop(){
  man.motor(rb::MotorId::M2).speed(0);
  man.motor(rb::MotorId::M3).speed(0);
}
















































































































};


