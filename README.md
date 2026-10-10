This project is for the DCSWorld F18 Radar Altimeter. I've adapted the Open Hornet bezel from the Fusion 360 project to cover the display.
The rotary encoder for push to test and setting the Min Height is connected to a seperate Arduino Mega and its inputs are handled by it. 
The display being used is the Waveshare 1.85" LCD without tought screen.  https://www.waveshare.com/wiki/ESP32-S3-LCD-1.85

<B>Requirements:</b></br>

ESP Board:
  ESP32 by Expressif (tested with version 3.3.1)
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
<br>          
LVGL v8.3.11:  Install: Arduino IDE → Library Manager → search LVGL → install.
<br>

<b>Build and flash (arduino-cli):</b>
<pre>
FQBN: esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn &lt;FQBN&gt; --libraries &lt;your Arduino libraries folder&gt; .
arduino-cli upload  --fqbn &lt;FQBN&gt; -p COM6 .
</pre>
Libraries: LVGL 8.3.11 and DCS-BIOS. LVGL is configured by the <code>lv_conf.h</code> in this folder
(16-bit color with byte swap on, so the display driver does not swap bytes itself).
DCS-BIOS uses the same USB serial port as uploads, so close the Serial Monitor while running DCS.

<b>Options in F18-RadarAltimeter.ino:</b>
<ul>
<li><code>USE_GLASS_OVERLAY</code> (1/0): full-screen glass reflection and bezel shading. Looks better, costs the most CPU.</li>
<li><code>ROTATE_ANTIALIAS</code> (1/0): smooth rotation of the needle and min-height bug. 0 is faster but slightly jagged.</li>
<li><code>LV_USE_PERF_MONITOR</code> in <code>lv_conf.h</code>: set to 1 to show CPU % and FPS at the bottom of the display.</li>
</ul>

<b>Display layers (back to front):</b> OFF flag, dial, OFF window shade, lamps, min-height bug, needle,
mask plate (the needle slides under it between 50 and 0), fixed hub cap, glass overlay.
The needle and bug share one rotation center, pixel (179, 180). The hub cap is a separate fixed image
so it does not move with the needle.

<b>Startup demo:</b> on power-up the lamps flash, then the needle, min-height bug and OFF flag sweep their full range and back.

<b>Regenerating the bitmaps:</b> the image <code>.c</code> files are generated (RGB565 with ordered dither, 16-bit variants only).
The generator scripts and original artwork are not in this repo.

<p>
<img width="439" height="387" alt="2025-09-25_13-50-20_005" src="https://github.com/user-attachments/assets/badb715b-2910-4d1c-ae19-d63cd57acf52" />
<img width="439" height="328" alt="2025-09-08_12-24-30_979" src="https://github.com/user-attachments/assets/4a5dad07-bb8e-4ad3-9b9c-7a68b6c19235" />

<img width="439" height="341" alt="2025-09-08_12-24-21_899" src="https://github.com/user-attachments/assets/db582a9f-3307-4765-8ca2-f18508dfae25" />

I've included STL files I used to print a mounting base for the display and the Bezel.

Its important to set the IDE up for the LCD Display. I've included a screenshot of my settings.

<img width="621" height="999" alt="IDE Settings" src="https://github.com/user-attachments/assets/39be1d15-527a-47b7-9954-9c481a2c8f41" />
 
  
</p>

<p>
## License
This project is licensed under a [Custom Non-Commercial License](LICENSE).  
You can use and modify it for **personal and educational purposes only**.  
For commercial use, please contact me for permission.

</p>
