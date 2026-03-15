# User

## Environment
- Host machine: Windows with WSL2
- Deploy tool: sshpass + scp from WSL
- LuckFox IP: 172.32.0.93
- SSH user: root / password: luckfox

## Preferences

## Project
UAV detection with YOLOv5 on RKNN NPU. RTSP output.
To analyze detections, dump motion features to a file then read it:
```bash
/root/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections -j /tmp/detections.json
cat /tmp/detections.json
```
