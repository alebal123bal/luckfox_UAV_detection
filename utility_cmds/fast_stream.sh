gst-launch-1.0 rtspsrc location=rtsp://172.32.0.93/live/0 latency=0 ! \
    rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! ximagesink sync=false