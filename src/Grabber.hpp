#include "SmartServoBus.hpp"
#include "RBCX.h"
#include<Arduino.h>

#include <memory>

//pro spravne fungovani SmartServoBus.hpp
using namespace lx16a;
extern std::unique_ptr<SmartServoBus> servoBus;



//struktura grabber obsahuje vsechny funkce pro ovladani grabberu
struct Grabber
{
    //funkce pro vypocet o jaky uhel se ma otocit prave servo
    Angle RightAngle(Angle angle)
    {
    angle = 240_deg - angle;
    return angle;
    }

    //mozne pozice grabberu
    enum grabber_state
        {
            closed,
            open,
            grab,
            half_open
        };

    grabber_state last_state = closed;//vychozi stav je zavreno
    //nastavy grabber na open pozici
    void Open(){
        if (!servoBus) return;
        if(last_state == closed){
            servoBus->set(0, 0_deg);
            delay(1000);//nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            servoBus->set(1, RightAngle(0_deg));
        }
        else{//grab
            servoBus->set(0, 0_deg); 
            servoBus->set(1, RightAngle(0_deg)); 
        }
        last_state = open;
    }
    //nastavy grabber na close pozici
    void Close(){
        if (!servoBus) return;
        if(last_state == open){
            servoBus->set(1, RightAngle(140_deg)); 
            delay(1000);//nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            servoBus->set(0, 145_deg); 
        }
        else{//grab
            servoBus->set(0, 50_deg); 
            delay(1000);
            servoBus->set(1, RightAngle(140_deg));
            delay(1000);//nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            servoBus->set(0, 145_deg); 
        }
        last_state = closed;
    }
    //nastavy grabber na grab pozici
    void Grab(){
        if (!servoBus) return;
        if(last_state == open){
            servoBus->set(0, 86_deg);
            servoBus->set(1, RightAngle(81_deg)); 
        }
        else{//close
            servoBus->set(0, 86_deg); 
            delay(1000);//nutna prodleva, aby nedoslo ke kolizi klepet pri otvirani
            servoBus->set(1, RightAngle(81_deg)); 
        }
        last_state = grab;
    }
    
    //nastavi grabber na polovicni open pozici (o 45 stupnu mene nez full open)
    void HalfOpen(){
        if (!servoBus) return;
        
        servoBus->set(0, 45_deg);
        if(last_state == closed){
            delay(1000); // ochrana pri otevirani z uplneho zavreni
        }
        servoBus->set(1, RightAngle(45_deg)); 
        
        last_state = half_open;
    }
};


