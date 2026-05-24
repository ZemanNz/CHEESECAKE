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
//struktury na ovladani robota
Grabber grabber;
Communication message;
Movemennt move;
Motors motors;

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
  move.Arcleft(168, 150);
  move.Straight(20000, 410,4000);
  move.Acceleration(20000, 100, 320);
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
  Serial.println("[POHYB] GoHome: Couvám ke spodní stěně bludiště...");
  move.BackwardUntillWall();
  Serial.println("[POHYB] GoHome: Jedu rovně 150 mm...");
  move.Straight(2000, 150, 99999);
  Serial.println("[POHYB] GoHome: Dělám pravý oblouk (90 stupňů, poloměr 300 mm)...");
  move.ArcRight(90, 300);
  Serial.println("[POHYB] GoHome: Jedu rovně 100 mm...");
  move.Straight(2000, 100, 99999);
  Serial.println("[POHYB] GoHome: Dělám levý oblouk (190 stupňů, poloměr 180 mm)...");
  move.Arcleft(190, 180);
  Serial.println("[POHYB] GoHome: Jedu rovně 200 mm...");
  move.Straight(2000, 200, 99999);
}

//ceka na zmacknuti on tlacitka pak program pokracuje
void WaitEorStart(){
  while (true){
    if (man.buttons().on() == 1){
      break;
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
  float search_speed = 6.0f; // Počáteční rychlost hledání (v procentech)
  float min_speed = 4.0f;    // Rychlost jemných kroků (v procentech) - zvýšeno na 4.0 %
  int centered_consecutive_count = 0;
  const int target_consecutive_confirmations = 3;
  const int centered_threshold = 15; // Tolerance pro vycentrování (v mm)
  const int step_zone_threshold = 40; // Odchylka v mm, pod kterou přejdeme do krokového režimu
  
  // Výpočet tiků pro přesně 90 stupňů: (PI * wheel_base) / (2.0 * mm_to_ticks)
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  
  Serial.printf("[MAIN] Zacinam plynule vyhledavani medveda (max 90 stupnu = %d tiků)...\n", max_ticks_90);

  // Vyčistíme stará data z bufferu přijímače, abychom nezastavili na starých datech
  message.clearRxBuffer();

  // Vynulujeme enkodér pravého kola před začátkem hledání
  man.motor(rb::MotorId::M3).setCurrentPosition(0);

  // Spustíme počáteční otáčení doleva (pouze pravým kolem, levé stojí na místě)
  man.motor(rb::MotorId::M2).speed(0);
  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(search_speed));
  
  int current_ticks = 0;
  uint32_t last_send_time = 0;
  uint32_t last_info_time = 0;
  
  while(true) {
      // 1. Zkontrolujeme aktuální pozici pravého kola (s ochranou proti přehlcení SPI)
      if (millis() - last_info_time > 20) {
          man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
              current_ticks = info.position();
          });
          last_info_time = millis();
      }
      
      // Pokud ujedeme více než 90 stupňů a medvěd nikde, vrátíme se zpět a ohlásíme chybu
      if (current_ticks > max_ticks_90) {
          Serial.printf("[MAIN] Překročen limit 90 stupňů (%d tiků). Medvěd nenalezen. Otáčím se zpět...\n", current_ticks);
          man.motor(rb::MotorId::M2).speed(0);
          man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(-search_speed));
          
          uint32_t last_reverse_info = 0;
          while (current_ticks > 5) {
              if (millis() - last_reverse_info > 20) {
                  man.motor(rb::MotorId::M3).requestInfo([&current_ticks](rb::Motor &info) {
                      current_ticks = info.position();
                  });
                  last_reverse_info = millis();
              }
              delay(10);
          }
          man.motor(rb::MotorId::M3).speed(0);
          return -1;
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

              // Diagnostický výpis přijatých dat z Raspberry Pi
              Serial.printf("[MAIN RX] Zprava z RPi: x=%d mm, y=%d mm, dist=%d mm, cam=%d, on=%d, shoda=%d/%d, tiky=%d/%d\n", 
                            x, y, message.msg.distance, message.msg.camera, message.msg.on, 
                            centered_consecutive_count + (abs(x) <= centered_threshold ? 1 : 0), 
                            target_consecutive_confirmations, current_ticks, max_ticks_90);
              
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
                  float current_speed = (abs(x) / 300.0f) * search_speed;
                  if (current_speed < min_speed) current_speed = min_speed;
                  if (current_speed > search_speed) current_speed = search_speed;
                  
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

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("[MAIN] Robot se spouští. Inicializace RBCX...");

  //pro fungovani RBCX (vypneme failsafe, protože vyhledávání blokuje na UART zprávách)
  man.install(rb::MAN_DISABLE_MOTOR_FAILSAFE);

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
  Serial.println("[MAIN] Čekám na stisknutí tlačítka ON...");
  WaitEorStart();
  // Odblokování serva s kamerou, aby s ním šlo hýbat (vypneme PWM buzení)
  man.stupidServo(0).disable();
  Serial.println("[MAIN] Servo s kamerou odblokováno. Čekám na další stisknutí tlačítka ON pro start...");
  WaitEorStart();
  delay(100);
  Serial.println("[MAIN] Původní jízda do hřiště (GoToField) je zakomentovaná pro účely testování...");
///////////////////
  /*
  std::thread t1(OpenGrabberBeforeField);//thread pro otevirani grabberu za jizdy
  GoToField(); //cesta do hriste
  t1.join();
  Serial.println("[MAIN] Robot dojel do hřiště. Zastaveno.");
  */
  
  // Rovnou nastavíme klepeta do přípravné polohy (jako by normálně dojel do hřiště)
  Serial.println("[KLEPETA] Nastavuji klepeta do přípravné polohy (HalfOpen) před zahájením detekce...");
  grabber.HalfOpen();

  Serial.println("[MAIN] Čekám 1 vteřinu na ustálení robota před hledáním medvěda...");
  delay(1000);

  // Spuštění plynulého vyhledávání medvěda
  int search_ticks = FindBearLeft();
  
  if (search_ticks < 0) {
      Serial.println("[MAIN] Detekce selhala nebo prekrocila 90 stupnu. Ukoncuji jizdu.");
      while(true) {
          delay(100);
      }
  }

  // Rozsvítit MODROU (a pro jistotu i zelenou) LED pro indikaci nalezení cíle
  man.leds().blue(true);
  man.leds().green(true);

  // Podrobné výpisy souřadnic po srovnání
  Serial.printf("[MAIN] Konečné souřadnice k medvědovi po srovnání: x=%d, y=%d, tiky=%d...\n", message.x_distance, message.y_distance, search_ticks);
  
  Serial.println("[MAIN] ČEKÁM NA STISKNUTÍ TLAČÍTKA UP (horní) pro zahájení lovu medvěda...");
  while(true){
    if (man.buttons().up() == 1) {
      Serial.println("[TLACITKA] Tlacitko UP stisknuto! Vyrazime.");
      break;
    }
    delay(10);
  }
  
  // Zhasnout diody před jízdou
  man.leds().blue(false);
  man.leds().green(false);

  Serial.println("[KLEPETA] Otevírám klepeta naplno (Open) a vyrážím za medvědem!");
  grabber.Open();
  
  // Jízda pro medvěda:
  int y_target = message.y_distance * 10; // převod na mm
  Serial.printf("[POHYB] Jedeme rovně %d mm k medvědovi.\n", y_target);
  move.Straight(2000, y_target, 4000);
  move.Stop();
  
  Serial.println("[KLEPETA] Zavíráme klepeta (chytáme medvěda)...");
  grabber.Grab();
  delay(1200); // aby se klepeta stihla zavřít
  
  // Návrat na otočný bod:
  Serial.printf("[POHYB] Couvám %d mm zpět na otočný bod.\n", y_target);
  move.Straight(-2000, y_target, 4000);
  move.Stop();
  
  // Otočení do plných 90 stupňů: (PI * wheel_base) / (2.0 * mm_to_ticks)
  const int max_ticks_90 = (M_PI * move.wheel_base) / (2.0 * move.mm_to_ticks);
  int remaining_ticks = max_ticks_90 - search_ticks;
  Serial.printf("[POHYB] Otáčím se o zbývající úhel do 90 stupňů (zbývá %d tiků)...\n", remaining_ticks);
  
  man.motor(rb::MotorId::M2).speed(0);
  man.motor(rb::MotorId::M3).setCurrentPosition(0);
  man.motor(rb::MotorId::M3).speed(motors.pctToSpeed(6.0f)); // 6.0 % rychlost hledání
  
  int temp_ticks = 0;
  uint32_t last_temp_info = 0;
  while (temp_ticks < remaining_ticks) {
      if (millis() - last_temp_info > 20) {
          man.motor(rb::MotorId::M3).requestInfo([&temp_ticks](rb::Motor &info) {
              temp_ticks = info.position();
          });
          last_temp_info = millis();
      }
      delay(10);
  }
  man.motor(rb::MotorId::M3).speed(0);
  
  // Nyní jsme natočeni kolmo (90 stupňů) ke stěně.
  // Zacouváme k pravé stěně, čímž se o ni dokonale srovnáme:
  Serial.println("[POHYB] Couvám k pravé stěně bludiště pro srovnání...");
  move.BackwardUntillWall();
  
  // Nyní jsme vyrovnaní u pravé stěny bludiště a jedeme domů
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
  delay(10);
}