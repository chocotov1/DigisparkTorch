// https://github.com/FastLED/FastLED/issues/413 Making FastLED 3.1.3 work with ATTinyCore
#define ATTINY_CORE 1

#include <FastLED.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
//#include <avr/wdt.h>

#define DATA_PIN    PIN_PB3
//#define CLK_PIN   4
#define LED_TYPE    SK6812
#define COLOR_ORDER GRB
//#define COLOR_ORDER RGB
#define NUM_LEDS    8
CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGBPalette16 gPal; // Fire2012WithPalette

// turn off RGB and on-board leds and put MCU to sleep after defined time
// note that the RGB leds typically still draw power even when turned "off" and are set to black
#define SLEEP_TIMEOUT_SECONDS 14400 // approximately 4 hours

//#define LED_BUILTIN_DEFAULT_STATE 0 // on-board led normally turned off
#define LED_BUILTIN_DEFAULT_STATE 1   // use more power with on-board led turned on: can help to reach the low power cutoff of many powerbanks

#define BRIGHTNESS          70
#define MIN_BRIGHTNESS      25
#define MAX_BRIGHTNESS     255

#define FRAMES_PER_SECOND  120

#define BUTTON_PRESSED_SHORT  1
#define BUTTON_PRESSED_DOUBLE 2
#define BUTTON_PRESSED_LONG   3

#define PATTERN_RAINBOW                0
#define PATTERN_RAINBOW_PAUSED         1
#define PATTERN_RAINBOW_GLITTER        2
#define PATTERN_RAINBOW_GLITTER_PAUSED 3
#define PATTERN_TORCH                  4
#define LAST_PATTERN                   PATTERN_TORCH
uint8_t current_pattern = 0;
bool cycle_rainbow_hue_active = 1;

volatile byte button_pressed_state = 0;
byte button_pin = PIN_PB0; // PCINT0 -> Pin Change Interrupt

uint32_t sleep_time_millis = 0;

bool change_brightness_active    = 0;
bool brightness_change_direction = 0;

void setup(){  
   // 9.3.2 GIMSK – General Interrupt Mask Register
   GIMSK = 1<<PCIE; // Pin Change Interrupt Enable

   // 9.3.4 PCMSK – Pin Change Mask Register
   PCMSK = 1<<button_pin;
   
   // tell FastLED about the LED strip configuration
   FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip).setRgbw();

   // set master brightness control
   FastLED.setBrightness(BRIGHTNESS);
   gPal = HeatColors_p; // Fire2012WithPalette

   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(button_pin, INPUT_PULLUP);

   digitalWrite(LED_BUILTIN, LED_BUILTIN_DEFAULT_STATE);

   set_sleep_mode(SLEEP_MODE_PWR_DOWN);
   set_sleep_timeout(SLEEP_TIMEOUT_SECONDS);
}

// button state handling detects short, double or long press
// perposfully blocks to ignore button bounce phase
ISR (PCINT0_vect){
  handle_button_press_blocking();

  // 9.3.3 GIFR – General Interrupt Flag Register
  GIFR &= 1<<PCIF; // clear pin change interrupt flag

  if (button_pressed_state){
     // disable pin change interrupt, reenable after processing button_pressed_state in loop
     GIMSK &= ~(1<<PCIE);
  }
}

void set_sleep_timeout(uint16_t timeout_seconds){
   sleep_time_millis = millis() + timeout_seconds * 1000UL;
}

void check_sleep_timeout(){
   if (button_pressed_state || !button_state()){
      // don't sleep when there's button activity
      return;
   }
     
   if (millis() >= sleep_time_millis){
      turn_off_all_leds();
      go_to_sleep();

      // wake up invoked by button press: ignore button_pressed_state that was evaluated in handle_button_press_blocking()
      // handle_button_press_blocking() also disables pin change interrupt, enable again:
      reset_button_pressed_state();

      digitalWrite(LED_BUILTIN, LED_BUILTIN_DEFAULT_STATE);

      // set new timeout after waking up:
      set_sleep_timeout(SLEEP_TIMEOUT_SECONDS);
   }
}

void turn_off_all_leds(){
   digitalWrite(LED_BUILTIN, LOW);
   fadeToBlackBy(leds, NUM_LEDS, 255);
   FastLED.show();
}

void go_to_sleep(){
   ADCSRA &= ~(1<<ADEN); // turn off ADC
   power_all_disable();  // power off ADC, Timer 0 and 1, serial interface

   GIMSK = 1<<PCIE; // pin change interrupt must be enabled for wakeup
   sleep_mode();    // sleep

   // watchdog not used at the moment
   //
   // from the ATTinyCore docs:
   // When using the WDT as a reset source and NOT using a bootloader remember that after reset the WDT will be enabled with minimum timeout.
   // The very first thing your application must do upon restart is reset the WDT (wdt_reset()),  clear WDRF flag in MCUSR (MCUSR&=~(1<<WDRF))
   // and then turn off or configure the WDT for your desired settings. If using the Optiboot bootloader, this is already done for you by the bootloader.
   //
   //wdt_reset();
   //MCUSR&=~(1<<WDRF);
   //wdt_disable(); 

   // awake again
   power_all_enable(); // power everything back on
}

void handle_button_press_blocking(){
   // debounce delay
   _delay_ms(5);

   // only interested when initial state is low: button pressed
   if (button_state()){
      return;
   }

   if (button_down_time(250) >= 250){
      button_pressed_state = BUTTON_PRESSED_LONG;
   } else {
      if (button_up_time(175) < 175){
         button_pressed_state = BUTTON_PRESSED_DOUBLE;
      } else {
         button_pressed_state = BUTTON_PRESSED_SHORT;
      }
   }
}

uint16_t button_down_time(uint16_t button_timeout_ms){
   for (uint16_t ms = 0; ms <= button_timeout_ms; ms++){
      _delay_ms(1);

      if (button_state()){
         return ms;
      }
   }

   return button_timeout_ms;
}

uint16_t button_up_time(uint16_t button_timeout_ms){
   for (uint16_t ms = 0; ms <= button_timeout_ms; ms++){
      _delay_ms(1);

      if (!button_state()){
         return ms;
      }
   }

   return button_timeout_ms;
}

bool button_state(){
   return PINB & 1<<button_pin;
}

void handle_button_pressed_state(){
   if (button_pressed_state == BUTTON_PRESSED_SHORT){
      blink_led_once(50);
      cycle_next_pattern();
   } else if (button_pressed_state == BUTTON_PRESSED_DOUBLE){
      blink_led_multiple(200, 2);
   } else if (button_pressed_state == BUTTON_PRESSED_LONG){
      // toggle brightness_change_direction
      brightness_change_direction = !brightness_change_direction;
      change_brightness_active = 1;
      blink_led_once(100);
   }

   reset_button_pressed_state();
}

void reset_button_pressed_state(){
   // 9.3.2 GIMSK – General Interrupt Mask Register
   GIMSK = 1<<PCIE; // Pin Change Interrupt Enable
   
   button_pressed_state = 0;
}

void blink_led_multiple (uint16_t blink_duration_ms, uint8_t blink_count){
   for (uint8_t i = 0; i < blink_count ; i++){
      if (i > 0){
         // subsequent blinks need leading delay to be visible
         delay(blink_duration_ms);
      }

      blink_led_once(blink_duration_ms);      
   }
}

void blink_led_once(uint16_t blink_duration_ms){
      digitalWrite(LED_BUILTIN, !LED_BUILTIN_DEFAULT_STATE);
      delay(blink_duration_ms);
      digitalWrite(LED_BUILTIN, LED_BUILTIN_DEFAULT_STATE);
}

void paint_current_pattern(){
   if (current_pattern == PATTERN_RAINBOW || current_pattern == PATTERN_RAINBOW_PAUSED){
      rainbow();
   } else if (current_pattern == PATTERN_RAINBOW_GLITTER || current_pattern == PATTERN_RAINBOW_GLITTER_PAUSED){
      rainbowWithGlitter();
   } else if (current_pattern == PATTERN_TORCH){
      // Add entropy to random number generator; we use a lot of it.
      random16_add_entropy( random());

      // Fourth, the most sophisticated: this one sets up a new palette every
      // time through the loop, based on a hue that changes every time.
      // The palette is a gradient from black, to a dark color based on the hue,
      // to a light color based on the hue, to white.
      //
      //   static uint8_t hue = 0;
      //   hue++;
      //   CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
      //   CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
      //   gPal = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);


      Fire2012WithPalette(); // run simulation frame, using palette colors 
   } 
}

void cycle_next_pattern(){
   current_pattern++;
   if (current_pattern > LAST_PATTERN){
      current_pattern = 0;
   }

   setup_pattern(current_pattern);
}

void setup_pattern(uint8_t pattern){
   // turn off cycling of rainbow hue for the paused rainbow patterns
   if (pattern == PATTERN_RAINBOW){
      cycle_rainbow_hue_active = 1;
   } else if (pattern == PATTERN_RAINBOW_PAUSED){
      cycle_rainbow_hue_active = 0;
   } else if (pattern == PATTERN_RAINBOW_GLITTER){
      cycle_rainbow_hue_active = 1;
   } else if (pattern == PATTERN_RAINBOW_GLITTER_PAUSED){
      cycle_rainbow_hue_active = 0;
   }
}

void rainbow(){
   // FastLED's built-in rainbow generator
   fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter(){
   // built-in FastLED rainbow, plus some random sparkly glitter
   rainbow();
   addGlitter(5);
}

void addGlitter(fract8 chanceOfGlitter){
   if (random8() < chanceOfGlitter){
      leds[ random16(NUM_LEDS) ] += CRGB::White;
   }
}

void cycle_rainbow_hue(){
   if (cycle_rainbow_hue_active){
      gHue++;
   }
}

void change_brightness(){   
   if (change_brightness_active){
      if (!button_state()){        
         // button still pressed

         // new_brightness initially copy of current brightness
         int16_t new_brightness = FastLED.getBrightness();

         int8_t brightness_change_rate = 1;

         if (new_brightness >= 80){
            brightness_change_rate = map(new_brightness, 80, 255, 1, 10);
         }

         if (!brightness_change_direction){
            brightness_change_rate *= -1; // invert 
         }

         new_brightness += brightness_change_rate;
         new_brightness = constrain(new_brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

         if (new_brightness != FastLED.getBrightness()){
            blink_led_once(20);
            FastLED.setBrightness(new_brightness);
         }
      } else {
         // button no longer pressed
         change_brightness_active = 0;
      }
   }
}


// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100 
//#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 15


void Fire2012WithPalette()
{
// Array of temperature readings at each simulation cell
  static uint8_t heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, 3));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(4);
      heat[y] = qadd8( heat[y], random8(160,255) );
      //heat[y] = qadd8( heat[y], random8(160,220) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      uint8_t colorindex = scale8( heat[j], 100);


      CRGB color = ColorFromPalette( gPal, colorindex);
      leds[j] = color;
    }
}

void loop(){
   // do some periodic updates
   EVERY_N_MILLISECONDS(40){
      cycle_rainbow_hue(); // slowly cycle the "base color" through the rainbow
      change_brightness();
   }

   check_sleep_timeout();

   if (button_pressed_state){
      handle_button_pressed_state();
   }

   paint_current_pattern();
    
   // send the 'leds' array out to the actual LED strip
   FastLED.show();
   // insert a delay to keep the framerate modest
   FastLED.delay(1000/FRAMES_PER_SECOND);
}
