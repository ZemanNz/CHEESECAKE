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
  if (sector == blue){
    Serial.println("[POHYB] GoHome (MODRÝ SEKTOR): Couvám ke stěně...");
    move.BackwardUntillWall();
    Serial.println("[POHYB] GoHome: Dělám levý oblouk (95 stupňů, poloměr 70 mm)...");
    move.Arcleft(95, 70);
    Serial.println("[POHYB] GoHome: Couvám ke stěně...");
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
  else{
    Serial.println("[POHYB] GoHome (OSTATNÍ SEKTY): Couvám ke stěně...");
    move.BackwardUntillWall();
    Serial.println("[POHYB] GoHome: Jedu rovně 30 mm...");
    move.Straight(2000, 30, 99999); 
    Serial.println("[POHYB] GoHome: Otáčím se doleva o 90 stupňů...");
    move.TurnLeft(90);
    Serial.println("[POHYB] GoHome: Couvám ke stěně...");
    move.BackwardUntillWall();
    Serial.println("[POHYB] GoHome: Dělám levý oblouk (95 stupňů, poloměr 70 mm)...");
    move.Arcleft(95, 70);
    Serial.println("[POHYB] GoHome: Couvám ke stěně...");
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

// Funkce pro plynulé vyhledávání medvěda (otáčení doleva s dynamickým zpomalováním a návratem)
void FindBearLeft() {
  float search_speed = 6.0f; // Počáteční rychlost hledání (v procentech)
  float min_speed = 4.0f;    // Minimální rychlost, aby se robot vůbec hýbal
  
  Serial.println("[MAIN] Zacinam plynule vyhledavani medveda (otaceni doleva)...");

  // Pro otáčení DOLEVA musí levý motor couvat (kladná hodnota) a pravý jet dopředu (kladná hodnota)
  man.motor(motors.m_id_left).speed(motors.pctToSpeed(search_speed));
  man.motor(motors.m_id_right).speed(motors.pctToSpeed(search_speed));
  
  while(true) {
      message.SendInPosstionMessage();
      message.WaitingForBearPosData(); // Zde se sekne, dokud medved neni v zaberu
      
      // Hodnota x_distance odteď chodí z Raspberry Pi v MILIMETRECH
      int x = message.x_distance;
      
      // Zkontrolujeme, jestli je medved uprostred s velmi malou toleranci +- 6 mm
      if (x >= -6 && x <= 6) {
          Serial.println("[MAIN] Medved je presne pred nami (X je blizko 0)! Zastavuji otaceni.");
          man.motor(motors.m_id_left).speed(0);
          man.motor(motors.m_id_right).speed(0);
          break;
      }
      
      // Vypocet plynuleho zpomaleni (max X je zhruba 300 mm od stredu)
      float current_speed = (abs(x) / 300.0f) * search_speed;
      if (current_speed < min_speed) current_speed = min_speed;
      if (current_speed > search_speed) current_speed = search_speed;
      
      // Pokud je x > 6, prelozili jsme ho a medved je nyni napravo od robota. Musime se otocit zpet DOPRAVA!
      if (x > 6) {
          Serial.printf("[MAIN] Prejeli jsme! x=%d mm. Menim smer, tocim se DOPRAVA rychlosti %.1f %%\n", x, current_speed);
          // Pro otaceni DOPRAVA dame zapornou hodnotu do obou motoru
          man.motor(motors.m_id_left).speed(motors.pctToSpeed(-current_speed));
          man.motor(motors.m_id_right).speed(motors.pctToSpeed(-current_speed));
      } 
      // Jinak jsme medveda jeste nedotahli do stredu, nebo je nalevo. Tocime se stale DOLEVA.
      else {
          Serial.printf("[MAIN] Srovnavam: x=%d mm, tocim DOLEVA rychlosti %.1f %%\n", x, current_speed);
          // Pro otaceni DOLEVA dame kladnou hodnotu do obou motoru
          man.motor(motors.m_id_left).speed(motors.pctToSpeed(current_speed));
          man.motor(motors.m_id_right).speed(motors.pctToSpeed(current_speed));
      }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("[MAIN] Robot se spouští. Inicializace RBCX...");

  //pro fungovani RBCX
  man.install();

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
  man.stupidServo(0).setPosition(0);
  Serial.println("[MAIN] Nastaveno hloupé servo. Čekám na další stisknutí tlačítka ON pro start...");
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
  FindBearLeft();

  // Rozsvítit MODROU (a pro jistotu i zelenou) LED pro indikaci nalezení cíle
  // Poznamka: Pokud by modrá LED zlobila (jak jsi zmiňoval), uvidíš aspoň zelenou
  man.leds().blue(true);
  man.leds().green(true);

  // Podrobné výpisy souřadnic po srovnání
  Serial.printf("[MAIN] Konečné souřadnice k medvědovi po srovnání: x=%d, y=%d...\n", message.x_distance, message.y_distance);
  
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
  
  GoForBear(message.x_distance,message.y_distance);
  Serial.println("[MAIN] Medvěd by měl být chycen. Jedeme domů (GoHome)...");
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