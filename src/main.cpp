#include "SmartServoBus.hpp"
#include "RBCX.h"
#include<Arduino.h>
#include<thread>
auto &man = rb::Manager::get(); //pro fungovani RBCX
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
  move.Arcleft(163, 150);
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
void GoHome(){
  Serial.println("[POHYB] GoHome: Otáčím se DOPRAVA o 90 stupňů...");
  move.TurnRight(90);
  Serial.println("[POHYB] GoHome: Couvám ke spodní stěně bludiště pomocí back_buttons...");
  motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
  Serial.println("[POHYB] GoHome: Dělám levý oblouk (poloměr 100 mm, 90 stupňů) pro odjezd od zdi...");
  move.Arcleft(90, 100);
  motors.forward_acc(80.0f, 40.0f);
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
  
  // Výpočet tiků pro přesně 90 stupňů: (PI * wheel_base) / (2.0 * mm_to_ticks)
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  
  for (int attempt = 1; attempt <= 3; attempt++) {
      if (attempt == 1) {
          Serial.println("[MAIN] Hledání medvěda - 1. pokus...");
      } else {
          // Před dalším pokusem popojedeme o 25 cm dopředu rychlostí 35 %
          Serial.printf("[MAIN] Pokus %d selhal. Popojíždím o 250 mm dopředu rychlostí 35 %%\n", attempt - 1);
          motors.forward_acc(250, 35);
      }

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
          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(search_speed_forward));
      } else {
          man.motor(rb::MotorId::M3).speed(0);
      }
      
      int current_ticks = 0;
      uint32_t last_send_time = 0;
      uint32_t last_info_time = 0;
      centered_consecutive_count = 0;
      
      while(true) {
          // 1. Zkontrolujeme aktuální pozici pravého kola (s ochranou proti přehlcení SPI)
          if (millis() - last_info_time > 20) {
              man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                  current_ticks = info.position();
              });
              last_info_time = millis();
          }
          
          // Kontrola limitů otáčení v závislosti na směru hledání
          if (direction == 1 && current_ticks > max_ticks_90) {
              Serial.printf("[MAIN] Dosažen limit 90° (%d tiků). Medvěd nenalezen. Otáčím se ZPĚT pomaleji (rychlost %.1f %%)...\n", current_ticks, search_speed_back);
              direction = -1;
              man.motor(rb::MotorId::M2).speed(0);
              man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-search_speed_back));
          } 
          else if (direction == -1 && current_ticks < 5) {
              Serial.printf("[MAIN] Vráceno na výchozí pozici 0°. Pokus %d selhal.\n", attempt);
              man.motor(rb::MotorId::M2).speed(0);
              man.motor(rb::MotorId::M3).speed(0);
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
                  
                  float current_search_speed = (direction == 1) ? search_speed_forward : search_speed_back;

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
      Serial.println("[TLACITKA] Tlačítko UP stisknuto! Startujeme celou jízdu...");
      run_entire_ride = true;
      break;
    }
    if (man.buttons().on() == 1) {
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
  float search_angle = man.mpu().getAngleZ();
  Serial.printf("[GYRO] Srovnaný úhel k medvědovi podle gyroskopu: Z = %.2f° (Ticks calculated: %.2f°)\n", 
                search_angle, search_angle_ticks);

  // Rozsvítit MODROU (a pro jistotu i zelenou) LED pro indikaci nalezení cíle
  man.leds().blue(true);
  man.leds().green(true);

  // Podrobné výpisy souřadnic po srovnání
  Serial.printf("[MAIN] Konečné souřadnice k medvědovi po srovnání: x=%d, y=%d, úhel=%.1f°, tiky=%d...\n", 
                message.x_distance, message.y_distance, search_angle, search_ticks);
  
  delay(200); // Krátká pauza na vizualizaci LED
  
  // Zhasnout diody před jízdou
  man.leds().blue(false);
  man.leds().green(false);

  // Otevření klepet v závislosti na úhlu od stěn (rezerva 20 stupňů od 0° a 90°)
  if (search_angle >= 20.0f && search_angle <= 80.0f) {
      Serial.println("[KLEPETA] Medvěd je v bezpečném úhlu. Otevírám klepeta naplno (Open)...");
      grabber.Open();
  } else {
      Serial.println("[KLEPETA] Medvěd je blízko stěny. Otevírám klepeta pouze částečně (HalfOpen) pro zamezení nárazu do zdi...");
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
      Serial.println("[KLEPETA] Varování: Medvěd nebyl spolehlivě detekován v klepetech ani po 3 pokusech. Pokračuji v návratu domů.");
  }
  
  // 1. Odcouvání 10 cm s akcelerací
  Serial.println("[POHYB] Couvám 100 mm zpět s akcelerací...");
  motors.backward_acc(100.0f, 30.0f);
  
  // 2. Natočení se na 90 stupňů k pravé stěně na základě Gyro Z heading
  float current_heading = man.mpu().getAngleZ();
  float remaining_angle = 90.0f - current_heading;
  
  Serial.printf("[POHYB GYRO] Aktuální azimut z gyroskopu: %.2f°, potřebujeme se natočit na 90°.\n", current_heading);
  Serial.printf("[POHYB GYRO] Výpočet zbývajícího úhlu pro otočení k pravé stěně: %.2f°\n", remaining_angle);
  
  if (remaining_angle > 0) {
      Serial.printf("[POHYB] Dotáčím se o zbývající úhel %.1f° doleva na plných 90° na místě...\n", remaining_angle);
      motors.turn_on_spot_left(remaining_angle, 40.0f);
  } else if (remaining_angle < 0) {
      Serial.printf("[POHYB] Přejeli jsme 90°. Dotáčím se o %.1f° doprava na místě...\n", std::abs(remaining_angle));
      motors.turn_on_spot_right(std::abs(remaining_angle), 40.0f);
  }
  
  // 3. Couvání k pravé stěně bludiště pro srovnání pomocí back_buttons (rychlost 75.0f):
  Serial.println("[POHYB] Couvám k pravé stěně bludiště pro srovnání pomocí back_buttons...");
  motors.back_buttons(75.0f, [](){ return man.buttons().left() == 1; }, [](){ return man.buttons().right() == 1; });
  
  // 4. Nyní jsme vyrovnaní u pravé stěny bludiště a jedeme domů
  Serial.println("[MAIN] Jedeme domů (GoHome)...");
  GoHome();
  Serial.println("[MAIN] Robot dojel domů. Konec programu.");
}

void loop(){
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