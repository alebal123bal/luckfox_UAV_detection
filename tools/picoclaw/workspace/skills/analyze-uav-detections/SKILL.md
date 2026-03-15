---
name: analyze-uav-detections
description: Analyze UAV detections produced by the YOLO pipeline.
homepage: https://github.com/alebalzan/luckfox_UAV_detection
metadata: {"nanobot":{"emoji":"🛸","requires":{"bins":["/root/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections"]}}}
---

# Analyze UAV Detections

Analyze UAV detections produced by the YOLO pipeline.

## Steps

1. Dump the motion-feature JSON to a temporary file:

```bash
/root/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections -j /tmp/detections.json
```

2. Read the file:

```bash
cat /tmp/detections.json
```

3. Analyze the JSON array. Each object is one decimated time window for one target:

```
timestamp_us     — end-of-window timestamp (microseconds since epoch)
trajectory.centroid  — [nx, ny] normalized position in frame [0..1]
trajectory.velocity  — [vx, vy] smoothed velocity in pixels/second
trajectory.speed     — smoothed scalar speed in pixels/second
trajectory.heading_rad — direction of motion in radians (atan2)
trajectory.samples   — number of raw detections averaged in this window
confidence       — mean detection confidence in this window
target_id        — YOLO target number
```

4. Report:
   - Current position and movement direction of each target
   - Speed trend (accelerating / decelerating / steady)
   - Approach angle relative to frame centre
   - Any targets that disappeared or reappeared

## Notes

- The file `/tmp/detections.json` is on the device's RAM tmpfs; it is lost on reboot.
- Detection logs are stored persistently under `/userdata/uav_detections/`.
- If no detections are found, check that the main pipeline binary is running:
  ```bash
  ps | grep luckfox_pico
  ```
- To get raw bounding boxes instead of motion features, use `--tail 20` for a table
  or `-c /tmp/export.csv` for full CSV.
