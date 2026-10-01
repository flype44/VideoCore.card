#!/bin/bash
rm -rf /tmp/mut && mkdir /tmp/mut && cd /mnt/c/Developers/VideoCore.card && cp -r src include unicam.resource /tmp/mut/ 2>/dev/null
sed -i '0,/VC6_SCALER_POS2_ALPHA(0xfff)/s//VC6_SCALER_POS2_ALPHA(0xffe)/' /tmp/mut/src/vc6.c
grep -c 'ALPHA(0xffe)' /tmp/mut/src/vc6.c
bash /mnt/c/Developers/VideoCore.card/tests/host/check.sh /tmp/mut 6
