import cv2  # Import the OpenCV library 
from ultralytics import YOLO  # Import the YOLO class from the Ultralytics library
import numpy as np  # Import NumPy for numerical operationse
import serial  # Import the pyserial library for serial communication
import time  # Import the time library for sleep functionality


def ComunictionSetup():
    port = '/dev/cu.usbmodem101' # port pro komunikaci s Raspberry Pi /dev/ttyACM0
    baund_rate = 115200 # rychlost komunikace
    ser = serial.Serial(port,baund_rate,timeout=1)
    time.sleep(2)
    ser.reset_input_buffer()
    print("serial comunication setup done")
    return ser

def SendData(data):
    command = f"{data}\n"
    ser.write(command.encode('utf-8'))
    print('data send')

def waitForResponse():
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            return line
        

# Function to set up the camera
def CameraSetup():
    cap = cv2.VideoCapture(1)  # Open the default camera (0 = built-in webcam)

    # Set video format to MJPG (better performance than default)
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc('M','J','P','G'))

    # Set the width and height of the video frame
    cap.set(3, 960)  # Width
    cap.set(4, 640)  # Height

    print("Camera setup complete.")
    return cap

# Function to capture a frame from the camera
def GetImage(cap):
    for i in range(5):  # Clear the buffer by reading a few frames
        ret, image = cap.read()  # 'ret' is a success flag; 'image' is the captured frame
    return image

# Function to display an image in a window
def ShowImage(img):
    cv2.imshow('image', img)  # Show the image in a new window

# Function to load a YOLOv11 model
def LoadModel(model_path="yolo11n.pt"):
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


# Main execution
#ser = ComunictionSetup()  # Set up serial communication


model = LoadModel()  # Load the YOLOv11 model
# Example usage
cap = CameraSetup()       # Initialize the camera

while True:
    image = GetImage(cap)        # Capture a frame
    # Run detection on the input image
    result = GetResult(model, image)
    if ObtainData(result) != None:
        x, y, w, h = ObtainData(result)  # Extract bounding box data

        # Calculate relative position from the center of the image
        relative_x = x - 480  # Calculate relative x position from center
        relative_y_bottom = 690 - (y + round(h/2)) # Calculate relative y position from bottom center
        print(f"Relative Position: x={relative_x}, y_bottom={relative_y_bottom}")  # Print relative position
        y_cm = 0.07895052 * np.e**(0.02147171 * relative_y_bottom) + 35.468015

        # Convert pixel offset to cm offset    
        x_cm = px_to_cm_x_offset(relative_x, y_cm)
        print(f"Calculated x position in cm: {x_cm}")
        print(f"Calculated y position in cm: {y_cm}")
        
        # Draw bounding box and center point on the image
        cv2.circle(image, (x, y + round(h/2)), 5, (255, 0, 255), -1)  # Draw center point
        cv2.rectangle(image, (x - round(w/2), y - round(h/2)), (x + round(w/2), y + round(h/2)), (0, 255, 0), 2)  # Draw bounding box
        cv2.line(image, (x, 0), (x, 640), (0, 0, 255), 1)  # Draw vertical line
        cv2.line(image, (0, y + round(h/2)), (960, y + round(h/2)), (0, 0, 255), 1)  # Draw horizontal line
        cv2.line(image, (480, 0), (480, 640), (255, 0, 0), 1)  # Draw center vertical line
        cv2.line(image, (0, 320), (960, 320), (255, 0, 0), 1)  # Draw center horizontal line


        # Display x_cm and y_cm in the bottom right corner
        text = f"X distance: {x_cm:.1f}, Y distance: {y_cm:.1f}"
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 0.7
        thickness = 2
        text_size, _ = cv2.getTextSize(text, font, font_scale, thickness)
        text_x = image.shape[1] - text_size[0] - 50
        text_y = image.shape[0] - 50
        cv2.putText(image, text, (text_x, text_y), font, font_scale, (0,255,255), thickness)
    
    
    
    if ObtainData(result) == None:
        print("No teddy bear detected.")
    # Print bounding box data
    ShowImage(image)# Display the frame
    if cv2.waitKey(1) & 0xFF == ord('q'):  # Exit loop if 'q' is pressed
        break
cap.release()  # Release the camera
cv2.destroyAllWindows()  # Close all OpenCV windows