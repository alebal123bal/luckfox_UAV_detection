bash /home/alebalzan/luckfox/luckfox_pico_rkmpi_example/build.sh

sshpass -p 'luckfox' scp -r \
"/home/alebalzan/luckfox/luckfox_pico_rkmpi_example/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo" \
"root@172.32.0.93:/root/"