
#include <FastLED.h>

/////
///// CONSTANTS AND DEFINITIONS
/////

//// define I/O pins
#define PIN_BUTTON_INT 0
#define PIN_LED  13
#define PIN_REED 27

//// Initialize FastLED Library with 61 WS2811 pixels
// Channels:
// deck-right: 0-27
// back: 28-29
// deck-left: 30-57
// front: 58-60
#define BRIGHTNESS          255     // 0-255 - 50 max recommended if powered from computer to prevent voltage drops while flashing
#define NUM_PIXELS          61      // total pixel count
#define LED_TYPE            WS2811  // type of LED Stripe used
#define COLOR_ORDER         GRB     // color order of the used LED Stripe
CRGB leds[NUM_PIXELS];

//// Definition and addressing of LED pixel sections
// section numbers
#define SECTION_DECK              0 // individual pixels going round
#define SECTION_FRONT             1
#define SECTION_BACK              2
#define SECTION_DECKPAR           3 // 2 parallel pixels front-to-back
#define SECTION_ALL               4
// section start addresses
#define SECTION_DECKRIGHT_START   0
#define SECTION_DECKLEFT_START    30
#define SECTION_BACK_START        28
#define SECTION_FRONT_START       58
// pixel count per section
#define SECTION_DECKRIGHT_PIXELS  28
#define SECTION_DECKLEFT_PIXELS   28
int SECTION_DECK_PIXELS = SECTION_DECKRIGHT_PIXELS + SECTION_DECKLEFT_PIXELS;
#define SECTION_FRONT_PIXELS      3
#define SECTION_BACK_PIXELS       2

// Stuff for interrupts and buttons
long debouncing_time = 15; //Debouncing Time in Milliseconds
volatile unsigned long last_millis;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

struct RotationSensor {
  const uint8_t PIN;
  uint32_t numberRotations;
  double frequency;
  long lastChangeMillis;
};

Button button0 = {PIN_BUTTON_INT, 0, false};
RotationSensor reed0 = {PIN_REED, 0, 0.0};

// speed detection
unsigned long rotations = 0;  // roation count since program start
double frequency = 0.5; // rotation frequency in hz
unsigned long rotationsLast = 0;
double frequencyLast = 0.0;
double millisLast = 0;
long reedMicros = 0;

// general program flow
int lightingProgramID = 0;

/////
///// INTERRUPT ROUTINES
/////

void IRAM_ATTR button0Pressed() {
  button0.numberKeyPresses += 1;
  button0.pressed = true;
}

void IRAM_ATTR reed0Pressed() {
  long current_millis = millis() -  reed0.lastChangeMillis;
  if(current_millis >= debouncing_time) {
    reed0.numberRotations += 1;
    reed0.frequency = (double)1000.0/(current_millis);  
    reed0.lastChangeMillis = millis();
  }
}

/////
///// SETUP FUNCTION
/////

void setup()
{
delay(2000);
Serial.begin(9600);
Serial.println("\nstarting bootup...");
pinMode(PIN_BUTTON_INT, INPUT_PULLUP);
pinMode(PIN_REED, INPUT_PULLUP);
reed0.frequency = 0.00001;
attachInterrupt(PIN_BUTTON_INT, button0Pressed, FALLING);
attachInterrupt(PIN_REED, reed0Pressed, RISING);
FastLED.addLeds<LED_TYPE, PIN_LED, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection( TypicalLEDStrip );
FastLED.setBrightness(BRIGHTNESS);
Serial.println("system booted.");
}

/////
///// MAIN LOOP
/////

void loop() {

  // read out onboard boot button
  if (button0.pressed) {
    lightingProgramID = (lightingProgramID+1)%2;
    button0.pressed = false;
  }

  // lighting program 1
  if (lightingProgramID == 0) {
    // driving scene
    if (millis()-reed0.lastChangeMillis <= 1500) {
      // back static color
      colorSection(SECTION_BACK,100,0,0);
      // front static color
      colorSection(SECTION_FRONT,0,30,3);
      // deck cycle (speed sensitive)
      effectCycleSpeed(SECTION_DECKPAR,0,100,20);
    }
    // standing scene
    else {
      // back static color
      //colorSection(SECTION_BACK,70,0,0);
      // deck pulse
      //effectPulse(SECTION_DECK,0,100,20,9);
      // front static
      //colorSection(SECTION_FRONT,0,30,3);
      // all pulse
     effectPulse(SECTION_ALL,0,100,20,9);
    } 
  }

  // lighting program 2
  else if (lightingProgramID == 1) {    
    // back static color
    colorSection(SECTION_BACK,70,0,0);
    // deck static color
    //colorSection(SECTION_DECK,0,100,20);
    // deck pulse
    effectPulse(SECTION_DECK,0,100,20,9);
    // front pingpong
    //effectPingPong(SECTION_FRONT,0,30,3,150);
    // front static
    colorSection(SECTION_FRONT,0,30,3);
  }

  //delay(1);
}

/////
///// PIXEL FUNCTIONS
/////
///// These functions provide an interface to the physical LED pixels.
///// They do not fire FastLED.show() on their own!
/////

// Pixel Color: Sets a pixel (with channel ch) in a section
// section argument takes these values: 0=deck, 1=front, 2=back
// Deck Pixels: 56 channels (0 - 55)
// Front Pixels: 3 channels (0 - 2)
// Back Pixels: 2 channels (0 - 1)
// Deck Parallel Pixels: 28 channels (0 - 27)
void colorPixel(int section, int ch, int r, int g, int b) {
  // deck pixels (going round)
  if (section == SECTION_DECK) {
    // map deck pixels on both sides to sequential addresses
    if ( ch >= 0 && ch < (SECTION_DECK_PIXELS/2)) {
      leds[ch].setRGB(r,g,b);
    }
    else if (ch >= 0 && ch < SECTION_DECK_PIXELS) {
      leds[ch+2].setRGB(r,g,b);
    }
  }
  // front pixels
  else if (section == SECTION_FRONT) {
    if ( ch >= 0 && ch < SECTION_FRONT_PIXELS) {
      leds[ch+58].setRGB(r,g,b);
    }
  }
  // back pixels
  else if (section == SECTION_BACK) {
    if ( ch >= 0 && ch < SECTION_BACK_PIXELS) {
      leds[ch+28].setRGB(r,g,b);
    }
  }
  // deck pixels (2 parallel, front-to-back)
  else if (section == SECTION_DECKPAR) {
    if ( ch >= 0 && ch < SECTION_DECKRIGHT_PIXELS) {
      leds[ch].setRGB(r,g,b);
      leds[57-ch].setRGB(r,g,b);
    }
  }
  else if (section == SECTION_ALL) {
    if ( ch >= 0 && ch < NUM_PIXELS) {
      leds[ch].setRGB(r,g,b);
    }
  }
}

// Section Color: Sets all pixels in a section
// section argument takes these values: 0=deck, 1=front, 2=back, 3=deck_parallel
void colorSection(int section, int r, int g, int b) {
  int n = 0;
  if (section == SECTION_DECK) {
    n = SECTION_DECK_PIXELS;
  }
  else if (section == SECTION_FRONT) {
    n = SECTION_FRONT_PIXELS;
  }
  else if (section == SECTION_BACK) {
    n = SECTION_BACK_PIXELS;
  }
  else if (section == SECTION_DECKPAR) {
    n = SECTION_DECKRIGHT_PIXELS;
  }
  else if (section == SECTION_ALL) {
    n = NUM_PIXELS;
  }
  for (int i = 0; i < n; i++) {
    colorPixel(section,i,r,g,b);
  }
}

/////
///// EFFECT FUNCTIONS
/////
///// These functions implement lighting effects.
///// They fire FastLED.show()!
/////

// Strobe: Blinks all pixels in a section
// section argument takes these values: 0=deck, 1=front, 2=back
// time argument controls time in milliseconds between steps.
void effectStrobe (int section, int r, int g, int b, int time) {
  for (int i=0; i<2; i++) {
    colorSection(section,r,g,b);
    FastLED.show();
    colorSection(section,0,0,0);
    delay(time);   
  }
}

// Pulse: Pixels in a section dim up and down
// section argument takes these values: 0=deck, 1=front, 2=back
// time argument controls time in milliseconds between steps (with 511 steps per cycle):
// a time of 2ms is around 1s cycle time
void effectPulse(int section, int r, int g, int b, int time) {
  int red = 0;
  int green = 0;
  int blue = 0;
  for (int i=0; i<=510; i++) {
    if (i<=255) {
      if (r > 0) {
       // red = ((double)i/(double)r)*r;
        red = ((double)i/255.0)*(double)r;
      }
      else {
        red = 0;  
      }
      if (g > 0) {
        green = ((double)i/255.0)*(double)g;
      }
      else {
        green = 0;  
      }
      if (b > 0) {
        blue = ((double)i/255.0)*(double)b;
      }
      else {
        blue = 0;  
      }
    }
    else {
      red = (r*2) - ((double)i/255.0)*(double)r;
      green = (g*2) - ((double)i/255.0)*(double)g;
      blue = (b*2) - ((double)i/255.0)*(double)b; 
    }
    colorSection(section,red,green,blue);
    FastLED.show();
    delay(time);
  }
}

// Cycle: Lights up all individual pixels in a section sequentially (cycles through all addresses)
// section argument takes these values: 0=deck, 1=front, 2=back
// time argument controls time in milliseconds between steps
void effectCycle(int section, int r, int g, int b, int time) {
  int n = 0;
  if (section == SECTION_DECK) {
    n = SECTION_DECK_PIXELS;
  }
  else if (section == SECTION_FRONT) {
    n = SECTION_FRONT_PIXELS;
  }
  else if (section == SECTION_BACK) {
    n = SECTION_BACK_PIXELS;
  }
  else if (section == SECTION_DECKPAR) {
    n = SECTION_DECKRIGHT_PIXELS;
  }
  for (int i=0; i<n; i++) {
    colorPixel(section,i,r,g,b);
    FastLED.show();
    colorPixel(section,i,0,0,0);
    delay(time);
  }  
}

// Speed sensitive cycle effect
void effectCycleSpeed(int section, int r, int g, int b) {
  int n = 0;
  if (section == SECTION_DECK) {
    n = SECTION_DECK_PIXELS;
  }
  else if (section == SECTION_FRONT) {
    n = SECTION_FRONT_PIXELS;
  }
  else if (section == SECTION_BACK) {
    n = SECTION_BACK_PIXELS;
  }
  else if (section == SECTION_DECKPAR) {
    n = SECTION_DECKRIGHT_PIXELS;
  }
  for (int i=0; i<n; i++) {
    colorPixel(section,i,r,g,b);
    FastLED.show();
    colorPixel(section,i,0,0,0);
    //int time = 60-(6*reed0.frequency);
    int time = (double)60.0/(reed0.frequency);
    if (time > 200) {
      time = 200;
    }
    delay(time);
  }  
}

// PingPong: Lights up all individual pixels in a section sequentially (walking the address range up  and down)
// section argument takes these values: 0=deck, 1=front, 2=back
// time argument controls time in milliseconds between steps
void effectPingPong(int section, int r, int g, int b, int time) {
  int n = 0;
  int currentPixel = 0;
  if (section == SECTION_DECK) {
    n = SECTION_DECK_PIXELS;
  }
  else if (section == SECTION_FRONT) {
    n = SECTION_FRONT_PIXELS;
  }
  else if (section == SECTION_BACK) {
    n = SECTION_BACK_PIXELS;
  }
  else if (section == SECTION_DECKPAR) {
    n = SECTION_DECKRIGHT_PIXELS;
  }
  for (int i=0; i<(n*2)-2; i++) {
    if (i<=n-1) {
      currentPixel = i;
    }
    else {
      currentPixel = (n*2)-2-i;
    }
    colorPixel(section,currentPixel,r,g,b);
    FastLED.show();
    colorPixel(section,currentPixel,0,0,0);  
    delay(time);
  }  
}
