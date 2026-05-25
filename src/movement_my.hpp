#pragma once

#include "RBCX.h"
#include <Arduino.h>
#include <functional>
#include <algorithm>
#include <cmath>

class Motors {
public:
    // Parametry motorů a podvozku
    rb::MotorId m_id_left;
    rb::MotorId m_id_right;
    bool m_polarity_switch_left;
    bool m_polarity_switch_right;
    
    float left_wheel_diameter;
    float right_wheel_diameter;
    float m_wheel_circumference;
    float m_wheel_circumference_left;
    float m_wheel_circumference_right;
    float prevod_motoru;
    float roztec_kol;
    
    float m_max_speed;
    
    float korekce_nedotacivosti_left;
    float korekce_nedotacivosti_right;
    float konstanta_radius_vnejsi_kolo;
    float konstanta_radius_vnitrni_kolo;

    // Konstruktor s výchozími hodnotami pro CHEESECAKE
    Motors(rb::MotorId id_left = rb::MotorId::M2,
           rb::MotorId id_right = rb::MotorId::M3,
           float left_wheel_dia = 61.943f,
           float right_wheel_dia = 62.057f,
           float wheel_base = 145.652f,
           float gear_ratio = 905.95f)
        : m_id_left(id_left)
        , m_id_right(id_right)
        , m_polarity_switch_left(true)
        , m_polarity_switch_right(false)
        , left_wheel_diameter(left_wheel_dia)
        , right_wheel_diameter(right_wheel_dia)
        , prevod_motoru(gear_ratio)
        , roztec_kol(wheel_base)
        , m_max_speed(5200.0f)
        , korekce_nedotacivosti_left(1.0f)
        , korekce_nedotacivosti_right(1.0f)
        , konstanta_radius_vnejsi_kolo(1.0f)
        , konstanta_radius_vnitrni_kolo(1.0f)
    {
        m_wheel_circumference_left = 3.14159265f * left_wheel_diameter;
        m_wheel_circumference_right = 3.14159265f * right_wheel_diameter;
        m_wheel_circumference = 3.14159265f * ((left_wheel_diameter + right_wheel_diameter) / 2.0f);

        // Konfigurace zrychlení a parametrů regulátoru motorů na koprocesoru (1:1 s knihovnou Robotka)
        auto& man = rb::Manager::get();
        if (man.coprocFwVersion().number >= 0x010100) {
            const MotorConfig motorConf = {
                .velEpsilon = 3,
                .posEpsilon = 8,
                .maxAccel = 50000,
            };
            man.motor(m_id_left).setConfig(motorConf);
            man.motor(m_id_right).setConfig(motorConf);
        }
    }

    // Pomocné konverzní funkce
    int16_t pctToSpeed(float pct) {
        pct = rb::clamp(pct, -100.0f, 100.0f);
        int32_t speed = static_cast<int32_t>((pct * m_max_speed) / 100.0f);
        return rb::clamp(speed, -INT16_MAX, INT16_MAX);
    }

    int32_t mmToTicks(float mm) const {
        return (mm / m_wheel_circumference) * prevod_motoru;
    }

    float ticksToMm(int32_t ticks) const {
        return float(ticks) / prevod_motoru * m_wheel_circumference;
    }

    int32_t mmToTicks_left(float mm) const {
        return (mm / m_wheel_circumference_left) * prevod_motoru;
    }

    int32_t mmToTicks_right(float mm) const {
        return (mm / m_wheel_circumference_right) * prevod_motoru;
    }

    int timeout_ms(float mm, float speed) {
        return static_cast<int>(295.0f * mm / std::abs(speed));
    }

    // Pohybové funkce podle zadání
    void forward(float mm, float speed);
    void backward(float mm, float speed);
    void turn_on_spot_right(float angle, float speed);
    void turn_on_spot_left(float angle, float speed);
    void radius_right(float radius, float angle, float speed);
    void radius_left(float radius, float angle, float speed);
    void forward_acc(float mm, float speed);
    void backward_acc(float mm, float speed);
    void back_buttons(float speed, std::function<bool()> first_button, std::function<bool()> second_button);
};

// Implementace metod třídy Motors

inline void Motors::forward(float mm, float speed) {
    auto& man = rb::Manager::get();
    
    float m_kp = 0.23f; // Proporcionální konstanta
    float m_min_speed = 20.0f; // Minimální rychlost motorů
    float m_max_correction = 8.5f;
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks_left = mmToTicks_left(mm);
    int target_ticks_right = mmToTicks_right(mm);
    float left_pos = 0;
    float right_pos = 0;
    float progres_left = 0.0f;
    float progres_right = 0.0f;
    float rozdil_progres = 0.0f;
    
    // Základní rychlosti s přihlédnutím k polaritě
    float base_speed_left = m_polarity_switch_left ? -speed : speed;
    float base_speed_right = m_polarity_switch_right ? -speed : speed;
    
    unsigned long start_time = millis();
    int timeoutMs = timeout_ms(mm, speed);
    
    while((target_ticks_left > std::abs(left_pos) || target_ticks_right > std::abs(right_pos)) && 
          (millis() - start_time < timeoutMs)) {
        
        // Čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);

        // P regulátor - pracuje s absolutními hodnotami pozic
        progres_left = (float(std::abs(left_pos)) / float(target_ticks_left));
        progres_right = (float(std::abs(right_pos)) / float(target_ticks_right));
        rozdil_progres = progres_left - progres_right;

        float correction = rozdil_progres * m_kp * 1800;
        correction = std::max(-m_max_correction, std::min(correction, m_max_correction));
        
        // Výpočet korigovaných rychlostí
        float speed_left = base_speed_left;
        float speed_right = base_speed_right;
        
        // Aplikace korekce podle polarity
        if (correction > 0) {
            // Levý je napřed - zpomalit levý
            if (m_polarity_switch_left) {
                speed_left += correction;  // Přidat k záporné rychlosti = zpomalit
                speed_right += correction;  // Odečíst od kladné rychlosti = zrychlit
            } else {
                speed_left -= correction;  // Odečíst od kladné rychlosti = zpomalit
                speed_right -= correction;  // Přidat ke kladné rychlosti = zrychlit
            }
        } else if (correction < 0) {
            // Pravý je napřed - zpomalit pravý
            if (m_polarity_switch_right) {
                speed_right -= correction;  // Odečíst od záporné rychlosti = zpomalit
                speed_left -= correction;  // Přidat k záporné rychlosti = zrychlit
            } else {
                speed_right += correction;  // Přidat ke kladné rychlosti = zpomalit
                speed_left += correction;  // Odečíst od kladné rychlosti = zrychlit
            }
        }
        
        // Zajištění minimální rychlosti
        if (std::abs(speed_left) < m_min_speed && std::abs(speed_left) > 0) {
            speed_left = (speed_left > 0) ? m_min_speed : -m_min_speed;
        }
        if (std::abs(speed_right) < m_min_speed && std::abs(speed_right) > 0) {
            speed_right = (speed_right > 0) ? m_min_speed : -m_min_speed;
        }
        
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(speed_left ));
        man.motor(m_id_right).speed(pctToSpeed(speed_right ));
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::backward(float mm, float speed) {
    auto& man = rb::Manager::get();
    
    float m_kp = 0.23f; // Proporcionální konstanta
    float m_min_speed = 20.0f; // Minimální rychlost motorů
    float m_max_correction = 8.5f;
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks_left = mmToTicks_left(mm);
    int target_ticks_right = mmToTicks_right(mm);
    float left_pos = 0;
    float right_pos = 0;
    float progres_left = 0.0f;
    float progres_right = 0.0f;
    float rozdil_progres = 0.0f;
    
    // Základní rychlosti s přihlédnutím k polaritě
    float base_speed_left = m_polarity_switch_left ? speed : -speed;
    float base_speed_right = m_polarity_switch_right ? speed : -speed;
    
    unsigned long start_time = millis();
    int timeoutMs = timeout_ms(mm, speed);
    
    while((target_ticks_left > std::abs(left_pos) || target_ticks_right > std::abs(right_pos)) && 
          (millis() - start_time < timeoutMs)) {
        
        // Čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);
        
        // P regulátor - pracuje s absolutními hodnotami pozic
        progres_left = (float(std::abs(left_pos)) / float(target_ticks_left));
        progres_right = (float(std::abs(right_pos)) / float(target_ticks_right));
        rozdil_progres = progres_left - progres_right;

        float correction = rozdil_progres * m_kp * 1800;
        correction = std::max(-m_max_correction, std::min(correction, m_max_correction));
        
        // Výpočet korigovaných rychlostí
        float speed_left = base_speed_left;
        float speed_right = base_speed_right;
        
        // Aplikace korekce podle polarity
        if (rozdil_progres > 0) {
            // Levý je napřed - zpomalit levý
            if (m_polarity_switch_left) {
                speed_left -= correction;  // Přidat k záporné rychlosti = zpomalit
                speed_right -= correction;  // Odečíst od kladné rychlosti = zrychlit
            } else {
                speed_left += correction;  // Odečíst od kladné rychlosti = zpomalit
                speed_right += correction;  // Přidat ke kladné rychlosti = zrychlit
            }
        } else if (rozdil_progres < 0) {
            // Pravý je napřed - zpomalit pravý
            if (m_polarity_switch_right) {
                speed_right += correction;  // Odečíst od záporné rychlosti = zpomalit
                speed_left += correction;  // Přidat k záporné rychlosti = zrychlit
            } else {
                speed_right -= correction;  // Přidat ke kladné rychlosti = zpomalit
                speed_left -= correction;  // Odečíst od kladné rychlosti = zrychlit
            }
        }
        
        // Zajištění minimální rychlosti
        if (std::abs(speed_left) < m_min_speed && std::abs(speed_left) > 0) {
            speed_left = (speed_left > 0) ? m_min_speed : -m_min_speed;
        }
        if (std::abs(speed_right) < m_min_speed && std::abs(speed_right) > 0) {
            speed_right = (speed_right > 0) ? m_min_speed : -m_min_speed;
        }
        
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(speed_left ));
        man.motor(m_id_right).speed(pctToSpeed(speed_right ));
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::turn_on_spot_right(float angle, float speed) {
    auto& man = rb::Manager::get();
    
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks = korekce_nedotacivosti_right * mmToTicks((M_PI * roztec_kol) * (angle / 360.0f));
    int left_pos = 0;
    int right_pos = 0;

    float step = 5.0f; // velikost kroku pro zvyšování rychlosti
    float min_speed = 15.0f; // minimální rychlost
    int o_kolik_drive_zpomalovat = 1400;
    byte a = 0;
    byte deaccelating = byte(240/std::abs(speed)); // počet cyklů mezi zpomalováním

    float max_correction = 7.0f; // Maximální korekce rychlosti

    float base_speed_left = m_polarity_switch_left ? -speed : speed;
    float base_speed_right = m_polarity_switch_right ? speed : -speed;

    float step_left = (base_speed_left > 0) ? step : -step;
    float step_right = (base_speed_right > 0) ? step : -step;

    float speed_left = base_speed_left;
    float speed_right = base_speed_right;

    float l_speed = 0.0f;
    float r_speed = 0.0f;

    unsigned long start_time = millis();
    const int timeoutMs = 10000; // 10 second timeout
    
    while((target_ticks > (std::abs(left_pos) + 5) || target_ticks > (std::abs(right_pos) + 5)) && 
          (millis() - start_time < timeoutMs)) {
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });
        delay(10);

        // Zrychlení
        if (((float(std::abs(left_pos)) + float(std::abs(right_pos)))/2) < (target_ticks - o_kolik_drive_zpomalovat)  && (std::abs(speed_left) < std::abs(speed) || std::abs(speed_right) < std::abs(speed))) {
            if((std::abs(base_speed_left) < std::abs(speed))) {
                speed_left += step_left;
            }
            if((std::abs(base_speed_right) < std::abs(speed))) {
                speed_right += step_right;
            }
        }
        // Zpomalování
        else if (((float(std::abs(left_pos)) + float(std::abs(right_pos))) / 2) > (target_ticks - o_kolik_drive_zpomalovat)) {
            if(a % deaccelating == 0) { // zpomaluj jen každých N cyklů pro plynulejší zpomalení
                if (std::abs(speed_left) > min_speed) {
                    speed_left -= step_left;
                }
                if (std::abs(speed_right) > min_speed) {
                    speed_right -= step_right;
                }
                a = 0; 
            }
            a++;
        }
        l_speed = speed_left;
        r_speed = speed_right;
        
        // Regulace
        float error = std::abs(left_pos) - std::abs(right_pos);
        float correction = error * 0.18f; // Proporcionální konstanta
        if(std::abs(correction) > max_correction) {
            correction = (correction > 0) ? max_correction : -max_correction;
        }

        if (error > 0) {
            // Levý je napřed - zpomalit levý
            if (base_speed_left > 0) {
                l_speed -= correction;  // Odečíst od kladné rychlosti = zpomalit
                r_speed += correction;  // Přidat ke kladné rychlosti = zrychlit
            } else {
                l_speed += correction;  // Přidat k záporné rychlosti = zpomalit
                r_speed -= correction;  // Odečíst od záporné rychlosti = zrychlit
            }
        }
        else {
            // Pravý je napřed - zpomalit pravý
            if (base_speed_right > 0) {
                r_speed += correction;  // Odečíst od záporné rychlosti = zpomalit
                l_speed -= correction;  // Přidat k záporné rychlosti = zrychlit
            } else {
                r_speed -= correction;  // Přidat ke kladné rychlosti = zpomalit
                l_speed += correction;  // Odečíst od kladné rychlosti = zrychlit
            }
        }

        // Omezení na minimum
        if (std::abs(l_speed) < min_speed) {
            l_speed = (l_speed > 0) ? min_speed : -min_speed;
        }
        if (std::abs(r_speed) < min_speed) {
            r_speed = (r_speed > 0) ? min_speed : -min_speed;
        }
        if(std::abs(l_speed) > (std::abs(base_speed_left) + 10)) {
            if(base_speed_left > 0) l_speed = base_speed_left + 10;
            else l_speed = base_speed_left - 10;
        }
        if(std::abs(r_speed) > (std::abs(base_speed_right) + 10)) {
            if(base_speed_right > 0) r_speed = base_speed_right + 10;
            else r_speed = base_speed_right - 10;
        }
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(l_speed));
        man.motor(m_id_right).speed(pctToSpeed(r_speed));
    }
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::turn_on_spot_left(float angle, float speed) {
    auto& man = rb::Manager::get();
    
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks = korekce_nedotacivosti_left * mmToTicks((M_PI * roztec_kol) * (angle / 360.0f));
    int left_pos = 0;
    int right_pos = 0;

    float step = 5.0f; // velikost kroku pro zvyšování rychlosti
    float min_speed = 15.0f; // minimální rychlost
    int o_kolik_drive_zpomalovat = 1400;
    byte a = 0;
    byte deaccelating = byte(240/std::abs(speed)); // počet cyklů mezi zpomalováním

    float max_correction = 7.0f; // Maximální korekce rychlosti

    float base_speed_left = m_polarity_switch_left ? speed : -speed;
    float base_speed_right = m_polarity_switch_right ? -speed : speed;

    float step_left = (base_speed_left > 0) ? step : -step;
    float step_right = (base_speed_right > 0) ? step : -step;

    float speed_left = base_speed_left;
    float speed_right = base_speed_right;

    float l_speed = 0.0f;
    float r_speed = 0.0f;

    unsigned long start_time = millis();
    const int timeoutMs = 10000; // 10 second timeout
    
    while((target_ticks > (std::abs(left_pos) + 5) || target_ticks > (std::abs(right_pos) + 5)) && 
          (millis() - start_time < timeoutMs)) {
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });
        delay(10);

        // Zrychlení
        if (((float(std::abs(left_pos)) + float(std::abs(right_pos)))/2) < (target_ticks - o_kolik_drive_zpomalovat)  && (std::abs(speed_left) < std::abs(speed) || std::abs(speed_right) < std::abs(speed))) {
            if((std::abs(base_speed_left) < std::abs(speed))) {
                speed_left += step_left;
            }
            if((std::abs(base_speed_right) < std::abs(speed))) {
                speed_right += step_right;
            }
        }
        // Zpomalování
        else if (((float(std::abs(left_pos)) + float(std::abs(right_pos))) / 2) > (target_ticks - o_kolik_drive_zpomalovat)) {
            if(a % deaccelating == 0) { // zpomaluj jen každých N cyklů pro plynulejší zpomalení
                if (std::abs(speed_left) > min_speed) {
                    speed_left -= step_left;
                }
                if (std::abs(speed_right) > min_speed) {
                    speed_right -= step_right;
                }
                a = 0; 
            }
            
            a++;
        }
        l_speed = speed_left;
        r_speed = speed_right;
        
        // Regulace
        float error = std::abs(left_pos) - std::abs(right_pos);
        float correction = error * 0.18f; // Proporcionální konstanta
        if(std::abs(correction) > max_correction) {
            correction = (correction > 0) ? max_correction : -max_correction;
        }

        if (error > 0) {
            // Levý je napřed - zpomalit levý
            if (base_speed_left > 0) {
                l_speed -= correction;  // Odečíst od kladné rychlosti = zpomalit
                r_speed += correction;  // Přidat ke kladné rychlosti = zrychlit
            } else {
                l_speed += correction;  // Přidat k záporné rychlosti = zpomalit
                r_speed -= correction;  // Odečíst od záporné rychlosti = zrychlit
            }
        }
        else {
            // Pravý je napřed - zpomalit pravý
            if (base_speed_right > 0) {
                r_speed += correction;  // Odečíst od záporné rychlosti = zpomalit
                l_speed -= correction;  // Přidat k záporné rychlosti = zrychlit
            } else {
                r_speed -= correction;  // Přidat ke kladné rychlosti = zpomalit
                l_speed += correction;  // Odečíst od kladné rychlosti = zrychlit
            }
        }

        // Omezení na minimum
        if (std::abs(l_speed) < min_speed) {
            l_speed = (l_speed > 0) ? min_speed : -min_speed;
        }
        if (std::abs(r_speed) < min_speed) {
            r_speed = (r_speed > 0) ? min_speed : -min_speed;
        }
        if(std::abs(l_speed) > (std::abs(base_speed_left) + 10)) {
            if(base_speed_left > 0) l_speed = base_speed_left + 10;
            else l_speed = base_speed_left - 10;
        }
        if(std::abs(r_speed) > (std::abs(base_speed_right) + 10)) {
            if(base_speed_right > 0) r_speed = base_speed_right + 10;
            else r_speed = base_speed_right - 10;
        }
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(l_speed));
        man.motor(m_id_right).speed(pctToSpeed(r_speed));
    }
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::radius_right(float radius, float angle, float speed) {
    auto& man = rb::Manager::get();
    
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    // Výpočet drah pro zatáčku VPRAVO
    float distance_left = (((radius + roztec_kol) * PI * angle) / 180) * konstanta_radius_vnejsi_kolo;  // vnější kolo
    float distance_right = (( radius * PI * angle) / 180)* konstanta_radius_vnitrni_kolo;               // vnitřní kolo

    int target_ticks_left = mmToTicks(distance_left);
    int target_ticks_right = mmToTicks(distance_right);

    // Základní výpočet rychlostí
    float target_speed_left = speed;  // vnější kolo
    float target_speed_right = speed * (radius / (roztec_kol + radius));  // vnitřní kolo

    // Úprava polarity
    if (m_polarity_switch_left)
        target_speed_left = -target_speed_left;
    if (m_polarity_switch_right)
        target_speed_right = -target_speed_right;

    // Proměnné pro zrychlení a zpomalení - RŮZNÉ PRO VNĚJŠÍ A VNITŘNÍ KOLO
    float step_outer = 3.0f; // větší krok pro vnější kolo
    float step_inner = 1.5f; // menší krok pro vnitřní kolo (menší rychlost)
    
    // MINIMÁLNÍ RYCHLOSTI V POMĚRU - místo pevné hodnoty
    float base_min_speed = 15.0f; // základní minimální rychlost
    float min_speed_outer = base_min_speed; // vnější kolo - vyšší minimum
    float min_speed_inner = base_min_speed * (target_speed_right / target_speed_left); // vnitřní kolo - poměrně nižší minimum
    
    // Omezení minimálních rychlostí na rozumné meze
    min_speed_outer = std::max(12.0f, std::min(min_speed_outer, 20.0f));
    min_speed_inner = std::max(10.0f, std::min(min_speed_inner, 18.0f));
    
    // RŮZNÉ BODY ZAČÁTKU ZPOMALOVÁNÍ - vnitřní kolo začne zpomalovat později
    int zpomalovat_outer = 1400; // vnější kolo začne zpomalovat dříve
    int zpomalovat_inner = zpomalovat_outer / 2; // vnitřní kolo začne zpomalovat 2x později
    
    byte counter_outer = 0;
    byte counter_inner = 0;
    
    // RŮZNÁ FREKVENCE ZPOMALOVÁNÍ
    byte deaccelating_outer = byte(240/std::abs(speed)); // častější zpomalování pro vnější kolo
    byte deaccelating_inner = byte(480/std::abs(speed)); // řidší zpomalování pro vnitřní kolo (2x méně často)

    // Směr kroku
    float step_dir_outer = (target_speed_left > 0) ? step_outer : -step_outer;
    float step_dir_inner = (target_speed_right > 0) ? step_inner : -step_inner;

    float current_speed_left = 0;
    float current_speed_right = 0;

    // P regulátor - konstanty
    const float Kp = 1.47f;
    const float max_speed_adjust = 5.9f;        

    int timeoutMs = 10000;
    unsigned long start_time = millis();
    bool left_done = false;
    bool right_done = false;

    int left_pos = 0;
    int right_pos = 0;

    while(millis() - start_time < timeoutMs) {
        // Synchronní čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);
        
        // VÝPOČET CELKOVÉHO POKROKU PRO KAŽDÉ KOLO ZVLÁŠŤ
        float progress_left = (float)std::abs(left_pos) / std::abs(target_ticks_left);
        float progress_right = (float)std::abs(right_pos) / std::abs(target_ticks_right);
        
        // ZRYCHLENÍ - PRO OBĚ KOLA
        if (std::abs(left_pos) < (std::abs(target_ticks_left) - zpomalovat_outer) && 
            std::abs(current_speed_left) < std::abs(target_speed_left)) {
            current_speed_left += step_dir_outer;
        }
        
        if (std::abs(right_pos) < (std::abs(target_ticks_right) - zpomalovat_inner) && 
            std::abs(current_speed_right) < std::abs(target_speed_right)) {
            current_speed_right += step_dir_inner;
        }
        
        // ZPOMALOVÁNÍ - VNĚJŠÍ KOLO (začne dříve a častěji)
        if (std::abs(left_pos) >= (std::abs(target_ticks_left) - zpomalovat_outer)) {
            if(counter_outer % deaccelating_outer == 0) {
                if (std::abs(current_speed_left) > min_speed_outer) {
                    current_speed_left -= step_dir_outer;
                }
                counter_outer = 0;
            }
            counter_outer++;
        }
        
        // ZPOMALOVÁNÍ - VNITŘNÍ KOLO (začne později a řidčeji)
        if (std::abs(right_pos) >= (std::abs(target_ticks_right) - zpomalovat_inner)) {
            if(counter_inner % deaccelating_inner == 0) {
                if (std::abs(current_speed_right) > min_speed_inner) {
                    current_speed_right -= step_dir_inner;
                }
                counter_inner = 0;
            }
            counter_inner++;
        }
        
        // P REGULÁTOR - synchronizace kol
        float progress_error = progress_left - progress_right;
        float speed_adjust = Kp * progress_error;
        
        // Omezení úpravy rychlosti
        if (speed_adjust > max_speed_adjust) speed_adjust = max_speed_adjust;
        if (speed_adjust < -max_speed_adjust) speed_adjust = -max_speed_adjust;
        
        // Upravené rychlosti
        float adjusted_speed_left = current_speed_left;
        float adjusted_speed_right = current_speed_right;
        
        if (!left_done && !right_done) {
            // Pokud vnější kolo (levé) je napřed, zpomalíme ho a/nebo zrychlíme vnitřní
            if (progress_error > 0) {
                adjusted_speed_left = current_speed_left * (1.0f - std::abs(speed_adjust));
                adjusted_speed_right = current_speed_right * (1.0f + std::abs(speed_adjust));
            }
            // Pokud vnitřní kolo (pravé) je napřed, zpomalíme ho a/nebo zrychlíme vnější
            else if (progress_error < 0) {
                adjusted_speed_left = current_speed_left * (1.0f + std::abs(speed_adjust));
                adjusted_speed_right = current_speed_right * (1.0f - std::abs(speed_adjust));
            }
            
            // Zajištění minimální rychlosti - S RŮZNÝMI MINIMY PRO KAŽDÉ KOLO
            if (std::abs(adjusted_speed_left) < min_speed_outer && std::abs(adjusted_speed_left) > 0) {
                adjusted_speed_left = (adjusted_speed_left > 0) ? min_speed_outer : -min_speed_outer;
            }
            if (std::abs(adjusted_speed_right) < min_speed_inner && std::abs(adjusted_speed_right) > 0) {
                adjusted_speed_right = (adjusted_speed_right > 0) ? min_speed_inner : -min_speed_inner;
            }
        }
        
        // Aplikace upravených rychlostí (i po dokončení jednoho kola musí druhé kolo plynule dojet a zpomalit)
        if (!left_done) {
            man.motor(m_id_left).speed(pctToSpeed(adjusted_speed_left));
        }
        if (!right_done) {
            man.motor(m_id_right).speed(pctToSpeed(adjusted_speed_right));
        }
        
        // Kontrola dokončení
        if (std::abs(left_pos)  >= std::abs(target_ticks_left) && !left_done) {
            left_done = true;
            man.motor(m_id_left).speed(0);
            man.motor(m_id_left).power(0);
        }
        
        if (std::abs(right_pos) >= std::abs(target_ticks_right) && !right_done) {
            right_done = true;
            man.motor(m_id_right).speed(0);
            man.motor(m_id_right).power(0);
        }
        
        if (left_done && right_done) {
            break;
        }
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::radius_left(float radius, float angle, float speed) {
    auto& man = rb::Manager::get();
    
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    // Výpočet drah pro zatáčku VLEVO
    float distance_left = ((radius * PI * angle) / 180) * konstanta_radius_vnitrni_kolo;               // vnitřní kolo
    float distance_right = (((radius + roztec_kol) * PI * angle) / 180) * konstanta_radius_vnejsi_kolo;  // vnější kolo

    int target_ticks_left = mmToTicks(distance_left);
    int target_ticks_right = mmToTicks(distance_right);

    // Základní výpočet rychlostí PRO ZATÁČKU VLEVO
    float target_speed_left = speed * (radius / (roztec_kol + radius));  // vnitřní kolo
    float target_speed_right = speed;  // vnější kolo

    // Úprava polarity
    if (m_polarity_switch_left)
        target_speed_left = -target_speed_left;
    if (m_polarity_switch_right)
        target_speed_right = -target_speed_right;

    // Proměnné pro zrychlení a zpomalení - RŮZNÉ PRO VNĚJŠÍ A VNITŘNÍ KOLO
    float step_outer = 3.0f; // větší krok pro vnější kolo (nyní pravé)
    float step_inner = 1.5f; // menší krok pro vnitřní kolo (nyní levé)
    
    // MINIMÁLNÍ RYCHLOSTI V POMĚRU - místo pevné hodnoty
    float base_min_speed = 15.0f; // základní minimální rychlost
    float min_speed_outer = base_min_speed; // vnější kolo - vyšší minimum
    float min_speed_inner = base_min_speed * (target_speed_left / target_speed_right); // vnitřní kolo - poměrně nižší minimum
    
    // Omezení minimálních rychlostí na rozumné meze
    min_speed_outer = std::max(12.0f, std::min(min_speed_outer, 20.0f));
    min_speed_inner = std::max(10.0f, std::min(min_speed_inner, 18.0f));
    
    // RŮZNÉ BODY ZAČÁTKU ZPOMALOVÁNÍ - vnitřní kolo začne zpomalovat později
    int zpomalovat_outer = 1400; // vnější kolo začne zpomalovat dříve
    int zpomalovat_inner = zpomalovat_outer / 2; // vnitřní kolo začne zpomalovat 2x později
    
    byte counter_outer = 0;
    byte counter_inner = 0;
    
    // RŮZNÁ FREKVENCE ZPOMALOVÁNÍ
    byte deaccelating_outer = byte(240/std::abs(speed)); // častější zpomalování pro vnější kolo
    byte deaccelating_inner = byte(480/std::abs(speed)); // řidší zpomalování pro vnitřní kolo (2x méně často)

    // Směr kroku
    float step_dir_outer = (target_speed_right > 0) ? step_outer : -step_outer; // vnější kolo je pravé
    float step_dir_inner = (target_speed_left > 0) ? step_inner : -step_inner;  // vnitřní kolo je levé

    float current_speed_left = 0;  // vnitřní kolo
    float current_speed_right = 0; // vnější kolo

    // P regulátor - konstanty
    const float Kp = 1.47f;
    const float max_speed_adjust = 5.9f;

    int timeoutMs = 10000;
    unsigned long start_time = millis();
    bool left_done = false;
    bool right_done = false;

    int left_pos = 0;
    int right_pos = 0;

    while(millis() - start_time < timeoutMs) {
        // Synchronní čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);
        
        // VÝPOČET CELKOVÉHO POKROKU PRO KAŽDÉ KOLO ZVLÁŠŤ
        float progress_left = (float)std::abs(left_pos) / std::abs(target_ticks_left);
        float progress_right = (float)std::abs(right_pos) / std::abs(target_ticks_right);
        
        // ZRYCHLENÍ - PRO OBĚ KOLA
        // Vnější kolo (pravé) zrychluje
        if (std::abs(right_pos) < (std::abs(target_ticks_right) - zpomalovat_outer) && 
            std::abs(current_speed_right) < std::abs(target_speed_right)) {
            current_speed_right += step_dir_outer;
        }
        
        // Vnitřní kolo (levé) zrychluje
        if (std::abs(left_pos) < (std::abs(target_ticks_left) - zpomalovat_inner) && 
            std::abs(current_speed_left) < std::abs(target_speed_left)) {
            current_speed_left += step_dir_inner;
        }
        
        // ZPOMALOVÁNÍ - VNĚJŠÍ KOLO (pravé, začne dříve a častěji)
        if (std::abs(right_pos) >= (std::abs(target_ticks_right) - zpomalovat_outer)) {
            if(counter_outer % deaccelating_outer == 0) {
                if (std::abs(current_speed_right) > min_speed_outer) {
                    current_speed_right -= step_dir_outer;
                }
                counter_outer = 0;
            }
            counter_outer++;
        }
        
        // ZPOMALOVÁNÍ - VNITŘNÍ KOLO (levé, začne později a řidčeji)
        if (std::abs(left_pos) >= (std::abs(target_ticks_left) - zpomalovat_inner)) {
            if(counter_inner % deaccelating_inner == 0) {
                if (std::abs(current_speed_left) > min_speed_inner) {
                    current_speed_left -= step_dir_inner;
                }
                counter_inner = 0;
            }
            counter_inner++;
        }
        
        // P REGULÁTOR - synchronizace kol
        float progress_error = progress_left - progress_right;
        float speed_adjust = Kp * progress_error;
        
        // Omezení úpravy rychlosti
        if (speed_adjust > max_speed_adjust) speed_adjust = max_speed_adjust;
        if (speed_adjust < -max_speed_adjust) speed_adjust = -max_speed_adjust;
        
        // Upravené rychlosti
        float adjusted_speed_left = current_speed_left;
        float adjusted_speed_right = current_speed_right;
        
        if (!left_done && !right_done) {
            // Pokud vnitřní kolo (levé) je napřed, zpomalíme ho a/nebo zrychlíme vnější
            if (progress_error > 0) {
                adjusted_speed_left = current_speed_left * (1.0f - std::abs(speed_adjust));
                adjusted_speed_right = current_speed_right * (1.0f + std::abs(speed_adjust));
            }
            // Pokud vnější kolo (pravé) je napřed, zpomalíme ho a/nebo zrychlíme vnitřní
            else if (progress_error < 0) {
                adjusted_speed_left = current_speed_left * (1.0f + std::abs(speed_adjust));
                adjusted_speed_right = current_speed_right * (1.0f - std::abs(speed_adjust));
            }
            
            // Zajištění minimální rychlosti - S RŮZNÝMI MINIMY PRO KAŽDÉ KOLO
            if (std::abs(adjusted_speed_left) < min_speed_inner && std::abs(adjusted_speed_left) > 0) {
                adjusted_speed_left = (adjusted_speed_left > 0) ? min_speed_inner : -min_speed_inner;
            }
            if (std::abs(adjusted_speed_right) < min_speed_outer && std::abs(adjusted_speed_right) > 0) {
                adjusted_speed_right = (adjusted_speed_right > 0) ? min_speed_outer : -min_speed_outer;
            }
        }
        
        // Aplikace upravených rychlostí (i po dokončení jednoho kola musí druhé kolo plynule dojet a zpomalit)
        if (!left_done) {
            man.motor(m_id_left).speed(pctToSpeed(adjusted_speed_left));
        }
        if (!right_done) {
            man.motor(m_id_right).speed(pctToSpeed(adjusted_speed_right));
        }
        
        // Kontrola dokončení
        if (std::abs(left_pos)  >= std::abs(target_ticks_left) && !left_done) {
            left_done = true;
            man.motor(m_id_left).speed(0);
            man.motor(m_id_left).power(0);
        }
        
        if (std::abs(right_pos) >= std::abs(target_ticks_right) && !right_done) {
            right_done = true;
            man.motor(m_id_right).speed(0);
            man.motor(m_id_right).power(0);
        }
        
        if (left_done && right_done) {
            break;
        }
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::forward_acc(float mm, float speed) {
    auto& man = rb::Manager::get();
    
    float m_kp = 0.23f; // Proporcionální konstanta
    float m_min_speed = 18.0f; // Minimální rychlost motorů
    float m_max_correction = 5.5f;

    byte step = 3;
    byte deaccelating = byte(250/std::abs(speed));
    byte a = 1;

    byte b = 1;
    byte accelerating = byte(300/std::abs(speed));

    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks_left = mmToTicks_left(mm);
    int target_ticks_right = mmToTicks_right(mm);
    float left_pos = 0;
    float right_pos = 0;
    float progres_left = 0.0f;
    float progres_right = 0.0f;
    float rozdil_progres = 0.0f;
    
    // Základní rychlosti s přihlédnutím k polaritě
    float base_speed_left = m_polarity_switch_left ? -speed : speed;
    float base_speed_right = m_polarity_switch_right ? -speed : speed;
    
    float step_left = (base_speed_left > 0) ? step : -step;
    float step_right = (base_speed_right > 0) ? step : -step;
    
    float current_speed_left = (base_speed_left > 0) ? m_min_speed : -m_min_speed;
    float current_speed_right = (base_speed_right > 0) ? m_min_speed : -m_min_speed;
    int o_kolik_drive_zpomalovat = int(10 * speed);    
    o_kolik_drive_zpomalovat = std::min(o_kolik_drive_zpomalovat, int(0.35f * std::min(target_ticks_left, target_ticks_right)));
    unsigned long start_time = millis();
    int timeoutMs = 1.7 * timeout_ms(mm, speed * 0.8);

    if(timeoutMs < 5000){
        timeoutMs = 5000;
    }
    
    while((target_ticks_left > std::abs(left_pos) || target_ticks_right > std::abs(right_pos)) && 
          (millis() - start_time < timeoutMs)) {
        
        // Čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);

        // Výpočet progresu
        progres_left = (float(std::abs(left_pos)) / float(target_ticks_left));
        progres_right = (float(std::abs(right_pos)) / float(target_ticks_right));
        rozdil_progres = progres_left - progres_right;

        // AKCELERACE A DEACELERACE
        float avg_progress = (progres_left + progres_right) / 2.0f;

        if((std::abs(left_pos) > (target_ticks_left - o_kolik_drive_zpomalovat)) && (std::abs(right_pos) > (target_ticks_right - o_kolik_drive_zpomalovat))) {
            // FÁZE ZPOMALENÍ
            if(a % deaccelating == 0) { // zpomaluj jen každých N cyklů pro plynulejší zpomalení
                if (std::abs(current_speed_left) > m_min_speed) {
                    current_speed_left -= step_left;
                }
                if (std::abs(current_speed_right) > m_min_speed) {
                    current_speed_right -= step_right;
                }
                a = 0; 
            }
            a++;
        }
        // Zrychlení
        else if((std::abs(current_speed_left) < std::abs(speed) || std::abs(current_speed_right) < std::abs(speed)) && (avg_progress < 0.4)) {
            if(b % accelerating == 0){
                if((std::abs(current_speed_left) < std::abs(speed))) {
                    current_speed_left += step_left;
                }
                if((std::abs(current_speed_right) < std::abs(speed))) {
                    current_speed_right += step_right;
                }
                b = 0;
            }
            b++;
        }
        else{
            // FÁZE KONSTANTNÍ RYCHLOSTI
            current_speed_left = base_speed_left;
            current_speed_right = base_speed_right;
        }

        float correction = rozdil_progres * m_kp * 1800;
        correction = std::max(-m_max_correction, std::min(correction, m_max_correction));
        
        // Výpočet korigovaných rychlostí
        float speed_left = current_speed_left;
        float speed_right = current_speed_right;
        
        // Aplikace korekce podle polarity
        if (correction > 0) {
            // Levý je napřed - zpomalit levý
            if (m_polarity_switch_left) {
                speed_left += correction;
                speed_right += correction;
            } else {
                speed_left -= correction;
                speed_right -= correction;
            }
        } else if (correction < 0) {
            // Pravý je napřed - zpomalit pravý
            if (m_polarity_switch_right) {
                speed_right -= correction;
                speed_left -= correction;
            } else {
                speed_right += correction;
                speed_left += correction;
            }
        }
        
        // Zajištění minimální rychlosti
        if (std::abs(speed_left) < m_min_speed && std::abs(speed_left) > 0) {
            speed_left = (speed_left > 0) ? m_min_speed : -m_min_speed;
        }
        if (std::abs(speed_right) < m_min_speed && std::abs(speed_right) > 0) {
            speed_right = (speed_right > 0) ? m_min_speed : -m_min_speed;
        }
        
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(speed_left));
        man.motor(m_id_right).speed(pctToSpeed(speed_right));
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::backward_acc(float mm, float speed) {
    auto& man = rb::Manager::get();
    
    float m_kp = 0.23f; // Proporcionální konstanta
    float m_min_speed = 18.0f; // Minimální rychlost motorů
    float m_max_correction = 5.5f;

    byte step = 3;
    byte deaccelating = byte(250/std::abs(speed));
    byte a = 1;

    byte b = 1;
    byte accelerating = byte(300/std::abs(speed));

    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);

    int target_ticks_left = mmToTicks_left(mm);
    int target_ticks_right = mmToTicks_right(mm);
    float left_pos = 0;
    float right_pos = 0;
    float progres_left = 0.0f;
    float progres_right = 0.0f;
    float rozdil_progres = 0.0f;
    
    // Základní rychlosti s přihlédnutím k polaritě
    float base_speed_left = m_polarity_switch_left ? speed : -speed;
    float base_speed_right = m_polarity_switch_right ? speed : -speed;
    
    float step_left = (base_speed_left > 0) ? step : -step;
    float step_right = (base_speed_right > 0) ? step : -step;

    float current_speed_left = (base_speed_left > 0) ? m_min_speed : -m_min_speed;
    float current_speed_right = (base_speed_right > 0) ? m_min_speed : -m_min_speed;
    
    int o_kolik_drive_zpomalovat = int(10 * speed);    
    o_kolik_drive_zpomalovat = std::min(o_kolik_drive_zpomalovat, int(0.35f * std::min(target_ticks_left, target_ticks_right)));
    unsigned long start_time = millis();
    int timeoutMs = 1.7 * timeout_ms(mm, speed * 0.8);

    if(timeoutMs < 5000){
        timeoutMs = 5000;
    }
    
    while((target_ticks_left > std::abs(left_pos) || target_ticks_right > std::abs(right_pos)) && 
          (millis() - start_time < timeoutMs)) {
        
        // Čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);

        // Výpočet progresu
        progres_left = (float(std::abs(left_pos)) / float(target_ticks_left));
        progres_right = (float(std::abs(right_pos)) / float(target_ticks_right));
        rozdil_progres = progres_left - progres_right;

        // AKCELERACE A DEACELERACE
        float avg_progress = (progres_left + progres_right) / 2.0f;

        if((std::abs(left_pos) > (target_ticks_left - o_kolik_drive_zpomalovat)) && (std::abs(right_pos) > (target_ticks_right - o_kolik_drive_zpomalovat))) {
            // FÁZE ZPOMALENÍ
            if(a % deaccelating == 0) { // zpomaluj jen každých N cyklů pro plynulejší zpomalení
                if (std::abs(current_speed_left) > m_min_speed) {
                    current_speed_left -= step_left;
                }
                if (std::abs(current_speed_right) > m_min_speed) {
                    current_speed_right -= step_right;
                }
                a = 0; 
            }
            a++;
        }
        // Zrychlení
        else if((std::abs(current_speed_left) < std::abs(speed) || std::abs(current_speed_right) < std::abs(speed)) && (avg_progress < 0.4)) {
            if(b % accelerating == 0){
                if((std::abs(current_speed_left) < std::abs(speed))) {
                    current_speed_left += step_left;
                }
                if((std::abs(current_speed_right) < std::abs(speed))) {
                    current_speed_right += step_right;
                }
                b = 0;
            }
            b++;
        }
        else{
            // FÁZE KONSTANTNÍ RYCHLOSTI
            current_speed_left = base_speed_left;
            current_speed_right = base_speed_right;
        }

        float correction = rozdil_progres * m_kp * 1800;
        correction = std::max(-m_max_correction, std::min(correction, m_max_correction));
        
        // Výpočet korigovaných rychlostí
        float speed_left = current_speed_left;
        float speed_right = current_speed_right;
        
        // Aplikace korekce podle polarity
        if (correction > 0) {
            // Levý je napřed - zpomalit levý
            if (m_polarity_switch_left) {
                speed_left -= correction;
                speed_right -= correction;
            } else {
                speed_left += correction;
                speed_right += correction;
            }
        } else if (correction < 0) {
            // Pravý je napřed - zpomalit pravý
            if (m_polarity_switch_right) {
                speed_right += correction;
                speed_left += correction;
            } else {
                speed_right -= correction;
                speed_left -= correction;
            }
        }
        
        // Zajištění minimální rychlosti
        if (std::abs(speed_left) < m_min_speed && std::abs(speed_left) > 0) {
            speed_left = (speed_left > 0) ? m_min_speed : -m_min_speed;
        }
        if (std::abs(speed_right) < m_min_speed && std::abs(speed_right) > 0) {
            speed_right = (speed_right > 0) ? m_min_speed : -m_min_speed;
        }
        
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(speed_left));
        man.motor(m_id_right).speed(pctToSpeed(speed_right));
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_right).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}

inline void Motors::back_buttons(float speed, std::function<bool()> first_button, std::function<bool()> second_button) {
    auto& man = rb::Manager::get();
    
    float m_kp = 0.23f; // Proporcionální konstanta
    float m_min_speed = 15.0f; // Minimální rychlost motorů
    float m_max_correction = 5.0f; // Maximální korekce rychlosti

    bool left_done = false;
    bool right_done = false;
    
    // Reset pozic
    man.motor(m_id_left).setCurrentPosition(0);
    man.motor(m_id_right).setCurrentPosition(0);
    
    int left_pos = 0;
    int right_pos = 0;

    byte step = 3;
    // Základní rychlosti s přihlédnutím k polaritě
    float base_speed_left = m_polarity_switch_left ? speed : -speed;
    float base_speed_right = m_polarity_switch_right ? speed : -speed;

    float step_left = (base_speed_left > 0) ? step : -step;
    float step_right = (base_speed_right > 0) ? step : -step;

    float current_speed_left = (base_speed_left > 0) ? m_min_speed : -m_min_speed;
    float current_speed_right = (base_speed_right > 0) ? m_min_speed : -m_min_speed;

    byte a = 0;
    
    unsigned long start_time = millis();
    int timeoutMs = 10000;
    
    while((millis() - start_time) < timeoutMs) {
        
        // Čtení pozic
        man.motor(m_id_left).requestInfo([&](rb::Motor& info) {
             left_pos = info.position();
          });
        man.motor(m_id_right).requestInfo([&](rb::Motor& info) {
             right_pos = info.position();
          });

        delay(10);
        
        // Zrychlování
        if((std::abs(current_speed_left) < std::abs(speed)) && (std::abs(current_speed_right) < std::abs(speed)) && (a % 9 == 0)){
            current_speed_left += step_left;
            current_speed_right += step_right;
            a = 0;
        }
        a++;

        // P regulátor - pracuje s absolutními hodnotami pozic
        int error = std::abs(left_pos) - std::abs(right_pos);

        float correction = (error) * m_kp;
        correction = std::max(-m_max_correction, std::min(correction, m_max_correction));
        
        // Výpočet korigovaných rychlostí
        float speed_left = current_speed_left;
        float speed_right = current_speed_right;
        
        // Aplikace korekce podle polarity
        if (error > 0) {
            // Levý je napřed - zpomalit levý
            if (m_polarity_switch_left) {
                speed_left -= correction;  // Přidat k záporné rychlosti = zpomalit
            } else {
                speed_left += correction;  // Odečíst od kladné rychlosti = zpomalit
            }
        } else if (error < 0) {
            // Pravý je napřed - zpomalit pravý
            if (m_polarity_switch_right) {
                speed_right += correction;  // Odečíst od záporné rychlosti = zpomalit
            } else {
                speed_right -= correction;  // Přidat ke kladné rychlosti = zpomalit
            }
        }
        
        // Zajištění minimální rychlosti
        if (std::abs(speed_left) < m_min_speed) {
            speed_left = (speed_left > 0) ? m_min_speed : -m_min_speed;
        }
        if (std::abs(speed_right) < m_min_speed) {
            speed_right = (speed_right > 0) ? m_min_speed : -m_min_speed;
        }
        
        // Nastavení výkonu motorů
        man.motor(m_id_left).speed(pctToSpeed(speed_left));
        man.motor(m_id_right).speed(pctToSpeed(speed_right));

        if(first_button() && !left_done) {
            start_time = millis();
            timeoutMs = 3000;
            left_done = true;
        }
        if(second_button() && !right_done) {
            start_time = millis();
            timeoutMs = 3000;
            right_done = true;
        }
        if(left_done && right_done) {
            delay(50);
            break;
        }
    }
    
    // Zastavení motorů
    man.motor(m_id_left).speed(0);
    man.motor(m_id_left).speed(0);
    man.motor(m_id_left).power(0);
    man.motor(m_id_right).power(0);
}
