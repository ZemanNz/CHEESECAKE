from ultralytics import YOLO

# Načti svůj YOLOv11 model (např. best.pt z tréninku)
model = YOLO("yolo11s.pt")

# Export do ONNX
model.export(format="onnx")  # vytvoří "yolov11s.onnx"