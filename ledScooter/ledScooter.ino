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
//// The scooter has four physical lighting sections (front, back, deck left, deck right)
//// They are mapped into 'logical' sections for convenient use in the code:
// section numbers
#define SECTION_DECK              0 // individual pixels going round - 56 pixels
#define SECTION_FRONT             1 // 3 pixels
#define SECTION_BACK              2 // 2 pixels
#define SECTION_DECKPAR           3 // 2 parallel pixels front-to-back - 28 channels
#define SECTION_ALL               4 // all pixels // 61 channels
// section physical start addresses
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

//// Stuff for interrupts and buttons
// Debouncing
long debouncing_time = 30; // debouncing Time in Milliseconds
// Struct for physical button
struct Button {
  const uint8_t PIN; // GPIO used
  uint32_t numberKeyPresses; // key press counter
  bool pressed; // set to true if button is pressed
  long lastChangeMillis; // timestamp of last button press
};
// Struct for reed sensor (rotation count)
struct RotationSensor {
  const uint8_t PIN; // GPIO used
  uint32_t numberRotations; // rotation rounter
  double frequency; // current rotation frequency
  long lastChangeMillis; // timestamp of last sensor trigger
};
// initialize data structures for buttons and sensors
Button button0 = {PIN_BUTTON_INT, 0, false, 0}; // onboard boot button
RotationSensor reed0 = {PIN_REED, 0, 0.00001, 0}; // reed contact for speed detection

//// general variables
int lightingProgramID = 0;  // ID of the currently running program / scene
bool brake = false;
int brakeCounter = 0;

/////
///// INTERRUPT ROUTINES
/////

// button0 interrupt handler
void IRAM_ATTR button0Pressed() {
  if ((millis() - button0.lastChangeMillis) >= debouncing_time) {
    button0.numberKeyPresses += 1;
    button0.pressed = true;
    button0.lastChangeMillis = millis();
  }
}

//  reed sensor interrupt handler
void IRAM_ATTR reed0Pressed() {
  long current_millis = millis() - reed0.lastChangeMillis;
  if (current_millis >= debouncing_time) {
    double currentFrequency = (double)1000.0/(current_millis);
    // TEST: Brake
    //if (reed0.frequency-currentFrequency  > reed0.frequency*0.1 && reed0.frequency-currentFrequency  > 0.4) {
    if (reed0.frequency-currentFrequency  > reed0.frequency*0.07) {
      brakeCounter += 1;
      //brake = true;
    }
    else {
      brakeCounter = 0;
      brake = false;
    }
    brake = (brakeCounter >= 2);
    
    reed0.numberRotations += 1;
    reed0.frequency = currentFrequency;
    reed0.lastChangeMillis = millis();
  }
}

/////
///// SETUP FUNCTION
/////

void setup()
{
  // initial safety delay
  delay(2000);
  // initialize serial communication
  Serial.begin(9600);
  Serial.println("\nstarting bootup...");
  // set pin modes
  pinMode(PIN_BUTTON_INT, INPUT_PULLUP);
  pinMode(PIN_REED, INPUT_PULLUP);
  // initialize interrupts
  attachInterrupt(PIN_BUTTON_INT, button0Pressed, FALLING);
  attachInterrupt(PIN_REED, reed0Pressed, RISING);
  // initialize fastLED stuff
  FastLED.addLeds<LED_TYPE, PIN_LED, COLOR_ORDER>(leds, NUM_PIXELS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(BRIGHTNESS);
  // setup finished.
  Serial.println("system booted.");
}

/////
///// MAIN LOOP
/////

void loop() {

  //// button processing
  // onboard 'boot' button - cycles through programs
  if (button0.pressed) {
    lightingProgramID = (lightingProgramID + 1) % 2;
    button0.pressed = false;
  }

  // lighting program 1
  if (lightingProgramID == 0) {
    // driving scene
    if (millis() - reed0.lastChangeMillis <= 1500) {
      //FastLED.clear();
      // back static color
      colorSection(SECTION_BACK, 15, 0, 0);
      // front static color
      colorSection(SECTION_FRONT, 0, 30, 10);
      // deck cycle (speed sensitive)
      effectCycleSpeed(SECTION_DECKPAR, 0, -1, -1, 3);
    }
    // standing scene
    else {
      //FastLED.clear();
      // all pulse
      effectPulse(SECTION_ALL, 0, 0, 50, 5);
    }
  }

  // lighting program 2
  else if (lightingProgramID == 1) {
    // back static color
    colorSection(SECTION_BACK, 70, 0, 0);
    // deck static color
    //colorSection(SECTION_DECK,0,100,20);
    // deck pulse
    effectPulse(SECTION_DECK, 0, 100, 20, 9);
    // front pingpong
    //effectPingPong(SECTION_FRONT,0,30,3,3,150);
    // front static
    colorSection(SECTION_FRONT, 0, 30, 3);
  }
}

/////
///// PIXEL FUNCTIONS
/////
///// These functions provide an interface to the physical LED pixels.
///// They do not fire FastLED.show() on their own!
/////

// Pixel Color: Sets a pixel (with channel ch) in a section
void colorPixel(int section, int ch, int r, int g, int b) {
  // deck pixels (going round)
  if (section == SECTION_DECK) {
    // map deck pixels on both sides to sequential addresses (leave out the back section's pixels)
    if ( ch >= 0 && ch < (SECTION_DECK_PIXELS / 2)) {
      leds[ch].setRGB(r, g, b);
    }
    else if (ch >= 0 && ch < SECTION_DECK_PIXELS) {
      leds[ch + 2].setRGB(r, g, b);
    }
  }
  // front pixels
  else if (section == SECTION_FRONT) {
    if ( ch >= 0 && ch < SECTION_FRONT_PIXELS) {
      leds[ch + SECTION_FRONT_START].setRGB(r, g, b);
    }
  }
  // back pixels
  else if (section == SECTION_BACK) {
    if ( ch >= 0 && ch < SECTION_BACK_PIXELS) {
      leds[ch + SECTION_BACK_START].setRGB(r, g, b);
    }
  }
  // deck pixels (2 parallel, front-to-back)
  else if (section == SECTION_DECKPAR) {
    if ( ch >= 0 && ch < SECTION_DECKRIGHT_PIXELS) {
      leds[ch].setRGB(r, g, b);
      leds[57 - ch].setRGB(r, g, b);
    }
  }
  else if (section == SECTION_ALL) {
    if ( ch >= 0 && ch < NUM_PIXELS) {
      leds[ch].setRGB(r, g, b);
    }
  }
}

// Section Color: Sets all pixels in a section
void colorSection(int section, int r, int g, int b) {
  int n = sectionPixelCount(section);
  for (int i = 0; i < n; i++) {
    colorPixel(section, i, r, g, b);
  }
}

/////
///// HELPER FUNCTIONS
/////
///// Some general helper functions
/////

int sectionPixelCount(int section) {
  int n;
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
  return n;
}

/////
///// EFFECT FUNCTIONS
/////
///// These functions implement lighting effects.
///// They fire FastLED.show()!
/////

// Strobe: Blinks all pixels in a section
// time argument controls time in milliseconds between steps.
void effectStrobe (int section, int r, int g, int b, int time) {
  // effect has two steps - lights on and lights off
  for (int i = 0; i < 2; i++) {
    colorSection(section, r, g, b);
    FastLED.show();
    colorSection(section, 0, 0, 0);
    delay(time);
  }
}

// Pulse: Pixels in a section dim up and down
// time argument controls time in milliseconds between steps (with 511 steps per cycle):
// a time of 2ms is around 1s cycle time
void effectPulse(int section, int r, int g, int b, int time) {
  int red = 0;
  int green = 0;
  int blue = 0;
  // effect has 510 steps (two times the rgb value range)
  for (int i = 0; i <= 510; i++) {
    // dim up for first 255 steps
    if (i <= 255) {
      // set current color values for red, green and blue
      if (r > 0) {
        red = ((double)i / 255.0) * (double)r;
      }
      else {
        red = 0;
      }
      if (g > 0) {
        green = ((double)i / 255.0) * (double)g;
      }
      else {
        green = 0;
      }
      if (b > 0) {
        blue = ((double)i / 255.0) * (double)b;
      }
      else {
        blue = 0;
      }
    }
    // dim down for last 255 steps
    else {
      // set current color values for red, green and blue
      red = (r * 2) - ((double)i / 255.0) * (double)r;
      green = (g * 2) - ((double)i / 255.0) * (double)g;
      blue = (b * 2) - ((double)i / 255.0) * (double)b;
    }
    // set the section's current color
    colorSection(section, red, green, blue);
    // show it
    FastLED.show();
    // wait for next step
    delay(time);
  }
}

// Cycle: Lights up all individual pixels in a section sequentially (cycles through all addresses)
// time argument controls time in milliseconds between steps
void effectCycle(int section, int r, int g, int b, int width, int time) {
  // set random number if color is set to -1
  if (r == -1) {
    r = random(0, 256);
  }
  if (g == -1) {
    g = random(0, 256);
  }
  if (b == -1) {
    b = random(0, 256);
  }
  // set effect step count equal to number of pixels in the section
  int steps = sectionPixelCount(section);
  // iterate through all pixels in the section
  for (int i = 0; i < steps; i++) {
    // set the pixels current color
    for (int n = 0; n < width; n++) {
      colorPixel(section, (i + n), r, g, b);
    }
    // show it
    FastLED.show();
    // set the pixels off again so that it will turn of with next show
    for (int n = 0; n < width; n++) {
      colorPixel(section, (i + n), 0, 0, 0);
    }
    // wait for next step
    delay(time);
  }
}

// CycleSpeed: Lights up all individual pixels in a section sequentially, going faster when driving faster (cycles through all addresses)
void effectCycleSpeed(int section, int r, int g, int b, int width) {
  // set random number if color is set to -1
  if (r == -1) {
    r = random(0, 256);
  }
  if (g == -1) {
    g = random(0, 256);
  }
  if (b == -1) {
    b = random(0, 256);
  }
  // set effect step count equal to number of pixels in the section
  int steps = sectionPixelCount(section);
  // iterate through all pixels in the section
  for (int i = 0; i < steps; i++) {
     // TEST: Brake light
    if (brake) {
      colorSection(SECTION_BACK,255,0,0);
      colorSection(SECTION_FRONT,255,0,0);
    }
    else {
      colorSection(SECTION_BACK,15,0,0);
      colorSection(SECTION_FRONT,0,30,10);
    }
    // set the pixels current color
    for (int n = 0; n < width; n++) {
      colorPixel(section, (i + n), r, g, b);
    }
    // show it
    FastLED.show();
    // set the pixels off again so that it will turn of with next show
    for (int n = 0; n < width; n++) {
      colorPixel(section, (i + n), 0, 0, 0);
    }
    // set the current step duration according to driving speed
    int time = (double)60.0 / (reed0.frequency);
    // maximum step duration
    if (time > 200) {
      time = 200;
    }
    // wait for next step
    delay(time);
  }
}

// PingPong: Lights up all individual pixels in a section sequentially (walking the address range up  and down)
// time argument controls time in milliseconds between steps
// TODO: Implement dynamic width
void effectPingPong(int section, int r, int g, int b, int width, int time) {
  // set effect step count equal to number of pixels in the section
  int n = sectionPixelCount(section);
  // set current pixel
  int currentPixel = 0;
  // iterate thorugh all pixels, going up and down (and not using boundary pixels twice)
  for (int i = 0; i < (n * 2) - 2; i++) {
    // cycle going up
    if (i <= n - 1) {
      currentPixel = i;
    }
    // cycle going down
    else {
      currentPixel = (n * 2) - 2 - i;
    }
    // set the pixel's current color
    colorPixel(section, currentPixel, r, g, b);
    // set the pixels current color
    //for (int n=0; n<width; n++) {
    //  colorPixel(section,(currentPixel+n),r,g,b);
    //}
    // show it
    FastLED.show();
    // set the pixel off again so that it will turn of with next show
    colorPixel(section, currentPixel, 0, 0, 0);
    // wait for next step
    delay(time);
  }
}
