#include "SmartServoBus.hpp"
#include "RBCX.h"
#include <Arduino.h>
#include <thread>
#include <stdarg.h>

auto &man = rb::Manager::get(); //pro fungovani RBCX

String logBuffer = "";

void logMsg(const char* format, ...) {
    char temp[256];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    
    logBuffer += temp;
    logBuffer += "\n";
    Serial.println(temp);
}
#include "Grabber.hpp"
std::unique_ptr<SmartServoBus> servoBus;
#include"Comunication.hpp"
#include "Movement.hpp"
#include "movement_my.hpp"
Grabber grabber;
Communication message;
Movemennt move;
Motors motors;

uint32_t gyro_reset_time = 0;
float gyro_z_offset = 0.0f;
float gyro_polarity = 1.0f;
float gyro_bear_angle = 0.0f;
float gyro_accumulated_offset = 0.0f;
float first_search_angle = 0.0f;
bool first_search_angle_saved = false;

float gyro_angle_initial = 0.0f;
float gyro_angle_bear_found = 0.0f;
float gyro_angle_after_grab_back = 0.0f;

float getGyroAngleZ();
float getAbsoluteGyroAngleZ();
void resetGyroZ();

// Uživatelská funkce pro výpočet a provedení otočení při návratu.
void perform_return_turn(float initial_angle, float bear_angle, float final_angle) {
    // 1. Výpočet cíle (standardní target_angle)
    float target_angle = (bear_angle > 90.0f) ? 80.0f : 90.0f;
    
    // Rozdíl mezi konečným úhlem po odcouvání a úhlem při nalezení medvěda
    float diff = final_angle - bear_angle;
    
    // Normalizace diff do [-180, 180]
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    
    logMsg("[POHYB] Vyhodnocení rozdílu úhlů: bear_angle=%.2f°, final_angle=%.2f°, diff=%.2f°", 
           bear_angle, final_angle, diff);
           
    // Prahová hodnota pro drastickou změnu (např. 7.5 stupně)
    const float diff_threshold = 7.5f;
    
    if (std::abs(diff) >= diff_threshold) {
        logMsg("[POHYB] Detekována drastická změna úhlu (>= %.1f°). Spouštím rozdělený návrat.", diff_threshold);
        
        // A. Otočení zpět na původní úhel (bear_angle)
        // Musíme se otočit o -diff stupňů
        float correction_angle = -diff;
        while (correction_angle > 180.0f) correction_angle -= 360.0f;
        while (correction_angle < -180.0f) correction_angle += 360.0f;
        
        if (correction_angle > 2.0f) {
            logMsg("  -> Narovnávám na původní směr: otočení o %.1f° DOLEVA...", correction_angle);
            motors.turn_on_spot_left(correction_angle, 40.0f);
        } else if (correction_angle < -2.0f) {
            logMsg("  -> Narovnávám na původní směr: otočení o %.1f° DOPRAVA...", std::abs(correction_angle));
            motors.turn_on_spot_right(std::abs(correction_angle), 40.0f);
        }
        
        // B. Odcouvání dalších 20 cm (200 mm) v původním směru
        logMsg("  -> Couvám dalších 200 mm pro bezpečné vzdálení od stěny...");
        motors.backward_acc(400.0f, 30.0f);
        
        // C. Konečné dotočení na target_angle
        // Načteme aktuální úhel z gyroskopu
        float current_heading = getAbsoluteGyroAngleZ() * gyro_polarity;
        float remaining_angle = target_angle - current_heading;
        while (remaining_angle > 180.0f) remaining_angle -= 360.0f;
        while (remaining_angle < -180.0f) remaining_angle += 360.0f;
        
        if (remaining_angle > 2.0f) {
            logMsg("  -> Finální dotočení k pravé stěně: o %.1f° DOLEVA...", remaining_angle);
            motors.turn_on_spot_left(remaining_angle, 40.0f);
        } else if (remaining_angle < -2.0f) {
            logMsg("  -> Finální dotočení k pravé stěně: o %.1f° DOPRAVA...", std::abs(remaining_angle));
            motors.turn_on_spot_right(std::abs(remaining_angle), 40.0f);
        } else {
            logMsg("  -> Již jsme nasměrováni správně (odchylka %.2f°).", remaining_angle);
        }
        
    } else {
        logMsg("[POHYB] Změna úhlu je malá (< %.1f°). Provádím standardní dotočení.", diff_threshold);
        
        // Standardní dotočení z aktuálního úhlu
        float remaining_angle = target_angle - final_angle;
        while (remaining_angle > 180.0f) remaining_angle -= 360.0f;
        while (remaining_angle < -180.0f) remaining_angle += 360.0f;
        
        if (remaining_angle > 2.0f) {
            logMsg("  -> Dotáčím se o %.1f° DOLEVA...", remaining_angle);
            motors.turn_on_spot_left(remaining_angle, 40.0f);
        } else if (remaining_angle < -2.0f) {
            logMsg("  -> Dotáčím se o %.1f° DOPRAVA...", std::abs(remaining_angle));
            motors.turn_on_spot_right(std::abs(remaining_angle), 40.0f);
        } else {
            logMsg("  -> Již jsme nasměrováni správně (odchylka %.2f°).", remaining_angle);
        }
    }
}

float getGyroAngleZ() {
    float raw = man.mpu().getAngleZ();
    float dt = (millis() - gyro_reset_time) / 1000.f;
    return raw - (gyro_z_offset * dt);
}

float getAbsoluteGyroAngleZ() {
    return gyro_accumulated_offset + getGyroAngleZ();
}

void resetGyroZ() {
    if (gyro_reset_time != 0) {
        gyro_accumulated_offset += getGyroAngleZ();
    }
    man.mpu().resetAngleZ();
    gyro_reset_time = millis();
}

void gyro_sender_task(void *pvParameters) {
    while (true) {
        float current_angle = getAbsoluteGyroAngleZ() * gyro_polarity;
        Serial1.printf("gyro:%.2f,%.2f,%.2f\n", gyro_angle_initial, gyro_angle_bear_found, current_angle);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void calibrateGyro() {
    Serial.println("[GYRO] Inicializace MPU (gyroskopu)...");
    man.mpu().init();
    man.mpu().sendStart();
    Serial.println("[GYRO] Čekám 1 sekundu na stabilizaci gyroskopických dat...");
    delay(1000);
    
    Serial.println("[GYRO] Spouštím přesnou kalibraci driftu (UDRŽUJTE ROBOTA V KLIDU)...");
    man.mpu().clearCalibrationData();
    delay(100);
    man.mpu().setCalibrationData();
    delay(100);
    
    float sum = 0.0f;
    const int samples = 100;
    for (int i = 0; i < samples; i++) {
        sum += man.mpu().getGyroZ();
        delay(20);
    }
    gyro_z_offset = sum / (float)samples;
    Serial.printf("[GYRO] Kalibrace dokončena. Změřený zbytkový drift: %.4f °/s\n", gyro_z_offset);
}

// IR senzory na GPIO 27 a 14
const uint8_t IR_PIN_1 = 27;
const uint8_t IR_PIN_2 = 14;

bool open_grabber = false; //promena ridici otevirani grabberu v threadu 


enum Sector
{
  blue,
  red,
  yellow,
  green
};
Sector sector = blue; //vychozi sektor je blue

//funkce pro jizdu do hriste
void GoToField(){
  move.Acceleration(300,32000,400);
  move.ArcRight(180,180);
  open_grabber = true; //otevírání grabberu ihned po rozjezdu rovně
  move.Straight(32000,100,5000);
  move.Arcleft(161, 150);
  move.Straight(20000, 990,4000); // +500 mm dál do hřiště
  move.Acceleration(20000, 100, 340);
  move.Stop();
}

void GoForBear(int x, int y){
  x=x*10;
  y=y*10;
  Serial.printf("[MAIN] Přepočítané milimetrové souřadnice pro jízdu za medvědem: x = %d mm, y = %d mm\n", x, y);

  //sector blue
  if (x < 400){
    sector = blue;
    Serial.println("[MAIN] Cíl leží v MODRÉM sektoru (x < 400).");
    if (y>1400){
      Serial.println("[MAIN] Omezuji y na max 1300 mm.");
      y=1300;
    }
    Serial.printf("[POHYB] Jedeme rovně %d mm k medvědovi.\n", y);
    move.Straight(2000,y,4000);
    move.Stop();
    Serial.println("[KLEPETA] Zavíráme klepeta (chytáme medvěda)...");
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }

  //sector red
  if (x >= 400 && x <= 600){
    sector = red;
    Serial.println("[MAIN] Cíl leží v ČERVENÉM sektoru (400 <= x <= 600).");
    if (y>1400){
      Serial.println("[MAIN] Omezuji y na max 1300 mm.");
      y=1300;
    }
    Serial.println("[POHYB] Jedeme rovně 100 mm.");
    move.Straight(2000,100,4000);
    Serial.println("[POHYB] Otáčíme se doprava o 45 stupňů.");
    move.TurnRight(45);
    Serial.println("[POHYB] Jedeme rovně 400 mm.");
    move.Straight(2000,400,4000);
    Serial.println("[POHYB] Otáčíme se doleva o 45 stupňů.");
    move.TurnLeft(45);
    Serial.printf("[POHYB] Jedeme rovně zbývajících %d mm k medvědovi.\n", y-200);
    move.Straight(2000,y-200,4000);
    move.Stop();
    Serial.println("[KLEPETA] Zavíráme klepeta (chytáme medvěda)...");
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
    Serial.println("[POHYB] Otáčíme se doprava o 90 stupňů.");
    move.TurnRight(90);
  }

  //sector yellow
  if (x > 600 && y>300){
    sector = yellow;
    Serial.println("[MAIN] Cíl leží ve ŽLUTÉM sektoru (x > 600, y > 300).");
    if(y>1400){
      Serial.println("[MAIN] Omezuji y na max 1300 mm.");
      y=1300;
    }
    Serial.printf("[POHYB] Jedeme rovně %d mm.\n", y);
    move.Straight(2000,y,4000);
    Serial.println("[POHYB] Otáčíme se doprava o 90 stupňů.");
    move.TurnRight(90);
    if(x>1400){
      Serial.println("[MAIN] Omezuji x na max 1300 mm.");
      x=1300;
    }
    Serial.printf("[POHYB] Jedeme rovně %d mm k medvědovi.\n", x);
    move.Straight(2000,x,4000);
    move.Stop();
    Serial.println("[KLEPETA] Zavíráme klepeta (chytáme medvěda)...");
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }

  //sector green
  if (x > 600 && y<=300){
    sector = green;
    Serial.println("[MAIN] Cíl leží v ZELENÉM sektoru (x > 600, y <= 300).");
    if(y<300){
      Serial.println("[MAIN] Omezuji y na min 300 mm.");
      y=300;
      }
    Serial.printf("[POHYB] Jedeme rovně %d mm.\n", y);
    move.Straight(2000,y,4000);
    Serial.println("[POHYB] Otáčíme se doprava o 90 stupňů.");
    move.TurnRight(90);
    if(x>1400){
      Serial.println("[MAIN] Omezuji x na max 1300 mm.");
      x=1300;
    }
    Serial.printf("[POHYB] Jedeme rovně %d mm k medvědovi.\n", x);
    move.Straight(2000,x,4000);
    move.Stop();
    Serial.println("[KLEPETA] Zavíráme klepeta (chytáme medvěda)...");
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }
}

//funkce pro jizdu zpet do domovske pozice
void GoHome(bool skip_initial_alignment = false){
  if (!skip_initial_alignment) {
      Serial.println("[POHYB] GoHome: Otáčím se DOPRAVA o 90 stupňů...");
      move.TurnRight(90);
      Serial.println("[POHYB] GoHome: Couvám ke spodní stěně bludiště pomocí back_buttons...");
      motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
      
      Serial.println("[POHYB] GoHome: Dělám levý oblouk (poloměr 100 mm, 90 stupňů) pro odjezd od zdi...");
      move.Arcleft(90, 100);
      Serial.println("[POHYB] GoHome: Jedu rovně 80 mm...");
      motors.forward_acc(80.0f, 40.0f);
  } else {
      Serial.println("[POHYB] GoHome: Již jsme vyrovnaní u spodní stěny (rychlá trasa). Přeskakuji počáteční otočení.");
      Serial.println("[POHYB] GoHome: Dělám PRVNÍ levý oblouk (poloměr 100 mm, 90 stupňů)...");
      move.Arcleft(90, 100);
      
      // Dodatečné vyrovnání o pravou stěnu (v rychlé trase jsme na spodní stěně v náhodné X pozici)
      Serial.println("[POHYB] GoHome: Couvám k PRAVÉ stěně pomocí back_buttons...");
      motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
      
      Serial.println("[POHYB] GoHome: Popojíždím 300 mm dopředu (doleva) od pravé stěny pro vytvoření druhého rádiusu...");
      motors.forward_acc(300.0f, 40.0f);
  }
  Serial.println("[POHYB] GoHome: Dělám pravý oblouk (90 stupňů, poloměr 300 mm)...");
  move.ArcRight(84, 300);
  Serial.println("[POHYB] GoHome: Jedu rovně 100 mm...");
  move.Straight(2000, 100, 99999);
  Serial.println("[POHYB] GoHome: Dělám levý oblouk (190 stupňů, poloměr 180 mm)...");
  move.Arcleft(190, 180);
  Serial.println("[POHYB] GoHome: Jedu rovně 200 mm...");
  move.Straight(2000, 200, 99999);
}

//ceka na zmacknuti on tlacitka pak program pokracuje
void WaitEorStart(){
  uint32_t last_ir_print = 0;
  while (true){
    if (man.buttons().on() == 1){
      break;
    }
    
    if (millis() - last_ir_print > 500) {
      int val1 = analogRead(IR_PIN_1);
      int val2 = analogRead(IR_PIN_2);
      uint32_t us1 = man.ultrasound(0).measure();
      uint32_t us2 = man.ultrasound(1).measure();
      Serial.printf("[IR SENSORS] GPIO 27 = %d, GPIO 14 = %d | US1 = %u mm, US2 = %u mm\n", val1, val2, us1, us2);
      last_ir_print = millis();
    }

    // Debug zadních tlačítek / mikrospínačů
    if (man.buttons().left() == 1 || man.buttons().right() == 1) {
      Serial.printf("[TLACITKA] Stisknuto zadní tlačítko: Levý mikrospínač = %d, Pravý mikrospínač = %d\n", 
                    man.buttons().left(), man.buttons().right());
      delay(200); // Ochrana proti zaspamování konzole
    }
    if (man.buttons().up() == 1) {
      Serial.printf("[TLACITKA] Stavy zadnich mikrospinacu: Levy = %d, Pravy = %d\n", 
                    man.buttons().left(), man.buttons().right());
      delay(200); // Ochrana proti zaspamování konzole
    }
    if (man.buttons().down() == 1) {
      Serial.println("[TLACITKA] Stisknuto tlacitko DOWN -> Zavirame klepeta uplne (Close)");
      grabber.Close();
      delay(500); // Ochrana proti opakovanemu provedeni
    }
    delay(10);
  }
}

//funkce pro otevirani grabberu po ceste do hriste
void OpenGrabberBeforeField(){
  while (true)
  {
    if(open_grabber == true){
      break;
    }
    delay(50);
  }
  delay(460);
  Serial.println("[KLEPETA] Otevíráme klepeta do přípravné polohy (HalfOpen)...");
  grabber.HalfOpen();
}

// Nová strategie vyhledávání medvěda:
// Pokus 1: Z výchozí pozice se otočíme -10° doprava, pak zametáme 170° doleva (rychle tam, pomalu zpět)
// Pokus 2: Otočíme se na 90° doleva, popojedeme 30 cm dopředu, pak plných 360°
int FindBear() {
  float search_speed_forward = 6.0f; // Rychlost hledání tam
  float search_speed_back = 3.0f;    // Rychlost hledání zpět
  float min_speed = 3.0f;            // Rychlost jemných kroků (vycentrování)
  int centered_consecutive_count = 0;
  const int target_consecutive_confirmations = 3;
  const int centered_threshold = 15; // Tolerance pro vycentrování (v mm)
  const int step_zone_threshold = 40; // Odchylka v mm, pod kterou přejdeme do krokového režimu
  
  first_search_angle = 0.0f;
  first_search_angle_saved = false;
  
  gyro_angle_initial = getAbsoluteGyroAngleZ() * gyro_polarity;
  gyro_angle_bear_found = 0.0f;
  gyro_angle_after_grab_back = 0.0f;
  
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  const int ticks_10 = max_ticks_90 * 10 / 90;
  
  for (int attempt = 1; attempt <= 2; attempt++) {
      // === PŘÍPRAVA POZICE PŘED ZAMETÁNÍM ===
      int sweep_end_ticks = 0;   // Kam zametáme (v tikách enkodéru)
      int sweep_start_ticks = 0; // Odkud se vracíme zpět
      float current_forward_speed = search_speed_forward;
      float current_back_speed = search_speed_back;

      if (attempt == 1) {
          // Pokus 1: Otočíme se 10° doprava a pak zametáme 180° doleva (-10° až +170°)
          Serial.println("[MAIN] Hledání medvěda - 1. pokus: Rozsah -10° až +170°");
          
          Serial.println("[MAIN] Otáčím se pomalu 10° DOPRAVA pouze pravým kolem (s detekcí)...");
          resetGyroZ();
          
          // Vyčistíme předchozí zprávy, abychom nereagovali na stará/stojící data
          message.clearRxBuffer();
          
          // Pravé kolo jede dozadu (speed = -4%), levé stojí (speed = 0)
          man.motor(rb::MotorId::M2).speed(0);
          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-4));
          
          uint32_t pre_turn_send_time = 0;
          bool bear_found_pre_turn = false;
          
          while (std::abs(getGyroAngleZ()) < 10.0f) {
              // Posíláme inposition do RPi
              if (millis() - pre_turn_send_time > 100) {
                  message.SendInPosstionMessage();
                  pre_turn_send_time = millis();
              }
              
              // Zpracování zpráv z RPi
              if (message.receiveMessage()) {
                  if (message.msg.x != 0 || message.msg.y != 0 || message.msg.distance != 0) {
                      int x = message.x_distance;
                      int y = message.y_distance;
                      
                      Serial.printf("[PRE-TURN RX] Nalezen medvěd během počátečního otáčení: x=%d mm, y=%d mm\n", x, y);
                      
                      if (std::abs(x) <= centered_threshold) {
                          centered_consecutive_count++;
                          if (centered_consecutive_count >= target_consecutive_confirmations) {
                              man.motor(rb::MotorId::M2).speed(0);
                              man.motor(rb::MotorId::M3).speed(0);
                              Serial.printf("[PRE-TURN] Medvěd vycentrován a potvrzen! x=%d, y=%d\n", x, y);
                              bear_found_pre_turn = true;
                              break;
                          }
                      } else {
                          centered_consecutive_count = 0;
                          // Jemná korekce směru
                          if (x > 0) {
                              man.motor(rb::MotorId::M2).speed(0);
                              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-min_speed));
                          } else {
                              man.motor(rb::MotorId::M2).speed(0);
                              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(min_speed));
                          }
                      }
                  }
              }
              delay(10);
          }
          
          man.motor(rb::MotorId::M2).speed(0);
          man.motor(rb::MotorId::M3).speed(0);
          
          if (bear_found_pre_turn) {
              int current_ticks = 0;
              man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                  current_ticks = info.position();
              });
              delay(100);
              if (!first_search_angle_saved) {
                  first_search_angle = getAbsoluteGyroAngleZ() * gyro_polarity;
                  first_search_angle_saved = true;
                  gyro_angle_bear_found = first_search_angle;
                  Serial.printf("[GYRO SAVED] Ukládám první úhel nalezení (v pre-turnu): %.2f°\n", first_search_angle);
              }
              return current_ticks;
          }
          
          // Nastavíme enkodér na -ticks_10 (jsme nyní na -10°)
          man.motor(rb::MotorId::M3).setCurrentPosition(-ticks_10);
          
          sweep_start_ticks = -ticks_10;              // -10°
          sweep_end_ticks = max_ticks_90 * 170 / 90;  // +170°
          
      } else {
          // Pokus 2: Otočíme se na 90° DOLEVA a popojedeme 30 cm, pak 360°
          Serial.println("[MAIN] 1. pokus selhal. Otáčím se na 90° DOLEVA a popojíždím 30 cm...");
          
          motors.turn_on_spot_left(90, 40.0f);
          delay(100);
          motors.forward(300, 40.0f);
          delay(100);
          
          // Reset gyro a enkodéry
          resetGyroZ();
          gyro_angle_initial = getAbsoluteGyroAngleZ() * gyro_polarity;
          delay(100);
          man.motor(rb::MotorId::M3).setCurrentPosition(0);
          
          Serial.println("[MAIN] Hledání medvěda - 2. pokus: Plných 360°");
          sweep_start_ticks = 0;
          sweep_end_ticks = max_ticks_90 * 360 / 90; // +360°
          current_forward_speed = search_speed_back; // pomalu
      }

      // Vyčistíme stará data
      message.clearRxBuffer();

      // Spustíme počáteční otáčení doleva
      int direction = 1;
      man.motor(rb::MotorId::M2).speed(0);
      man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_forward_speed));
      
      int current_ticks = (attempt == 1) ? -ticks_10 : 0;
      uint32_t last_send_time = 0;
      uint32_t last_info_time = 0;
      uint32_t last_bear_seen_time = millis();
      centered_consecutive_count = 0;
      bool sweep_failed = false;
      
      // Pro 2. pokus: pass 1 = 0->180°, pass 2 = 180°->-180° (celkem 360°)
      int pass = 1;

      while(true) {
          // 1. Aktuální pozice pravého kola
          if (millis() - last_info_time > 20) {
              man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                  current_ticks = info.position();
              });
              last_info_time = millis();
          }
          
          // === KONTROLA LIMITŮ OTÁČENÍ ===
          if (attempt == 1) {
              // Pokus 1: -10° až +170° tam a zpět
              if (direction == 1 && current_ticks > sweep_end_ticks) {
                  Serial.printf("[MAIN] Dosažen +170° (%d tiků). Otáčím se ZPĚT...\n", current_ticks);
                  direction = -1;
                  man.motor(rb::MotorId::M2).speed(0);
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_back_speed));
              } 
              else if (direction == -1 && current_ticks < sweep_start_ticks) {
                  Serial.printf("[MAIN] Zpět na -10° (%d tiků). Pokus 1 ukončen.\n", current_ticks);
                  // Vrátíme se na 0°
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_back_speed));
                  uint32_t last_req = 0;
                  while (true) {
                      if (millis() - last_req > 20) {
                          man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                              current_ticks = info.position();
                          });
                          last_req = millis();
                      }
                      if (current_ticks >= -50 && current_ticks <= 50) break;
                      delay(5);
                  }
                  man.motor(rb::MotorId::M3).speed(0);
                  Serial.println("[MAIN] Vráceno na 0°.");
                  break;
              }
          } else {
              // Pokus 2: plných 360° (pass 1: 0->180°, pass 2: 180°->-180°)
              if (pass == 1 && direction == 1 && current_ticks > sweep_end_ticks) {
                  Serial.printf("[MAIN] Dosažen +180° (%d tiků). Pokračuji zpět na -180°...\n", current_ticks);
                  pass = 2;
                  direction = -1;
                  int target_minus_180 = -max_ticks_90 * 180 / 90;
                  sweep_start_ticks = target_minus_180;
                  man.motor(rb::MotorId::M2).speed(0);
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_back_speed));
              }
              else if (pass == 2 && direction == -1 && current_ticks < sweep_start_ticks) {
                  Serial.printf("[MAIN] Dosažen -180° (%d tiků). Pokus 2 ukončen.\n", current_ticks);
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_back_speed));
                  uint32_t last_req = 0;
                  while (true) {
                      if (millis() - last_req > 20) {
                          man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                              current_ticks = info.position();
                          });
                          last_req = millis();
                      }
                      if (current_ticks >= -50 && current_ticks <= 50) break;
                      delay(5);
                  }
                  man.motor(rb::MotorId::M3).speed(0);
                  Serial.println("[MAIN] Vráceno na 0°.");
                  sweep_failed = true;
                  break;
              }
          }

          // 2. Posíláme "inposition" do RPi
          if (millis() - last_send_time > 100) {
              message.SendInPosstionMessage();
              last_send_time = millis();
          }

          // 3. Příjem dat z RPi
          if (message.receiveMessage()) {
              if (message.msg.x != 0 || message.msg.y != 0 || message.msg.distance != 0) {
                  int x = message.x_distance;
                  int y = message.y_distance;
                  
                  last_bear_seen_time = millis();
                  
                  float current_search_speed = (direction == 1) ? current_forward_speed : current_back_speed;

                  Serial.printf("[MAIN RX] x=%d mm, y=%d mm, shoda=%d/%d, tiky=%d, smer=%d\n", 
                                x, y, 
                                centered_consecutive_count + (abs(x) <= centered_threshold ? 1 : 0), 
                                target_consecutive_confirmations, current_ticks, direction);
                  
                  // Vycentrování
                  if (abs(x) <= centered_threshold) {
                      centered_consecutive_count++;
                      man.motor(rb::MotorId::M2).speed(0);
                      man.motor(rb::MotorId::M3).speed(0);
                      
                      if (centered_consecutive_count >= target_consecutive_confirmations) {
                          Serial.printf("[MAIN] Medvěd potvrzen %dx! x=%d, y=%d, tiky=%d.\n", 
                                        target_consecutive_confirmations, x, y, current_ticks);
                          if (!first_search_angle_saved) {
                              first_search_angle = getAbsoluteGyroAngleZ() * gyro_polarity;
                              first_search_angle_saved = true;
                              gyro_angle_bear_found = first_search_angle;
                              Serial.printf("[GYRO SAVED] Ukládám první úhel nalezení: %.2f°\n", first_search_angle);
                          }
                          return current_ticks;
                      }
                      continue;
                  } else {
                      centered_consecutive_count = 0;
                  }
                  
                  // Krokový režim
                  if (abs(x) <= step_zone_threshold) {
                      if (x > 0) {
                          man.motor(rb::MotorId::M2).speed(0);
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-min_speed));
                      } else {
                          man.motor(rb::MotorId::M2).speed(0);
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(min_speed));
                      }
                      delay(40);
                      man.motor(rb::MotorId::M2).speed(0);
                      man.motor(rb::MotorId::M3).speed(0);
                      delay(50);
                  } else {
                      float current_speed = (abs(x) / 300.0f) * current_search_speed;
                      if (current_speed < min_speed) current_speed = min_speed;
                      if (current_speed > current_search_speed) current_speed = current_search_speed;
                      
                      if (x > 0) {
                          man.motor(rb::MotorId::M2).speed(0);
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_speed));
                      } else {
                          man.motor(rb::MotorId::M2).speed(0);
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_speed));
                      }
                  }
              }
          }

          // Ztráta medvěda -> obnovení zametání
          if (millis() - last_bear_seen_time > 500) {
              float speed = (direction == 1) ? current_forward_speed : -current_back_speed;
              man.motor(rb::MotorId::M2).speed(0);
              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(speed));
              centered_consecutive_count = 0;
          }

          delay(10);
      }
      
      if (sweep_failed) break;
  }

  Serial.println("[MAIN] Všechny pokusy o vyhledání medvěda selhaly.");
  return -9999;
}
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("[MAIN] Robot se spouští. Inicializace RBCX...");

  //pro fungovani RBCX (vypneme failsafe, protože vyhledávání blokuje na UART zprávách)
  man.install(rb::MAN_DISABLE_MOTOR_FAILSAFE);

  // Inicializace MPU (gyroskopu) s přesnou kalibrací driftu
  calibrateGyro();
  resetGyroZ();

  // Inicializace infračervených senzorů jako ve Fotonovi
  pinMode(IR_PIN_1, INPUT);
  pinMode(IR_PIN_2, INPUT);

  Serial.println("[SERVOS] Inicializace SmartServoBus pres STM koprocesor...");
  //parametry pro komunikaci se servy 
  servoBus = std::make_unique<SmartServoBus>();
  servoBus->begin(2, &man.smartServoBusBackend()); 

  servoBus->setAutoStop(0, false);//vypne autostop leveho serva 
  servoBus->setAutoStop(1, false);//vypne autostop praveho serva
  Serial.println("[SERVOS] Inicializace serv dokončena.");
  
  // Zapneme komunikaci s RPi (na vyhrazenem Serial1)
  Serial.println("[MAIN] Spouštím trvalou komunikaci s Raspberry Pi (Serial1)...");
  message.start();

  // Spustíme pozadový úkol pro odesílání gyroskopu na Raspberry Pi
  xTaskCreate(gyro_sender_task, "gyro_sender_task", 2048, NULL, 1, NULL);
  Serial.println("[MAIN] Pozadový úkol gyro_sender_task spuštěn.");

//po zapnuti ceka na zpravu od Raspberry Pi ze je ready
//message.WaitForReadyMessage();
  // Odblokování serva s kamerou ihned při startu
  man.stupidServo(0).disable();
  Serial.println("[MAIN] Servo s kamerou odblokováno.");
  Serial.println("[MAIN] Čekám na výběr startu: UP (celá jízda), ON (jen vyhledávání), DOWN (zavřít klepeta)...");
  
  bool run_entire_ride = false;
  while(true){
     if (man.buttons().up() == 1) {
       logBuffer = "";
       Serial.println("[TLACITKA] Tlačítko UP stisknuto! Startujeme celou jízdu...");
       run_entire_ride = true;
       break;
     }
     if (man.buttons().on() == 1) {
       logBuffer = "";
       Serial.println("[TLACITKA] Tlačítko ON stisknuto! Startujeme pouze vyhledávání...");
       run_entire_ride = false;
       break;
     }
    if (man.buttons().down() == 1) {
      Serial.println("[TLACITKA] Tlačítko DOWN stisknuto -> Zavírám klepeta (Close)...");
      grabber.Close();
      delay(500); // Ochrana proti zaspamování, zůstáváme ve smyčce
      Serial.println("[MAIN] Čekám na další výběr: UP, ON, DOWN...");
    }
    
    // Debug zadních mikrospínačů během čekání
    if (man.buttons().left() == 1 || man.buttons().right() == 1) {
      Serial.printf("[TLACITKA] Stisknuto zadní tlačítko: Levý mikrospínač = %d, Pravý mikrospínač = %d\n", 
                    man.buttons().left(), man.buttons().right());
      delay(200);
    }
    
    // Výpis IR senzorů a gyroskopu každých 500 ms pro diagnostiku před jízdou
    static uint32_t last_setup_ir_print = 0;
    if (millis() - last_setup_ir_print > 500) {
      int val1 = analogRead(IR_PIN_1);
      int val2 = analogRead(IR_PIN_2);
      float angleZ = getGyroAngleZ();
      uint32_t us1 = man.ultrasound(0).measure();
      uint32_t us2 = man.ultrasound(1).measure();
      Serial.printf("[SETUP] GPIO 27 = %d, GPIO 14 = %d | Gyro Z = %.2f° | US1 = %u mm, US2 = %u mm\n", val1, val2, angleZ, us1, us2);
      last_setup_ir_print = millis();
    }
    
    delay(10);
  }
  delay(100);
  
  if (run_entire_ride) {
      // 1. Cesta do hřiště (GoToField)
      Serial.println("[MAIN] Startuji jízdu do hřiště (GoToField)...");
      std::thread t1(OpenGrabberBeforeField);//thread pro otevirani grabberu za jizdy
      GoToField(); //cesta do hriste
      t1.join();
      Serial.println("[MAIN] Robot dojel do hřiště.");
  } else {
      Serial.println("[MAIN] Přeskakuji jízdu do hřiště (GoToField) a startuji přímo vyhledávání.");
  }
  
  // Rovnou nastavíme klepeta do přípravné polohy (jako by normálně dojel do hřiště)
  Serial.println("[KLEPETA] Nastavuji klepeta do přípravné polohy (HalfOpen) před zahájením detekce...");
  grabber.HalfOpen();

  Serial.println("[MAIN] Čekám 150 ms na ustálení robota před hledáním medvěda...");
  delay(150);
  gyro_accumulated_offset = 0.0f;
  gyro_reset_time = 0; // Vynutíme, aby resetGyroZ() nepřičítalo předchozí úhel
  resetGyroZ();
  delay(50); // Krátká pauza pro ustálení resetu
  logMsg("[MAIN] Nulový úhel nastaven. Gyro Z před vyhledáváním (nulový stupeň) = %.2f°", getAbsoluteGyroAngleZ());

  // Spuštění plynulého vyhledávání medvěda
  int search_ticks = FindBear();
  
  if (search_ticks == -9999) {
      Serial.println("[MAIN] Detekce selhala nebo prekrocila 90 stupnu. Ukoncuji jizdu.");
      while(true) {
          delay(100);
      }
  }


  // Výpočet úhlu natočení podle enkodérů
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  float search_angle_ticks = (search_ticks * 90.0f) / max_ticks_90;
  
  // Načtení úhlu z gyroskopu
  float search_angle_raw = getGyroAngleZ();

  logMsg("[VÝPOČET GYROSKOPU - HLEDÁNÍ]");
  logMsg("  - Hledání dokončeno, search_ticks = %d", search_ticks);
  logMsg("  - max_ticks_90 = %d", max_ticks_90);
  logMsg("  - Vypočtený úhel z enkodérů (search_angle_ticks) = %.2f°", search_angle_ticks);
  logMsg("  - Raw úhel z gyroskopu (search_angle_raw) = %.2f°", search_angle_raw);
  
  // Detekce polarity gyroskopu vůči enkodérům (pouze pokud se robot reálně pohnul o více než 5 stupňů)
  gyro_polarity = 1.0f;
  if (std::abs(search_angle_ticks) > 5.0f && std::abs(search_angle_raw) > 5.0f) {
      if ((search_angle_ticks > 0 && search_angle_raw < 0) || (search_angle_ticks < 0 && search_angle_raw > 0)) {
          gyro_polarity = -1.0f;
          logMsg("  - ZJIŠTĚNA INVERZNÍ POLARITA GYROSKOPU. Nastavuji koeficient polarity na -1.0.");
      } else {
          gyro_polarity = 1.0f;
          logMsg("  - Polarita gyroskopu souhlasí s enkodéry (koeficient 1.0).");
      }
  } else {
      logMsg("  - Malý úhel natočení (ticks=%.2f°, raw=%.2f°). Používám výchozí koeficient polarity 1.0.", search_angle_ticks, search_angle_raw);
  }
  
  float search_angle = first_search_angle;
  gyro_bear_angle = search_angle;
  logMsg("[GYRO] Srovnaný úhel k medvědovi: Gyro z prvního vyhledání (použito) = %.2f°, Ticks = %.2f° (Raw: %.2f°)", 
                search_angle, search_angle_ticks, search_angle_raw);

  // Rozsvítit MODROU (a pro jistotu i zelenou) LED pro indikaci nalezení cíle
  man.leds().blue(true);
  man.leds().green(true);

  // Podrobné výpisy souřadnic po srovnání
  logMsg("[MAIN] Konečné souřadnice k medvědovi po srovnání: x=%d, y=%d, úhel=%.1f°, tiky=%d...", 
                message.x_distance, message.y_distance, search_angle, search_ticks);
  
  delay(200); // Krátká pauza na vizualizaci LED
  
  // Zhasnout diody před jízdou
  man.leds().blue(false);
  man.leds().green(false);

  // Otevření klepet pouze na polovinu (HalfOpen) podle nových pravidel
  logMsg("[KLEPETA] Otevírám klepeta pouze částečně (HalfOpen)...");
  grabber.HalfOpen();
  
  // Jízda pro medvěda:
  int y_target = message.y_distance;
  if (y_target < 100) {
      y_target = 100;
  }
  Serial.printf("[POHYB] Jedeme rovně minimální vypočítanou vzdálenost %d mm k medvědovi s akcelerací...\n", y_target);
  motors.forward_acc(y_target, 80);
  
  // Dojezd podle IR senzorů
  Serial.println("[POHYB] Začínám dojíždění k medvědovi podle IR senzorů (GPIO 27 a 14)...");
  
  // Rozjedeme motory dopředu pomalou rychlostí 30 %
  man.motor(rb::MotorId::M2).speed(motors.pctToSpeed(-30.0f));
  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(30.0f));
  
  uint32_t start_slow_drive = millis();
  const int ir_threshold = 2000;
  
  while (true) {
      int val1 = analogRead(IR_PIN_1);
      int val2 = analogRead(IR_PIN_2);
      
      // Diagnostický výpis
      Serial.printf("[POHYB IR] IR1 (27) = %d, IR2 (14) = %d\n", val1, val2);
      
      if (val1 < ir_threshold && val2 < ir_threshold) {
          Serial.printf("[POHYB IR] Medvěd spatřen oběma IR senzory! IR1=%d, IR2=%d. Zastavuji.\n", val1, val2);
          break;
      }
      
      if (millis() - start_slow_drive > 3000) {
          Serial.println("[POHYB IR] Vypršel bezpečnostní limit 3 sekundy. Zastavuji.");
          break;
      }
      
      delay(10);
  }
  
  // Okamžitě zastavíme motory
  man.motor(rb::MotorId::M2).speed(0);
  man.motor(rb::MotorId::M3).speed(0);
  
  bool grab_successful = false;
  for (int grab_attempt = 1; grab_attempt <= 3; grab_attempt++) {
      Serial.printf("[KLEPETA] Zavíráme klepeta - pokus %d (chytáme medvěda)...\n", grab_attempt);
      grabber.Grab();
      delay(1200); // aby se klepeta stihla zavřít
      
      // Ověření, zda oba IR senzory vidí medvěda
      int val1 = analogRead(IR_PIN_1);
      int val2 = analogRead(IR_PIN_2);
      if (val1 < ir_threshold && val2 < ir_threshold) {
          Serial.printf("[KLEPETA] Medvěd úspěšně chycen! Potvrzeno oběma IR senzory: IR1=%d, IR2=%d\n", val1, val2);
          grab_successful = true;
          break;
      } else {
          Serial.printf("[KLEPETA] Pokus %d o chycení selhal! Medvěd není v klepetech. IR1=%d, IR2=%d\n", grab_attempt, val1, val2);
          if (grab_attempt < 3) {
              Serial.println("[KLEPETA] Otevírám klepeta (HalfOpen) a popojíždím 10 cm (100 mm) dopředu pro nový pokus...");
              grabber.HalfOpen();
              delay(500);
              motors.forward_acc(100, 30);
          }
      }
  }
  
  if (!grab_successful) {
      logMsg("[KLEPETA] Varování: Medvěd nebyl spolehlivě detekován v klepetech ani po 3 pokusech. Pokračuji v návratu domů.");
  }
  
  // 1. Odcouvání 10 cm s akcelerací pro uvolnění
  logMsg("[POHYB] Couvám 100 mm zpět s akcelerací pro uvolnění...");
  motors.backward_acc(100.0f, 30.0f);
  
  // Uložení třetího úhlu po odcouvání
  gyro_angle_after_grab_back = getAbsoluteGyroAngleZ() * gyro_polarity;
  logMsg("[GYRO SAVED] Ukládám úhel po chycení a odcouvání: %.2f°", gyro_angle_after_grab_back);

  // 2. Pro návrat a dotočení použijeme úhel z enkodérů (předpokládáme nulovou změnu úhlu při couvání rovně)
  
  float current_heading_raw_gyro = getGyroAngleZ();
  float current_heading_gyro = current_heading_raw_gyro * gyro_polarity;

  logMsg("[VÝPOČET ENKODÉRŮ - NAVRÁCENÍ]");
  logMsg("  - Výchozí úhel nalezení medvěda z enkodérů (search_angle) = %.2f°", search_angle);
  logMsg("  - Načtená raw orientace z gyroskopu po odcouvání (gyro_raw) = %.2f°", current_heading_raw_gyro);
  logMsg("  - Korigovaný gyro úhel po odcouvání = %.2f°", current_heading_gyro);

  // 3. Provedení návratového otočení (výpočet a pohyb jsou definovány v perform_return_turn)
  perform_return_turn(gyro_angle_initial, gyro_angle_bear_found, gyro_angle_after_grab_back);
  
  float heading_after_rotation_raw = getGyroAngleZ();
  float heading_after_rotation = heading_after_rotation_raw * gyro_polarity;
  logMsg("  - Úhel z gyroskopu PO dotočení (raw): %.2f°", heading_after_rotation_raw);
  logMsg("  - Korigovaný úhel PO dotočení: %.2f°", heading_after_rotation);
  
  // 4. Couvání k pravé stěně pro srovnání
  logMsg("[POHYB] Couvám k pravé stěně bludiště pro srovnání...");
  motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
  
  // 5. Jedeme domů (standardní trasa - od pravé stěny)
  logMsg("[MAIN] Jedeme domů (GoHome)...");
  GoHome(false);
  
  logMsg("[MAIN] Robot dojel domů. Konec programu.");
  man.leds().yellow(false);
}

void loop(){
  // Pokud stiskneme tlačítko OFF, vypíšeme uložené logy
  if (man.buttons().off() == 1) {
      if (logBuffer.length() > 0) {
          Serial.println("\n--- VYPIS ULOZENYCH ZAZNAMU (LOG BUFFER) ---");
          Serial.print(logBuffer);
          Serial.println("--- KONEC VYPISU ---\n");
      } else {
          Serial.println("\n--- LOG BUFFER JE PRAZDNY ---\n");
      }
      delay(500); // ochrana proti zakmitům
  }

  // Debug zadních tlačítek po dojezdu domů / v nečinnosti
  if (man.buttons().left() == 1 || man.buttons().right() == 1) {
    Serial.printf("[TLACITKA] Zadní tlačítko stisknuto v hlavní smyčce: Levý = %d, Pravý = %d\n", 
                  man.buttons().left(), man.buttons().right());
    delay(200);
  }
  
  static uint32_t last_loop_ir_print = 0;
  if (millis() - last_loop_ir_print > 500) {
    int val1 = analogRead(IR_PIN_1);
    int val2 = analogRead(IR_PIN_2);
    float angleZ = getGyroAngleZ();
    uint32_t us1 = man.ultrasound(0).measure();
    uint32_t us2 = man.ultrasound(1).measure();
    Serial.printf("[LOOP] GPIO 27 = %d, GPIO 14 = %d | Gyro Z = %.2f° | US1 = %u mm, US2 = %u mm\n", val1, val2, angleZ, us1, us2);
    last_loop_ir_print = millis();
  }
  delay(10);
}