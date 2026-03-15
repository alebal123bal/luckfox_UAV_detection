---
name: analyze-uav-detections
description: Analyze UAV detections produced by the YOLO pipeline.
homepage: https://github.com/alebalzan/luckfox_UAV_detection
metadata: {"nanobot":{"emoji":"🛸","requires":{"bins":["/root/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections"]}}}
---

# Analyze UAV Detections

Analyze UAV detections produced by the YOLO pipeline.

## Steps

1. Run the detection analysis program:

```bash
/root/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections
```

2. Interpret the output.

3. Report UAV trajectory, speed, and approach direction.

## Output Format

Provide a concise analysis of UAV movement.
