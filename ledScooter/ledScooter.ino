
#include <FastLED.h>

/////
///// CONSTANTS AND DEFINITIONS
/////

//// define I/O pinsc
#define PIN_BUTTON_INT 0
#define PIN_LED  13
#define PIN_REED 27

//// Initialize FastLED Library with 61 WS2811 pixels
// Channels:
// deck-right: 0-27
// back: 28-29
// deck-left: 30-57
// front: 58-60
#define BRIGHTNESS          128     // 0-255 - 50 max recommended if powered from computer to prevent voltage drops while flashing
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

int lightingProgramID = 0;

/////
///// SETUP FUNCTION
/////

void setup()
{
delay(2000);
Serial.begin(9600);
Serial.println("\nstarting bootup...");
pinMode(PIN_BUTTON_INT, INPUT_PULLUP);
FastLED.addLeds<LED_TYPE, PIN_LED, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection( TypicalLEDStrip );
FastLED.setBrightness(BRIGHTNESS);
Serial.println("system booted.");
}

/////
///// MAIN LOOP
/////

void loop() {
  bool buttonPress = ! digitalRead(PIN_BUTTON_INT);

  if (buttonPress) {
    lightingProgramID = (lightingProgramID+1)%2;
  }

  if (lightingProgramID == 0) {
    // back static color
    colorSection(SECTION_BACK,200,0,0);
    // deck static color
    colorSection(SECTION_DECK,200,0,0);
    // front pingpong
    effectPingPong(SECTION_FRONT,0,50,0,150);
  }
  
  else if (lightingProgramID == 1) {
    // back static color
    colorSection(SECTION_BACK,200,0,0);
    // front static color
    colorSection(SECTION_FRONT,0,50,0);
    // deck pingpong (front-to-back)
    effectCycle(SECTION_DECKPAR,255,0,0,50);
  }
  
  // deck pulse
  //effectPulse(SECTION_DECK,255,0,0,10);

    
  delay(1);
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
  if (section == 0) {
    // map deck pixels on both sides to sequential addresses
    if ( ch >= 0 && ch < (SECTION_DECK_PIXELS/2)) {
      leds[ch].setRGB(r,g,b);
    }
    else if (ch >= 0 && ch < SECTION_DECK_PIXELS) {
      leds[ch+2].setRGB(r,g,b);
    }
  }
  // front pixels
  else if (section == 1) {
    if ( ch >= 0 && ch < SECTION_FRONT_PIXELS) {
      leds[ch+58].setRGB(r,g,b);
    }
  }
  // back pixels
  else if (section == 2) {
    if ( ch >= 0 && ch < SECTION_BACK_PIXELS) {
      leds[ch+28].setRGB(r,g,b);
    }
  }
  // deck pixels (2 parallel, front-to-back)
  else if (section == 3) {
    if ( ch >= 0 && ch < SECTION_DECKRIGHT_PIXELS) {
      leds[ch].setRGB(r,g,b);
      leds[57-ch].setRGB(r,g,b);
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
  for (int i = 0; i < n; i++) {
    colorPixel(section,i,r,g,b);
    // TODO: Remove this if if it has really proven as irrelevant
    //if (section == 3) {
    //  colorPixel(section,i*2,r,g,b);
    //}
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
