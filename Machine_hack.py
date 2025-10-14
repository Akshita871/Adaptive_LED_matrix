import cv2
import matplotlib.pyplot as plt
import cvlib as cv
import urllib.request
import numpy as np
from cvlib.object_detection import draw_bbox
import concurrent.futures

url = 'http://192.168.10.162/cam-hi.jpg'

im = None

def run1():
    cv2.namedWindow("live transmission", cv2.WINDOW_AUTOSIZE)
    while True:
        img_resp = urllib.request.urlopen(url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)
        cv2.imshow('live transmission', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break
    cv2.destroyAllWindows()

def run2():
    cv2.namedWindow("detection", cv2.WINDOW_AUTOSIZE)
    while True:
        img_resp = urllib.request.urlopen(url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        # Detect common objects using yolov3-tiny model
        bbox, label, conf = cv.detect_common_objects(im, model='yolov3-tiny')

        # Filter only cars
        car_bbox, car_label, car_conf = [], [], []
        for i in range(len(label)):
            if label[i] == 'car':
                car_bbox.append(bbox[i])
                car_label.append(label[i])
                car_conf.append(conf[i])

        # Draw bounding boxes for cars
        im = draw_bbox(im, car_bbox, car_label, car_conf)

        cv2.imshow('detection', im)
        key = cv2.waitKey(5)
        if key == ord('q'):
            break
    cv2.destroyAllWindows()

if _name_ == "_main_":  # ✅ fixed here
    print("started")
    with concurrent.futures.ProcessPoolExecutor() as executor:
        f1 = executor.submit(run1)
        f2 = executor.submit(run2)