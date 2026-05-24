import cv2  # Import the OpenCV library 
from ultralytics import YOLO  # Import the YOLO class from the Ultralytics library
import numpy as np  # Import NumPy for numerical operationse
import serial  # Import the pyserial library for serial communication
import time  # Import the time library for sleep functionality
import struct
import os
import threading


def ComunictionSetup():
    port = '/dev/serial0' # port pro komunikaci s Raspberry Pi /dev/ttyACM0
    baud_rate = 115200 # rychlost komunikace
    ser = serial.Serial(port, baud_rate, timeout=1)
    time.sleep(2)
    ser.reset_input_buffer()
    print("serial comunication setup done")
    return ser


def pack_message(x, y, camera, on, angle, distance, max_distance):
    # x, y (uint16_t), camera (bool), on (bool), angle (int16_t), distance (uint16_t), max_distance (uint16_t)
    payload = struct.pack('<HH??hHH', int(x) & 0xFFFF, int(y) & 0xFFFF, bool(camera), bool(on), int(angle), int(distance) & 0xFFFF, int(max_distance) & 0xFFFF)
    checksum = sum(payload) % 256
    message = bytes([0xAA, 0x55]) + payload + bytes([checksum])
    return message


def SendData(ser, x, y, camera, on, angle, distance, max_distance):
    msg = pack_message(x, y, camera, on, angle, distance, max_distance)
    ser.write(msg)
    print(f"[RPi UART TX] Odeslána data robotu: x={x} mm, y={y} mm, distance={distance} mm")


def waitForResponse(ser):
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            return line
        


# Function to set up the camera
def CameraSetup():
    cap = cv2.VideoCapture(0)  # Zmeneno z 1 na 0 (bezna hodnota na Raspberry Pi)
    if not cap.isOpened():
        print("Kamera na indexu 0 nenalezena, zkousim index 1...")
        cap = cv2.VideoCapture(1)
        
    if not cap.isOpened():
        print("CRITICAL ERROR: Nepodarilo se otevrit zadnou kameru!")
        return None

    # Set video format to MJPG (better performance than default)
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc('M','J','P','G'))

    # Set the width and height of the video frame
    cap.set(3, 640)  # Width (sníženo z 960 na 640)
    cap.set(4, 480)  # Height (sníženo z 640 na 480)

    print("Camera setup complete.")
    return cap


# Function to capture a frame from the camera
def GetImage(cap):
    if cap is None:
        return None
    # Pouze jedno přečtení pro maximální FPS (vlákno na pozadí doplňuje data kontinuálně)
    ret, image = cap.read()
    if not ret:
        return None
    return image


# Function to display an image in a window
def ShowImage(img):
    cv2.imshow('image', img)  # Show the image in a new window


# Function to load a YOLOv11 model
def LoadModel(model_name="yolo11n.pt"):
    # Cesta k modelu se bere relativne ke slozce, ve ktere je tento skript
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = os.path.join(script_dir, model_name)
    
    model = YOLO(model_path)  # Load the model from the given path
    print(f"Model successfully loaded from {model_path}")
    return model


# Function to perform object detection on a given source
def GetResult(model, source):
    results = model.predict(source, verbose=False)  # Predict on the input (image, video, etc.)
    result = results[0]  # Take the first result (usually only one for static image)
    return result


# Function to extract and print bounding box data
def ObtainData(result):
    for box in result.boxes:
        cls = int(box.cls[0])  # Get class index
        x, y, w, h = box.xywh[0]  # Get box center x, y, width, height
        x, y, w, h = int(x), int(y), int(w), int(h)  # Convert to integers
        conf = float(box.conf[0])  # Get confidence score
        print(f"Box: x={x}, y={y}, w={w}, h={h}, Confidence: {conf:.2f}, Class: {cls}")
        if cls == 77:
            return (x, y, w, h)  # Return box if class is 77 (teddy bear)
    return None  # Return None if no box of class 77 is found


# Function to convert pixel offset to cm offset based on distance
def px_to_cm_x_offset(offset_px, distance_cm, frame_width=960, fov_deg=45):
    # Každý pixel představuje určitý úhel
    deg_per_px = fov_deg / frame_width
    angle_rad = np.radians(offset_px * deg_per_px)

    # Přepočet offsetu na cm pomocí tangensu a hloubky
    cm_offset = np.tan(angle_rad) * distance_cm
    return cm_offset


# Globální proměnná pro stav spuštění
started = False

def click_event(event, x, y, flags, param):
    global started
    if event == cv2.EVENT_LBUTTONDOWN:
        # Pokud uživatel klikne do obdélníku START tlačítka (x: 20-170, y: 20-70)
        if 20 <= x <= 170 and 20 <= y <= 70:
            started = True
            print("[RPi MAIN] Tlacitko START stisknuto! Nyni cekam na 'inposition' z robota.")


# === Konfigurace hybridního trackeru ===
CROP_Y = 240         # Oříznutí horní části obrazu (nastaveno na 240 pro dolní polovinu ze 480 px)
HSV_H_TOL = 20       # Tolerance Hue pro barevnou masku
HSV_S_TOL = 75       # Tolerance Saturation
HSV_V_TOL = 75       # Tolerance Value

data_lock = threading.Lock()
frame_lock = threading.Lock()

latest_frame = None       # Sdílený nejnovější snímek (640x480)
track_color = None        # Průměrná HSV barva medvěda
have_color = False        # Indikátor, zda máme platnou barvu pro HSV tracking
yolo_bear_data = None     # Poslední data nalezená YOLO (cx, cy, w, h) v 640x480 souřadnicích
hsv_bear_data = None      # Poslední data nalezená HSV (cx, cy, w, h) v 640x480 souřadnicích


def detector_thread():
    """Vlákno na pozadí běžící 1x za sekundu, které spouští YOLO na oříznutém obrázku."""
    global latest_frame, track_color, have_color, yolo_bear_data
    print("[RPi YOLO Thread] Detekční vlákno na pozadí spuštěno.")
    while True:
        time.sleep(1.0)
        
        with frame_lock:
            if latest_frame is None:
                continue
            frame_copy = latest_frame.copy()
            
        # Oříznutí horních CROP_Y pixelů (např. 320 px)
        cropped_img = frame_copy[CROP_Y:480, 0:640]
        
        # YOLO inference na malém výřezu je extrémně rychlá
        results = model.predict(cropped_img, verbose=False)
        result = results[0]
        
        bear_found = False
        for box in result.boxes:
            cls = int(box.cls[0])
            if cls == 77:  # Teddy bear (třída 77)
                x_crop, y_crop, w, h = box.xywh[0]
                x_crop, y_crop, w, h = int(x_crop), int(y_crop), int(w), int(h)
                
                # Přepočet souřadnic středu zpět do celého obrazu 640x480
                cx = x_crop
                cy = y_crop + CROP_Y
                
                # Získání průměrné HSV barvy medvěda z malého okolí středu v oříznutém snímku
                hsv_cropped = cv2.cvtColor(cropped_img, cv2.COLOR_BGR2HSV)
                r = 2  # Menší 5x5 okolí pro zamezení průměrování různých barev medvěda a pozadí
                x_lo = max(0, x_crop - r)
                x_hi = min(640, x_crop + r + 1)
                y_lo = max(0, y_crop - r)
                y_hi = min(480 - CROP_Y, y_crop + r + 1)
                
                roi = hsv_cropped[y_lo:y_hi, x_lo:x_hi]
                if roi.size > 0:
                    h_val, s_val, v_val, _ = cv2.mean(roi)
                    
                    with data_lock:
                        track_color = (h_val, s_val, v_val)
                        have_color = True
                        yolo_bear_data = (cx, cy, w, h)
                    bear_found = True
                    break
                    
        if not bear_found:
            with data_lock:
                # Pokud YOLO nenašel medvěda, okamžitě vypneme barevné sledování (ochrana proti šumu)
                have_color = False
                yolo_bear_data = None


# Main execution
ser = ComunictionSetup()  # Set up serial communication

model = LoadModel()  # Load the YOLOv11 model
cap = CameraSetup()       # Initialize the camera

cv2.namedWindow('image')
cv2.setMouseCallback('image', click_event)

# Spuštění detekčního vlákna na pozadí
det_thread = threading.Thread(target=detector_thread, daemon=True)
det_thread.start()

waiting_for_robot = True
print("[RPi MAIN] RPi je pripraveno. Klikni na tlacitko START v okne kamery.")

while True:
    image = GetImage(cap)        # Capture a frame
    if image is None:
        print("[RPi Video] Čekám na obraz z kamery...")
        time.sleep(1)
        continue

    # Uložení snímku pro detekční vlákno
    with frame_lock:
        latest_frame = image.copy()

    # Zpracování UARTu pouze pokud jsme odstartovali
    if started and ser.in_waiting > 0:
        try:
            line = ser.readline().decode('utf-8').strip()
            if line:
                print(f"[RPi UART RX] Přijato od robota: '{line}'")
                if "inposition" in line:
                    waiting_for_robot = False
                    print("\n[RPi MAIN] --- ROBOT JE NA MISTE ---")
                    print("[RPi MAIN] Zacinam vyhodnocovat snimky naplno (max FPS)...")
                    # Vymazání fronty pro čistý snímek
                    for _ in range(5):
                        cap.read()
        except Exception as e:
            print(f"[RPi UART RX] Chyba při čtení: {e}")
            
    elif not started and ser.in_waiting > 0:
        # Před startem zahazujeme balast ze seriové linky
        ser.reset_input_buffer()

    # Načtení sdílených dat z YOLO vlákna
    with data_lock:
        cur_have_color = have_color
        cur_color = track_color
        cur_yolo_data = yolo_bear_data

    # --- HSV Sledování v hlavním cyklu ---
    hsv_tracked = False
    hsv_bear_data_local = None

    if cur_have_color and cur_color is not None:
        cropped_img = image[CROP_Y:480, 0:640]
        cropped_hsv = cv2.cvtColor(cropped_img, cv2.COLOR_BGR2HSV)
        
        h, s, v = cur_color
        h0, h1 = h - HSV_H_TOL, h + HSV_H_TOL
        s0, s1 = max(0, s - HSV_S_TOL), min(255, s + HSV_S_TOL)
        v0, v1 = max(0, v - HSV_V_TOL), min(255, v + HSV_V_TOL)
        
        # Ošetření přetečení Hue
        if h0 < 0 or h1 > 179:
            lower1, upper1 = (0, s0, v0), (h1 % 180, s1, v1)
            lower2, upper2 = (h0 % 180, s0, v0), (179, s1, v1)
            m1 = cv2.inRange(cropped_hsv, lower1, upper1)
            m2 = cv2.inRange(cropped_hsv, lower2, upper2)
            mask = cv2.bitwise_or(m1, m2)
        else:
            mask = cv2.inRange(cropped_hsv, (h0, s0, v0), (h1, s1, v1))
            
        mask = cv2.erode(mask, None, iterations=2)
        mask = cv2.dilate(mask, None, iterations=2)
        
        cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        bestA, bestPt, bestBox = 0, None, None
        for c in cnts:
            x_c, y_c, w_c, h_c = cv2.boundingRect(c)
            A = cv2.contourArea(c)
            aspect_ratio = w_c / h_c if h_c != 0 else 0
            # Teddy bear je dost velký objekt
            if A > 80 and 0.4 <= aspect_ratio <= 2.5:  # Sníženo ze 150 pro citlivější detekci v dálce
                if A > bestA:
                    M = cv2.moments(c)
                    if M['m00'] > 0:
                        bestA = A
                        bestPt = (int(M['m10']/M['m00']), int(M['m01']/M['m00']))
                        bestBox = (x_c, y_c, w_c, h_c)
                        
        if bestPt is not None:
            cx = bestPt[0]
            cy = bestPt[1] + CROP_Y
            w = bestBox[2]
            h = bestBox[3]
            hsv_bear_data_local = (cx, cy, w, h)
            hsv_tracked = True
            
            with data_lock:
                hsv_bear_data = hsv_bear_data_local
        else:
            # Barevný tracking ztratil cíl, ale barevný režim nevypínáme okamžitě
            # (pouze vynulujeme hsv data a necháme běžet dál, dokud YOLO nepotvrdí ztrátu)
            with data_lock:
                hsv_bear_data = None

    # Určení finálních dat medvěda pro tento snímek
    if hsv_tracked:
        last_bear_data = hsv_bear_data_local
        tracker_mode = "HSV"
    else:
        last_bear_data = cur_yolo_data
        tracker_mode = "YOLO"

    # Výpočet souřadnic v centimetrech
    x_cm_out, y_cm_out = None, None
    if last_bear_data is not None:
        x, y, w, h = last_bear_data
        
        # Přepočet souřadnic ze 640x480 na virtuální 960x640 pro zachování kalibrace
        x_960 = x * 1.5
        y_bottom_960 = (y + round(h/2)) * (640.0 / 480.0)
        relative_x = x_960 - 480
        relative_y_bottom = 690 - y_bottom_960
        y_cm = 0.07895052 * np.e**(0.02147171 * relative_y_bottom) + 35.468015
        x_cm = px_to_cm_x_offset(relative_x, y_cm, frame_width=960)
        x_cm_out, y_cm_out = x_cm, y_cm
        
        # Vykreslení prvků detekce (Zelená = HSV, Modrá = YOLO)
        color_box = (0, 255, 0) if tracker_mode == "HSV" else (255, 0, 0)
        cv2.circle(image, (x, y + round(h/2)), 5, (255, 0, 255), -1)
        cv2.rectangle(image, (x - round(w/2), y - round(h/2)), (x + round(w/2), y + round(h/2)), color_box, 2)
        cv2.line(image, (x, 0), (x, 480), (0, 0, 255), 1)
        cv2.line(image, (0, y + round(h/2)), (640, y + round(h/2)), (0, 0, 255), 1)
        cv2.line(image, (320, 0), (320, 480), (255, 0, 0), 1)
        cv2.line(image, (0, 240), (640, 240), (255, 0, 0), 1)

        # Text s vypočtenou vzdáleností
        text = f"Mode: {tracker_mode} X: {x_cm:.1f} cm, Y: {y_cm:.1f} cm"
        font = cv2.FONT_HERSHEY_SIMPLEX
        cv2.putText(image, text, (image.shape[1] - 320, image.shape[0] - 20), font, 0.6, (0, 255, 255), 2)

    # Vykreslení hranice ořezu
    cv2.line(image, (0, CROP_Y), (640, CROP_Y), (0, 255, 255), 2)
    cv2.putText(image, "Hranice orezu", (10, CROP_Y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)

    # Vykreslování stavového UI (semafor) vlevo nahore
    if not started:
        cv2.rectangle(image, (20, 20), (170, 70), (0, 0, 255), -1)
        cv2.putText(image, "START", (40, 55), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        cv2.putText(image, "Cekam na START...", (200, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    elif waiting_for_robot:
        cv2.circle(image, (50, 50), 20, (0, 165, 255), -1)
        cv2.putText(image, "Ceka na RBCX ('inposition')...", (90, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 165, 255), 2)
    else:
        cv2.circle(image, (50, 50), 20, (255, 0, 255), -1)
        cv2.putText(image, "DETEKCE AKTIVNI - Hledam medveda!", (90, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 255), 2)

    # Logika aktivního režimu a odesílání dat
    is_active_mode = started and not waiting_for_robot
    if is_active_mode:
        if last_bear_data is not None:
            # Převod na milimetry pro odeslání
            x_mm = int(x_cm_out * 10)
            y_mm = int(y_cm_out * 10)
            print(f"[RPi MAIN] Odesílám souřadnice medvěda ({tracker_mode}): x={x_cm_out:.2f} cm, y={y_cm_out:.2f} cm")
            SendData(ser, x=x_mm, y=y_mm, camera=True, on=True, angle=0, distance=y_mm, max_distance=1300)
            print("[RPi MAIN] Data odeslana. Prechazim zpet do rezimu cekani (oranzova).")
            waiting_for_robot = True
            # Vymažeme data po odeslání, abychom neposlali staré/stojící souřadnice znovu
            last_bear_data = None
            with data_lock:
                yolo_bear_data = None
                hsv_bear_data = None

    ShowImage(image)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()