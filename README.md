# uv-exposure
  UV exposure box controller for making PCB's @ home
  working on ARDUINO MEGA2560 + LCD 20x4 (2004)
  inspired by this: https://www.youtube.com/watch?v=ZAlxNNI-BVM
  ... but different principle of operation:
  - minimalistic but functional UI on 4 rows 20 characters LCD
  - only one command (push) button (no rotary encoder or microswitches)
  - two indipendent timed channels, combine as you wish like for eg.:
    use 1st. channel for UV-LED strip for UV exposure of PCB
    use 2nd. channel for regular white LED strip and PCB/film inspection
    you got the idea, combine as you wish eg. 2 channels both for UV-LED
  - use two relays for powering both channels - or use a MOSFET switch as
    did the author from the YT video bellow ...
  - take care of LED/PSU power to not exceed combined power of Arduino + 
    accessories + combined LED strips, i'm using 12V / 5A LED driver       
  - this is work in progress!

 (c)2019 by cybercow222

  v 0.1a
