import serial
import time

def main():
    print("UART Test: Raspberry Pi")
    
    port = '/dev/serial0' # Raspberry Pi UART
    baud_rate = 115200
    
    try:
        ser = serial.Serial(port, baud_rate, timeout=1)
        time.sleep(2)
        ser.reset_input_buffer()
        print("[STAV: USPECH] UART inicializovan. Zacinam zasilat zpravy...")
    except Exception as e:
        print(f"[STAV: CHYBA] Chyba pri otevirani portu: {e}")
        return

    while True:
        # Odeslani zpravy na RBCX
        zprava = "Ahoj RBCX\n"
        try:
            ser.write(zprava.encode('utf-8'))
            print("[STAV: ODESLANO] Zprava 'Ahoj RBCX' uspesne odeslana na UART.")
        except Exception as e:
            print(f"[STAV: CHYBA] Nelze odeslat zpravu: {e}")
        
        # Cteni odpovedi (pokud nejaka je)
        if ser.in_waiting > 0:
            while ser.in_waiting > 0:
                try:
                    odpoved = ser.readline().decode('utf-8', errors='ignore').strip()
                    if odpoved:
                        print(f"[STAV: USPECH] Prijato od RBCX: {odpoved}")
                    else:
                        print("[STAV: VAROVANI] Prijat prazdny retezec.")
                except Exception as e:
                    print(f"[STAV: CHYBA] Chyba pri cteni z UART: {e}")
        else:
            print("[STAV: CEKAM] Zatim nedorazila zadna odpoved z RBCX.")
            
        time.sleep(1)

if __name__ == "__main__":
    main()