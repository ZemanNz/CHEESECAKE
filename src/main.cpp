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
  move.Straight(32000,100,5000);
  open_grabber = true; //tady se zacne otevirat grabber
  move.Arcleft(161, 150);
  move.Straight(20000, 490,4000);
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
      Serial.printf("[IR SENSORS] GPIO 27 = %d, GPIO 14 = %d\n", val1, val2);
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

// Funkce pro plynulé vyhledávání medvěda (s potlačením šumu, limitou 90° a vrácením ujetých tiků)
int FindBearLeft() {
  float search_speed_forward = 6.0f; // Rychlost hledání tam (0° -> 90°)
  float search_speed_back = 3.0f;    // Rychlost hledání zpět (90° -> 0°)
  float min_speed = 3.0f;            // Rychlost jemných kroků (vycentrování)
  int centered_consecutive_count = 0;
  const int target_consecutive_confirmations = 3;
  const int centered_threshold = 15; // Tolerance pro vycentrování (v mm)
  const int step_zone_threshold = 40; // Odchylka v mm, pod kterou přejdeme do krokového režimu
  bool has_driven_halfway = false;   // Příznak, zda jsme již popojeli o polovinu cesty k medvědovi
  
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  const int ticks_10 = max_ticks_90 * 10 / 90;
  
  for (int attempt = 1; attempt <= 3; attempt++) {
      if (attempt == 1) {
          Serial.println("[MAIN] Hledání medvěda - 1. pokus...");
      } else {
          // Před dalším pokusem popojedeme o 32 cm dopředu rychlostí 35 %
          Serial.printf("[MAIN] Pokus %d selhal. Popojíždím o 320 mm dopředu rychlostí 35 %%\n", attempt - 1);
          motors.forward_acc(320, 35);
      }

      // Rychlost hledání pro tento pokus (u 2. a 3. pokusu se otáčíme pomalu v obou směrech)
      float current_forward_speed = (attempt == 1) ? search_speed_forward : search_speed_back;
      float current_back_speed = search_speed_back;

      // Vynulujeme enkodér pravého kola před začátkem pokusu
      man.motor(rb::MotorId::M3).setCurrentPosition(0);

      // Vyčistíme stará data z bufferu přijímače
      message.clearRxBuffer();

      // 0. Nejprve zkontrolujeme, zda medvěda nevidíme přímo před sebou bez otáčení
      Serial.println("[MAIN] Kontroluji, zda medvěd není přímo před námi...");
      bool found_initially = false;
      uint32_t start_check = millis();
      while (millis() - start_check < 800) {
          message.SendInPosstionMessage();
          delay(100);
          if (message.receiveMessage()) {
              if (message.msg.x != 0 || message.msg.y != 0 || message.msg.distance != 0) {
                  Serial.printf("[MAIN] Medvěd nalezen hned na startu! Souřadnice: x=%d, y=%d. Přeskakuji vyhledávací rotaci.\n", message.x_distance, message.y_distance);
                  found_initially = true;
                  break;
              }
          }
      }

      // Spustíme počáteční otáčení doleva (pouze pravým kolem, levé stojí na místě), pokud jsme ho nenašli hned
      int direction = 1; // 1 = doleva (vpřed), -1 = doprava (zpět k nule)
      man.motor(rb::MotorId::M2).speed(0);
      if (!found_initially) {
          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_forward_speed));
      } else {
          man.motor(rb::MotorId::M3).speed(0);
      }
      
      int current_ticks = 0;
      uint32_t last_send_time = 0;
      uint32_t last_info_time = 0;
      uint32_t last_bear_seen_time = millis();
      centered_consecutive_count = 0;
      
      while(true) {
          // 1. Zkontrolujeme aktuální pozici pravého kola (s ochranou proti přehlcení SPI)
          if (millis() - last_info_time > 20) {
              man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                  current_ticks = info.position();
              });
              last_info_time = millis();
          }
          
          // Absolutní bezpečnostní limity otáčení (cca -10° až +100°)
          const int max_ticks_100 = max_ticks_90 * 100 / 90;
          if (current_ticks > max_ticks_100 || current_ticks < -ticks_10) {
              Serial.printf("[MAIN] Absolutní bezpečnostní limit překročen (ticks=%d, rozsah: [%d, %d]). Návrat k nule...\n", 
                            current_ticks, -ticks_10, max_ticks_100);
              man.motor(rb::MotorId::M2).speed(0);
              
              if (current_ticks < 0) {
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_back_speed));
              } else {
                  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_back_speed));
              }
              
              uint32_t last_req = 0;
              while (true) {
                  if (millis() - last_req > 20) {
                      man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                          current_ticks = info.position();
                      });
                      last_req = millis();
                  }
                  if ((current_ticks >= 0 && current_ticks < 50) || (current_ticks <= 0 && current_ticks > -50)) {
                      break;
                  }
                  delay(5);
              }
              man.motor(rb::MotorId::M3).speed(0);
              Serial.println("[MAIN] Bezpečně vráceno a srovnáno na 0°.");
              break; // přeruší vnitřní smyčku, začne nový pokus
          }
          
          // Kontrola limitů otáčení v závislosti na směru hledání
          if (direction == 1 && current_ticks > max_ticks_90) {
              Serial.printf("[MAIN] Dosažen limit 90° (%d tiků). Medvěd nenalezen. Otáčím se ZPĚT pomaleji (rychlost %.1f %%)...\n", current_ticks, current_back_speed);
              direction = -1;
              man.motor(rb::MotorId::M2).speed(0);
              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_back_speed));
          } 
          else if (direction == -1 && current_ticks < -ticks_10) {
              Serial.printf("[MAIN] Dosažen limit -10° (%d tiků) při přejíždění nuly. Pokus %d selhal. Vracím se na 0°...\n", current_ticks, attempt);
              man.motor(rb::MotorId::M2).speed(0);
              
              // Pomalý návrat dopředu zpět na 0°
              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_back_speed));
              uint32_t last_req = 0;
              while (true) {
                  if (millis() - last_req > 20) {
                      man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                          current_ticks = info.position();
                      });
                      last_req = millis();
                  }
                  if (current_ticks >= 0) {
                      break;
                  }
                  delay(5);
              }
              man.motor(rb::MotorId::M3).speed(0);
              Serial.println("[MAIN] Vráceno a srovnáno na 0°.");
              break; // přeruší vnitřní smyčku, pokračuje dalším pokusem (attempt)
          }

          // 2. Pravidelně posíláme dotaz "inposition" do RPi
          if (millis() - last_send_time > 100) {
              message.SendInPosstionMessage();
              last_send_time = millis();
          }

          // 3. Zkontrolujeme, zda přišla nová nenulová data z RPi (neblokujícím způsobem)
          if (message.receiveMessage()) {
              if (message.msg.x != 0 || message.msg.y != 0 || message.msg.distance != 0) {
                  int x = message.x_distance;
                  int y = message.y_distance;
                  
                  // Aktualizujeme čas posledního úspěšného spatření medvěda
                  last_bear_seen_time = millis();
                  
                  float current_search_speed = (direction == 1) ? current_forward_speed : current_back_speed;

                  // Diagnostický výpis přijatých dat z Raspberry Pi
                  Serial.printf("[MAIN RX] Zprava z RPi: x=%d mm, y=%d mm, dist=%d mm, cam=%d, on=%d, shoda=%d/%d, tiky=%d/%d, smer=%d\n", 
                                x, y, message.msg.distance, message.msg.camera, message.msg.on, 
                                centered_consecutive_count + (abs(x) <= centered_threshold ? 1 : 0), 
                                target_consecutive_confirmations, current_ticks, max_ticks_90, direction);
                  
                   // Zkontrolujeme, jestli je medved vycentrován (v rámci tolerance +-15 mm)
                   if (abs(x) <= centered_threshold) {
                       centered_consecutive_count++;
                       
                       // Zastavíme motory pro přesné změření v klidu
                       man.motor(rb::MotorId::M2).speed(0);
                       man.motor(rb::MotorId::M3).speed(0);
                       
                       // STRATEGICKÝ KROK: Pokud je medvěd daleko (y > 500 mm) a je zhruba vycentrován,
                       // popojedeme o polovinu vzdálenosti blíže a začneme měřit znovu od nuly.
                       // Tím dostaneme robota do pozice, kde je medvěd blízko a stabilně sledovatelný přes HSV.
                       if (!has_driven_halfway && y > 400) {
                           int half_dist = y / 2;
                           Serial.printf("[MAIN] Detekován vzdálený medvěd (y=%d mm, x=%d mm). Popojíždím o %d mm blíže pro přesné zaměření.\n", y, x, half_dist);
                           delay(150); // Pauza na úplné zastavení
                           
                           // Popojedeme dopředu rychlostí 40 %
                           motors.forward_acc(half_dist, 40);
                           
                           has_driven_halfway = true;
                           
                           // Resetujeme gyroskop a enkodéry na novou nulu
                           man.mpu().resetAngleZ();
                           delay(100);
                           man.motor(rb::MotorId::M3).setCurrentPosition(0);
                           
                           // Vrátíme se na začátek vyhledávání (pokus = 1)
                           attempt = 0;
                           break; // vyskočíme z vnitřní smyčky a vyhledávání začne znovu z nové pozice
                       }
                       
                       if (centered_consecutive_count >= target_consecutive_confirmations) {
                           Serial.printf("[MAIN] Vyhledavani uspesne dokončeno! Medved potvrzen %d-krat za sebou. Koncova poloha: x=%d mm, y=%d mm, tiky=%d.\n", 
                                         target_consecutive_confirmations, x, y, current_ticks);
                           return current_ticks;
                       }
                       continue; // Přejdeme na další měření bez pohybu
                   } else {
                      // Pokud je chyba mimo toleranci, vynulujeme počítadlo potvrzení
                      centered_consecutive_count = 0;
                  }
                  
                  // Pokud jsme blízko cíle (chyba <= 40 mm), pohybujeme se v malých pulsech (pouze pravým kolem)
                  if (abs(x) <= step_zone_threshold) {
                      if (x > 0) {
                          Serial.printf("[MAIN] Krokovy rezim DOPRAVA (pouze prave kolo): x=%d mm, rychlost %.1f %%\n", x, min_speed);
                          man.motor(rb::MotorId::M2).speed(0); // Levé kolo stojí
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-min_speed)); // Pravé couvá
                      } else {
                          Serial.printf("[MAIN] Krokovy rezim DOLEVA (pouze prave kolo): x=%d mm, rychlost %.1f %%\n", x, min_speed);
                          man.motor(rb::MotorId::M2).speed(0); // Levé kolo stojí
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(min_speed)); // Pravé jede vpřed
                      }
                      
                      delay(40); // Krátký pulz 40 ms
                      
                      // Okamžitě zastavíme motory
                      man.motor(rb::MotorId::M2).speed(0);
                      man.motor(rb::MotorId::M3).speed(0);
                      
                      delay(50); // Krátká stabilizační pauza před dalším měřením
                  } 
                  // Pokud jsme daleko (chyba > 40 mm), otáčíme se plynule s dynamickou rychlostí (pouze pravým kolem)
                  else {
                      float current_speed = (abs(x) / 300.0f) * current_search_speed;
                      if (current_speed < min_speed) current_speed = min_speed;
                      if (current_speed > current_search_speed) current_speed = current_search_speed;
                      
                      if (x > 0) {
                          Serial.printf("[MAIN] Plynule DOPRAVA (pouze prave kolo): x=%d mm, rychlost %.1f %%\n", x, current_speed);
                          man.motor(rb::MotorId::M2).speed(0); // Levé kolo stojí
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-current_speed)); // Pravé couvá
                      } else {
                          Serial.printf("[MAIN] Plynule DOLEVA (pouze prave kolo): x=%d mm, rychlost %.1f %%\n", x, current_speed);
                          man.motor(rb::MotorId::M2).speed(0); // Levé kolo stojí
                          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(current_speed)); // Pravé jede vpřed
                      }
                  }
              }
          }

          // Pokud jsme ztratili medvěda z dohledu na více než 500 ms, obnovíme standardní zametání
          if (millis() - last_bear_seen_time > 500) {
              float speed = (direction == 1) ? current_forward_speed : -current_back_speed;
              man.motor(rb::MotorId::M2).speed(0);
              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(speed));
              centered_consecutive_count = 0;
          }

          delay(10);
      }
  }

  // Pokud všechny pokusy selhaly
  Serial.println("[MAIN] Všechny pokusy o vyhledání medvěda selhaly. Končím s chybou.");
  return -9999;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("[MAIN] Robot se spouští. Inicializace RBCX...");

  //pro fungovani RBCX (vypneme failsafe, protože vyhledávání blokuje na UART zprávách)
  man.install(rb::MAN_DISABLE_MOTOR_FAILSAFE);

  // Inicializace MPU (gyroskopu)
  Serial.println("[MAIN] Inicializace MPU (gyroskopu)...");
  man.mpu().init();
  man.mpu().sendStart();
  Serial.println("[MAIN] Čekám 1 sekundu na ustálení gyroskopických dat...");
  delay(1000);
  man.mpu().setCalibrationData(); // Zkalibruje offsety (robot musí být v klidu!)
  man.mpu().resetAngleZ();
  Serial.println("[MAIN] Gyroskop úspěšně zkalibrován.");

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
      float angleZ = man.mpu().getAngleZ();
      Serial.printf("[SETUP] GPIO 27 = %d, GPIO 14 = %d | Gyro Z = %.2f°\n", val1, val2, angleZ);
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

  Serial.println("[MAIN] Čekám 1 vteřinu na ustálení robota před hledáním medvěda...");
  // Resetujeme gyroskop na nulu před zahájením vyhledávání
  man.mpu().resetAngleZ();
  delay(100); // Krátká pauza pro ustálení resetu
  Serial.printf("[MAIN] Nulový úhel nastaven. Gyro Z před vyhledáváním = %.2f°\n", man.mpu().getAngleZ());

  // Spuštění plynulého vyhledávání medvěda
  int search_ticks = FindBearLeft();
  
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
  float search_angle_raw = man.mpu().getAngleZ();
  
  // Detekce polarity gyroskopu vůči enkodérům (pouze pokud se robot reálně pohnul o více než 5 stupňů)
  float gyro_polarity = 1.0f;
  if (std::abs(search_angle_ticks) > 5.0f && std::abs(search_angle_raw) > 5.0f) {
      if ((search_angle_ticks > 0 && search_angle_raw < 0) || (search_angle_ticks < 0 && search_angle_raw > 0)) {
          gyro_polarity = -1.0f;
          logMsg("[GYRO] ZJIŠTĚNA INVERZNÍ POLARITA GYROSKOPU. Nastavuji koeficient polarity na -1.0.");
      } else {
          gyro_polarity = 1.0f;
          logMsg("[GYRO] Polarita gyroskopu souhlasí s enkodéry (koeficient 1.0).");
      }
  }
  
  float search_angle = search_angle_raw * gyro_polarity;
  logMsg("[GYRO] Srovnaný úhel k medvědovi podle gyroskopu (korigovaný): Z = %.2f° (Raw: %.2f°, Ticks: %.2f°)", 
                search_angle, search_angle_raw, search_angle_ticks);

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

  // Otevření klepet v závislosti na úhlu od stěn (rezerva 20 stupňů od 0° a 90°)
  if (search_angle >= 20.0f && search_angle <= 80.0f) {
      logMsg("[KLEPETA] Medvěd je v bezpečném úhlu. Otevírám klepeta naplno (Open)...");
      grabber.Open();
  } else {
      logMsg("[KLEPETA] Medvěd je blízko stěny. Otevírám klepeta pouze částečně (HalfOpen) pro zamezení nárazu do zdi...");
      grabber.HalfOpen();
  }
  
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
              Serial.println("[KLEPETA] Otevírám klepeta a popojíždím 10 cm (100 mm) dopředu pro nový pokus...");
              grabber.Open();
              delay(500);
              motors.forward_acc(100, 30);
          }
      }
  }
  
  if (!grab_successful) {
      logMsg("[KLEPETA] Varování: Medvěd nebyl spolehlivě detekován v klepetech ani po 3 pokusech. Pokračuji v návratu domů.");
  }
  
  // 1. Odcouvání 10 cm s akcelerací pro uvolnění (společné pro obě trasy)
  logMsg("[POHYB] Couvám 100 mm zpět s akcelerací pro uvolnění...");
  motors.backward_acc(100.0f, 30.0f);

  // 2. Načteme aktuální orientaci robota PO odcouvání (pro eliminaci prokluzů a nárazů klepety)
  float current_heading_raw = man.mpu().getAngleZ();
  float current_heading = current_heading_raw * gyro_polarity;

  // Vyhodnocení rychlého návratu:
  // - Pokud se robot otočil doprava (do záporných úhlů k pravé stěně), povolujeme pouze velmi malou odchylku do -5° (téměř rovně).
  // - Pokud se robot otočil doleva (do kladných úhlů), povolujeme odchylku až do +15°.
  // Tím zabráníme kolizi s pravou stěnou při zpětné jízdě.
  bool direct_return = (search_angle >= -5.0f && search_angle <= 15.0f) && 
                       (current_heading >= -5.0f && current_heading <= 15.0f);
  
  if (direct_return) {
      logMsg("[MAIN] Rychlý návrat: Úhel výjezdu (%.2f°) i úhel po odcouvání (%.2f°) jsou v toleranci. Couvám ke spodní stěně...", search_angle, current_heading);
      // Rozsvítíme žlutou LED pro vizuální potvrzení rychlé trasy
      man.leds().yellow(true);
      
      // Pokračujeme v couvání ke spodní stěně bludiště na zadní mikrospínače
      motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
      
      logMsg("[MAIN] Jedeme domů (GoHome - rychlá trasa)...");
      GoHome(true); // Přeskočí otočení o 90° a vyrovnání u pravé stěny
  } else {
      logMsg("[MAIN] Standardní návrat: Úhel výjezdu (%.2f°) nebo úhel po odcouvání (%.2f°) je mimo toleranci. Vyrovnám se o pravou stěnu.", search_angle, current_heading);
      
      // Natočení se na 90 stupňů k pravé stěně na základě Gyro Z heading a enkodérů
      float remaining_angle_ticks = 90.0f - search_angle_ticks;
      float remaining_angle_gyro = 90.0f - current_heading;
      
      // Pomocný lambda výraz pro wrap úhlu do rozmezí [-180, 180] stupňů
      auto wrap_180 = [](float angle) {
          while (angle > 180.0f) angle -= 360.0f;
          while (angle < -180.0f) angle += 360.0f;
          return angle;
      };
      
      remaining_angle_ticks = wrap_180(remaining_angle_ticks);
      remaining_angle_gyro = wrap_180(remaining_angle_gyro);
      
      float remaining_angle = remaining_angle_gyro; // Výchozí
      float diff = std::abs(remaining_angle_gyro - remaining_angle_ticks);
      
      logMsg("[POHYB ANGLE] Výpočet otočení k pravé stěně (korigovaný polaritou & wrap):");
      logMsg("  - Podle enkodérů: %.2f°", remaining_angle_ticks);
      logMsg("  - Podle gyroskopu: %.2f° (korigovaný azimut: %.2f°, Raw: %.2f°)", remaining_angle_gyro, current_heading, current_heading_raw);
      logMsg("  - Rozdíl metod: %.2f°", diff);
      
      if (diff <= 15.0f) {
          remaining_angle = remaining_angle_gyro;
          logMsg("[POHYB ANGLE] Shoda v toleranci 15°. Používám gyroskop: %.2f°", remaining_angle);
      } else {
          // V případě větší odchylky použijeme průměr obou hodnot pro eliminaci hrubé chyby
          remaining_angle = (remaining_angle_gyro + remaining_angle_ticks) / 2.0f;
          logMsg("[POHYB ANGLE] VAROVÁNÍ: Hodnoty se rozcházejí! Používám průměr obou: %.2f°", remaining_angle);
      }
      
      // Znovu wrapneme výsledný úhel pro nalezení nejkratší trasy otočení
      remaining_angle = wrap_180(remaining_angle);
      
      // Uživatelské pravidlo: Po chycení medvěda se nikdy neotáčíme doprava.
      // Pokud je zbývající úhel záporný (přejeli jsme 90°), neotáčíme se a necháme se vyrovnat stěnou.
      if (remaining_angle < 0) {
          logMsg("[POHYB] Vypočtený zbývající úhel je %.2f° (záporný). Otáčení doprava je zakázáno, zůstávám stát.", remaining_angle);
          remaining_angle = 0.0f;
      }
      
      if (remaining_angle > 0) {
          logMsg("[POHYB] Dotáčím se o zbývající úhel %.1f° doleva na plných 90° na místě...", remaining_angle);
          motors.turn_on_spot_left(remaining_angle, 40.0f);
      }
      
      // 3. Couvání k pravé stěně bludiště pro srovnání pomocí back_buttons (rychlost 75.0f):
      logMsg("[POHYB] Couvám k pravé stěně bludiště pro srovnání pomocí back_buttons...");
      motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
      
      // 4. Nyní jsme vyrovnaní u pravé stěny bludiště a jedeme domů
      logMsg("[MAIN] Jedeme domů (GoHome - standardní trasa)...");
      GoHome(false);
  }
  logMsg("[MAIN] Robot dojel domů. Konec programu.");
  // Zhasneme žlutou LED po dojezdu domů
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
    float angleZ = man.mpu().getAngleZ();
    Serial.printf("[LOOP] GPIO 27 = %d, GPIO 14 = %d | Gyro Z = %.2f°\n", val1, val2, angleZ);
    last_loop_ir_print = millis();
  }
  delay(10);
}