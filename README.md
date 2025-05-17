# DigisparkTorch
LED effect lamp powered by Digispark, 8 RGB LEDs and the [FastLED](https://github.com/FastLED/FastLED) library.

## Features
- single button control
- store settings (EEPROM)
- timeout after 4 hours
- Fire2012WithPalette (FastLED) version optimized for 8 LEDs

## Hardware
- Digispark (ATtiny85) running at 16 MHz with the [ATTinyCore](https://github.com/SpenceKonde/ATTinyCore) (No Bootloader)
- 8 SK6812 RGB LEDs
- button mod (details below) with debouncing in software

## Gallery

<a href="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/DigisparkTorch_01.jpg">
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/DigisparkTorch_01.jpg" width=275>
</a>
<a href="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/DigisparkTorch_02.jpg">
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/DigisparkTorch_02.jpg" width=450>
</a>
<a href="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/led_strip.jpg">
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/led_strip.jpg" width=180>
</a>

Youtube video:<br>
[![DigisparkTorch Youtube](https://img.youtube.com/vi/WWqercYuQ-k/0.jpg)](https://www.youtube.com/watch?v=WWqercYuQ-k)

## Button controls
- normal press: cycle to next pattern:
  - rainbow
  - rainbow -> hold color
  - rainbowWithGlitter
  - rainrowWithGlitter -> hold color
  - Fire2012withPalette 
- double quick press: save settings to EEPROM
- long press: change brightness

## Digispark button mod
Two unmodified buttons on the left. Prepped right button has two legs removed:
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/button_prepping_01.jpg" width=600>

Slightly bend one or both legs so that they are near the GND and P0 pins. Use some glue (super glue, E6000, T-7000 etc.) on the button to fixate it during soldering (and also making the bond stronger over all). Letting the glue set for a few minutes before positioning it makes the soldering process much easier.

<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/button_prepping_02.jpg" width=400>

Afterwards:<br>
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/button_soldered.jpg" width=400>

## Programming
ISP programming setup with Atmel-ICE:
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/ISP_progamming_Atmel-ICE.jpg">

Using the 'No Bootloader' variants of [ATTinyCore](https://github.com/SpenceKonde/ATTinyCore) removes the 5 second boot delay that the Digisparks normally have and also saves some flash memory. It is recommended to turn on B.O.D. for better integrity of the stored settings in EEPROM:
<img src="https://raw.githubusercontent.com/chocotov1/DigisparkTorch/master/media/ATTinyCore_Burn_Bootloader_16MHz_No_Bootloader.png">

## Memory usage
<pre>
Sketch uses 6142 bytes (74%) of program storage space. Maximum is 8192 bytes.
Global variables use 248 bytes (48%) of dynamic memory, leaving 264 bytes for local variables. Maximum is 512 bytes.
</pre>

## Power consumption 
At max brightness i've seen around 0.6 and 0.7 W (120 mA and 140 mA at 5 V).
