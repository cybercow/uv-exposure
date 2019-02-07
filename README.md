# uv-exposure
   inspired by this: https:www.youtube.com/watch?v=ZAlxNNI-BVM
   - minimalistic but functional UI on 4 rows 20 characters LCD
   - only one command (microswitch/push) button (no rotary encoder or multiple microswitches)
     use double clicks with continuous intuitive clicking for navigation
   - two independent - but single timed channels, combine as you wish like for eg.:  
     use 1st. channel for UV-LED strip for UV exposure of PCB
     use 2nd. channel for regular white LED strip and PCB/film inspection;  
     you got the idea ...
   - use two relays for powering both channels - or use a MOSFET switch as
     did the author from the YT video bellow ...
   - take care of LED/PSU power to not exceed combined power of Arduino + 
     accessories + combined LED strips, i'm using 12V / 5A LED driver       
   - this is work in progress!
   - why i did this ? well, my old scsi scanner had only one button :P, 
     then, i had Arduino Mega and 2004 LCD display laying around ...
     then, i wanted something just darned simple and practical to work my PCB's

  This should fit on Arduino Uno, but i didn't had any @ the moment so here you go
  with the Mega ... 

  I added a small (but smart) screen saver to save some power when not used for some time
  it will respectively turn on/off the LCD backlight ...

  I'm using 12V relays with 2 driver TUN transistors like BC547 or whatever ...

  Will post the schematic and upload YT video ...
  
  (c)2019 by cybercow222 / v0.1b
