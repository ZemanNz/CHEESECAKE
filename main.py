import cv2  # Import the OpenCV library 
from ultralytics import YOLO  # Import the YOLO class from the Ultralytics library
import numpy as np  # Import NumPy for numerical operationse
import serial  # Import the pyserial library for serial communication
import time  # Import the time library for sleep functionality


def ComunictionSetup():
    port = '/dev/serial0' # port pro komunikaci s Raspberry Pi /dev/ttyACM0
    baud_rate = 115200 # rychlost komunikace
    ser = serial.Serial(port, baud_rate, timeout=1)
    time.sleep(2)
    ser.reset_input_buffer()
    print("serial comunication setup done")
    return ser

import struct

def pack_message(x, y, camera, on, angle, distance, max_distance):
    # x, y (uint16_t), camera (bool), on (bool), angle (int16_t), distance (uint16_t), max_distance (uint16_t)
    payload = struct.pack('<HH??hHH', int(x) & 0xFFFF, int(y) & 0xFFFF, bool(camera), bool(on), int(angle), int(distance) & 0xFFFF, int(max_distance) & 0xFFFF)
    checksum = sum(payload) % 256
    message = bytes([0xAA, 0x55]) + payload + bytes([checksum])
    return message

def SendData(ser, x, y, camera, on, angle, distance, max_distance):
    msg = pack_message(x, y, camera, on, angle, distance, max_distance)
    ser.write(msg)
    print(f"[RPi UART TX] Odeslána data robotu: x={x:.2f} cm, y={y:.2f} cm, distance={distance} mm")

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
    cap.set(3, 960)  # Width
    cap.set(4, 640)  # Height

    print("Camera setup complete.")
    return cap

# Function to capture a frame from the camera
def GetImage(cap):
    if cap is None:
        return None
    for i in range(5):  # Clear the buffer by reading a few frames
        ret, image = cap.read()  # 'ret' is a success flag; 'image' is the captured frame
    if not ret:
        return None
    return image

# Function to display an image in a window
def ShowImage(img):
    cv2.imshow('image', img)  # Show the image in a new window

import os

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
    results = model.predict(source)  # Predict on the input (image, video, etc.)
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
        ###############################################################################
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

# Main execution
ser = ComunictionSetup()  # Set up serial communication

model = LoadModel()  # Load the YOLOv11 model
cap = CameraSetup()       # Initialize the camera

cv2.namedWindow('image')
cv2.setMouseCallback('image', click_event)

waiting_for_robot = True
print("[RPi MAIN] RPi je pripraveno. Klikni na tlacitko START v okne kamery.")

last_detect_time = 0
last_bear_data = None

while True:
    image = GetImage(cap)        # Capture a frame
    if image is None:
        print("[RPi Video] Čekám na obraz z kamery...")
        time.sleep(1)
        continue

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

    # Logika spouštění YOLO
    is_active_mode = started and not waiting_for_robot
    should_detect = is_active_mode or (time.time() - last_detect_time > 2.0)
    
    if should_detect:
        result = GetResult(model, image)
        last_bear_data = ObtainData(result)
        last_detect_time = time.time()

    # Vykreslování výsledků detekce (aby obraz neblikal, vykreslujeme vzdy i posledni znama data)
    x_cm_out, y_cm_out = None, None
    if last_bear_data is not None:
        x, y, w, h = last_bear_data
        relative_x = x - 480
        relative_y_bottom = 690 - (y + round(h/2))
        y_cm = 0.07895052 * np.e**(0.02147171 * relative_y_bottom) + 35.468015
        x_cm = px_to_cm_x_offset(relative_x, y_cm)
        x_cm_out, y_cm_out = x_cm, y_cm
        
        # Zeleny ramecek kolem medveda
        cv2.circle(image, (x, y + round(h/2)), 5, (255, 0, 255), -1)
        cv2.rectangle(image, (x - round(w/2), y - round(h/2)), (x + round(w/2), y + round(h/2)), (0, 255, 0), 2)
        cv2.line(image, (x, 0), (x, 640), (0, 0, 255), 1)
        cv2.line(image, (0, y + round(h/2)), (960, y + round(h/2)), (0, 0, 255), 1)
        cv2.line(image, (480, 0), (480, 640), (255, 0, 0), 1)
        cv2.line(image, (0, 320), (960, 320), (255, 0, 0), 1)

        # Text s vypoctenou vzdalenosti vpravo dole
        text = f"X distance: {x_cm:.1f}, Y distance: {y_cm:.1f}"
        font = cv2.FONT_HERSHEY_SIMPLEX
        text_size, _ = cv2.getTextSize(text, font, 0.7, 2)
        cv2.putText(image, text, (image.shape[1] - text_size[0] - 50, image.shape[0] - 50), font, 0.7, (0,255,255), 2)

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

    # Odesílání dat, POUZE pokud jsme v aktivním režimu a POUZE pokud máme čerstvě spočítanou detekci
    if is_active_mode and should_detect:
        if last_bear_data is not None:
            # Převod na milimetry pro odeslání
            x_mm = int(x_cm_out * 10)
            y_mm = int(y_cm_out * 10)
            print(f"[RPi YOLO] Detekován medvěd. Vypočtená vzdálenost: x={x_cm_out:.2f} cm, y={y_cm_out:.2f} cm")
            SendData(ser, x=x_mm, y=y_mm, camera=True, on=True, angle=0, distance=y_mm, max_distance=1300)
            print("[RPi MAIN] Data odeslana. Prechazim zpet do rezimu cekani (oranzova).")
            waiting_for_robot = True
        else:
            print("[RPi YOLO] Nebyl detekován žádný medvěd. Zkousim dalsi snimek...")

    ShowImage(image)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()