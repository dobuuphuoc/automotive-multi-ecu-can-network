#!/usr/bin/env python3
"""Benchmark MediaPipe Face Mesh on Jetson (or any PC).

Usage:
  python3 bench_facemesh.py --video face.mp4          # video file (loops), no camera needed
  python3 bench_facemesh.py --camera 0                # any USB webcam
Optional: --seconds 60  --width 640  --height 480

While it runs, open a second terminal and run `tegrastats`.
If GR3D_FREQ (GPU load) stays near 0% while CPU cores are busy, inference runs on CPU.
"""
import argparse
import time

import cv2
import mediapipe as mp

if not hasattr(mp, "solutions"):
    raise SystemExit(
        "This mediapipe version has no mp.solutions (removed in newer releases).\n"
        "On Jetson (Python 3.10) use:  pip install 'mediapipe==0.10.14' 'numpy<2'")

ap = argparse.ArgumentParser()
src = ap.add_mutually_exclusive_group(required=True)
src.add_argument("--video", help="path to a video file containing your face")
src.add_argument("--camera", type=int, help="camera index, e.g. 0")
ap.add_argument("--seconds", type=float, default=60.0)
ap.add_argument("--width", type=int, default=640)
ap.add_argument("--height", type=int, default=480)
args = ap.parse_args()

cap = cv2.VideoCapture(args.video if args.video else args.camera)
if args.camera is not None:
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
if not cap.isOpened():
    raise SystemExit("Cannot open video source")

face = mp.solutions.face_mesh.FaceMesh(
    max_num_faces=1, refine_landmarks=True,
    min_detection_confidence=0.5, min_tracking_confidence=0.5)

frames = detected = 0
lat_ms = []
size = None
t0 = time.perf_counter()
while time.perf_counter() - t0 < args.seconds:
    ok, frame = cap.read()
    if not ok:
        if args.video and frames > 0:            # loop the video file
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue
        break
    size = frame.shape[1], frame.shape[0]
    t1 = time.perf_counter()
    res = face.process(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
    lat_ms.append((time.perf_counter() - t1) * 1000.0)
    frames += 1
    detected += 1 if res.multi_face_landmarks else 0
elapsed = time.perf_counter() - t0

if not frames:
    raise SystemExit("No frames processed")
lat_ms.sort()
pct = lambda p: lat_ms[min(len(lat_ms) - 1, int(p * len(lat_ms)))]
rate = 100.0 * detected / frames
print(f"mediapipe {mp.__version__}, frame size {size[0]}x{size[1]}")
print(f"frames: {frames} in {elapsed:.1f} s -> {frames / elapsed:.1f} FPS")
print(f"inference latency ms: p50 {pct(0.50):.1f}  p95 {pct(0.95):.1f}  max {lat_ms[-1]:.1f}")
print(f"face detected in {rate:.0f}% of frames")
if rate < 90:
    print("WARNING: face not found in most frames, FPS is not representative. "
          "Use a video/camera where your face is clearly visible.")
