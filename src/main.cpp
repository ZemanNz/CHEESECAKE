#include "SmartServoBus.hpp"
#include "RBCX.h"
#include<Arduino.h>
#include<thread>
auto &man = rb::Manager::get(); //pro fungovani RBCX
#include "Grabber.hpp"
std::unique_ptr<SmartServoBus> servoBus;
#include"Comunication.hpp"
#include "Movement.hpp"
//struktury na ovladani robota
Grabber grabber;
Communication message;
Movemennt move;

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
  //sector blue
  if (x < 400){
    sector = blue;
    if (y>1400){
      y=1300;
    }
    move.Straight(2000,y,4000);
    move.Stop();
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }

  //sector red
  if (x >= 400 && x <= 600){
    sector = red;
    if (y>1400){
      y=1300;
    }
    move.Straight(2000,100,4000);
    move.TurnRight(45);
    move.Straight(2000,400,4000);
    move.TurnLeft(45);
    move.Straight(2000,y-200,4000);
    move.Stop();
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
    move.TurnRight(90);
  }

  //sector yellow
  if (x > 600 && y>300){
    sector = yellow;
    if(y>1400){
      y=1300;
    }
    move.Straight(2000,y,4000);
    move.TurnRight(90);
    if(x>1400){
      x=1300;
    }
    move.Straight(2000,x,4000);
    move.Stop();
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }

  //sector green
  if (x > 600 && y<=300){
    sector = green;
    if(y<300){
      y=300;
      }
    move.Straight(2000,y,4000);
    move.TurnRight(90);
    if(x>1400){
      x=1300;
    }
    move.Straight(2000,x,4000);
    move.Stop();
    grabber.Grab();
    delay(1200);// aby se grabber stihnul zavrit
  }
}

//funkce pro jizdu zpet do domovske pozice
void GoHome(){
  if (sector == blue){
    move.BackwardUntillWall();
    //move.Straight(2000, 100, 99999);//
    //move.TurnRight(90);
    move.Arcleft(95, 70);
    move.BackwardUntillWall();
    move.Straight(2000, 150, 99999);
    move.ArcRight(90, 300);
    move.Straight(2000, 100, 99999);
    move.Arcleft(190, 180);
    move.Straight(2000, 200, 99999);
    //grabber.Open();
  }
else{
  move.BackwardUntillWall();
  move.Straight(2000, 30, 99999); 
  move.TurnLeft(90);
  //////////////////////////
  move.BackwardUntillWall();
    //move.Straight(2000, 100, 99999);//
    //move.TurnRight(90);
    move.Arcleft(95, 70);
    move.BackwardUntillWall();
    move.Straight(2000, 150, 99999);
    move.ArcRight(90, 300);
    move.Straight(2000, 100, 99999);
    move.Arcleft(190, 180);
    move.Straight(2000, 200, 99999);
    //grabber.Open();
}
}

//ceka na zmacknuti on tlacitka pak program pokracuje
void WaitEorStart(){
  while (true){
    if (man.buttons().on() == 1){
      break;
    }
    if (man.buttons().up() == 1) {
      Serial.printf("[TLACITKA] Stavy zadnich mikrospinacu: Levy = %d, Pravy = %d\n", 
                    man.buttons().left(), man.buttons().right());
      delay(200); // Ochrana proti zaspamování konzole
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
  grabber.Open();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("[MAIN] Robot se spouští. Inicializace RBCX...");

  //pro fungovani RBCX
  auto &man = rb::Manager::get();
  man.install();

  Serial.println("[SERVOS] Inicializace SmartServoBus (UART1, GPIO 27)...");
  //parametry pro komunikaci se servy 
  servoBus = std::make_unique<SmartServoBus>();
  servoBus->begin(2, UART_NUM_1, GPIO_NUM_27); 

  message.setup();

  servoBus->setAutoStop(0, false);//vypne autostop leveho serva 
  servoBus->setAutoStop(1, false);//vypne autostop praveho serva
  Serial.println("[SERVOS] Inicializace serv dokončena.");
  
//po zapnuti ceka na zpravu od Raspberry Pi ze je ready
//message.WaitForReadyMessage();
  Serial.println("[MAIN] Čekám na stisknutí tlačítka ON...");
  WaitEorStart();
  man.stupidServo(0).setPosition(0);
  Serial.println("[MAIN] Nastaveno hloupé servo. Čekám na další stisknutí tlačítka ON pro start...");
  WaitEorStart();
  delay(100);
  Serial.println("[MAIN] Startuji jízdu do hřiště (GoToField)...");
///////////////////
  std::thread t1(OpenGrabberBeforeField);//thread pro otevirani grabberu za jizdy
  GoToField(); //cesta do hriste
  t1.join();
  Serial.println("[MAIN] Robot dojel do hřiště. Zastaveno.");

  // Uvolníme UART1 před zapnutím komunikace s RPi
  Serial.println("[SERVOS] Uvolňuji serva a port UART1...");
  servoBus.reset();
  delay(50);
  Serial.println("[SERVOS] Serva uvolněna.");

  // Zapneme komunikaci s RPi na UART1
  Serial.println("[MAIN] Spouštím komunikaci s Raspberry Pi...");
  message.start();
  message.SendInPosstionMessage();
  message.WaitingForBearPosData();
  message.end();
  Serial.println("[MAIN] Komunikace s Raspberry Pi dokončena a uvolněna.");
  delay(50);

  // Znovu zapneme serva na UART1
  Serial.println("[SERVOS] Znovu inicializuji serva pro chycení medvěda...");
  servoBus = std::make_unique<SmartServoBus>();
  servoBus->begin(2, UART_NUM_1, GPIO_NUM_27); 
  servoBus->setAutoStop(0, false);
  servoBus->setAutoStop(1, false);
  Serial.println("[SERVOS] Serva jsou opět připravena.");

  Serial.printf("[MAIN] Jedeme pro medvěda na souřadnice: x=%d, y=%d...\n", message.x_distance, message.y_distance);
  GoForBear(message.x_distance,message.y_distance);
  Serial.println("[MAIN] Medvěd by měl být chycen. Jedeme domů (GoHome)...");
  GoHome();
  Serial.println("[MAIN] Robot dojel domů. Konec programu.");
}
void loop(){}