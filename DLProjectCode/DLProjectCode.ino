/*
    Modified      February 23, 2024, Bob Fontana, AK3Y
                  Overhaul of display code using display.width and strlen
                  Corrected power calculation code in 2017 W8TEE article
                  Added 3.3V reference adjustment to determine true ADC range
                  Modified resistor divider calculation to provide more accurate measurement

    Release 1.01  June 15, 2018, Jack Purdum, W8TEE. Minor edits to the code to make it easier to read

    Release 1.0   Oct.13, 2017, Jack Purdum, W8TEE, Al Peter, AC8GY. You may use/modify this code,
                  but keep this comment intact in the file and place any changes in front of this
                  entry.
*/

#include <Wire.h>             // Standard IDE library file
#include <Adafruit_GFX.h>     // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h> // https://github.com/adafruit/Adafruit_SSD1306

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels (OLED capable of 64 bit height, but using only 32 here)
#define OLED_RESET 4     // Reset pin # for OLED display

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)

#define REFERENCEPIN A1 // Measure onboard 3.3V reference with this pin
#define SENSORPIN A2    // Arduino pin that the RF detector is attached to
#define ITERATIONS 30

// See QST text for determining following constants
// #define MYDUMMYLOADOHMS         49.2    	  // Measured dummy load resistance - ORIGINAL VALUE
#define MYDUMMYLOADOHMS 49.5   // Measured dummy load resistance
#define DIODEVOLTAGEDROP 0.653 // Voltage drop from diode(s)
#define V33 3.28               // Measured 3.3V reference voltage
// #define Vratio                  19.490      // Resistor divider ratio - ORIGINAL VALUE
#define Vratio 19.50 // Resistor divider ratio

int sensorValue = 0; // Measured sensor value

void Splash()
{
  const char *text1 = "RF DUMMY LOAD";
  const char *text2 = "*****";
  const char *text3 = "Bob Fontana AK3Y";

  display.clearDisplay(); // Clear the graphics display buffer.
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(display.width() / 2 - strlen(text1) * 6 / 2, 0); // Each character is approximately 6 pixels wide in default type font
  display.println(text1);
  display.setCursor(display.width() / 2 - strlen(text2) * 6 / 2, 9);
  display.println(text2);
  display.setCursor(display.width() / 2 - strlen(text3) * 6 / 2, 18);
  display.println(text3);

  display.display();
}

void setup()
{
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the 128x32).
                                             // Your OLED might use a different address. Google
                                             // "Arduino I2C scanner" or http://playground.arduino.cc/Main/I2cScanner

  Splash();               // Display Splash screen
  delay(4000);            // Wait 4 seconds
  display.clearDisplay(); // Clear the graphics display buffer.
}

void loop()
{
  char buff[10];
  const char *text = "WATTS IN";
  int i;
  float sum;
  float watts;

  sum = 0.0;
  i = 0;
  while (i < ITERATIONS)
  {
    sensorValue = analogRead(SENSORPIN); // Input from voltage divider
    delay(10);                           // Let pin settle
    watts = CalculateWatts(sensorValue);
    sum += watts;
    i++;
  }
  sum /= ITERATIONS;
  dtostrf(sum, 4, 2, buff); // Convert value into floating point character string

  display.clearDisplay();

  display.setTextSize(1); // Display Calculated Watts
  display.setCursor(display.width() / 2 - strlen(buff) * 6 / 2, 0);
  display.println(buff);

  display.setTextSize(1); // Display WATTS IN label
  display.setTextColor(WHITE);
  display.setCursor(display.width() / 2 - strlen(text) * 6 / 2, 10); // Each character is approximately 6 pixels wide in default type font
  display.println(text);

  bargraph(0, 20, display.width(), 8, sum, autorange(sum)); // Display autoranging bargraph
  display.display();
  delay(250);
}

float CalculateWatts(int sensorValue)
/*****
  Purpose: To convert the value read from the DL and convert it to watts

  Parameter list:
    int sensorValue       the value read on the analog input pin
  Return value:
    float               the calculated watts

Analysis:
Ds = sensorValue (0..1023)
v2 = center point in the voltage divider
v1 = average value at the output of the diode and top of capacitor
vin = input voltage to the Wattmeter
Vcc = voltage corresponding to 1023 on the A/D outputs (may vary a bit from unit to unit and how the device is powered -- usually around 5V).

Then,
Ds ~ 1023 v2/Vcc
v1 = v2 * Vratio
v1 ~ vin - DIODEVOLTAGEDROP
Pin(rms) = vin^2/2*MYDUMMYLOADOHMS  That is (peak voltage * 0.707)^2/MYDUMMYLOADOHMS

Pin(rms) ~ (Vratio*Ds*Vcc/1023 + DIODEVOLTAGEDROP)^2/(2*MYDUMMYLOADOHMS)
*****/
{
  float vcc, temp;

  vcc = Measure33Reference(); // Determine the actual Arduino A/D reference voltage
  temp = Vratio * sensorValue * vcc / 1023 + DIODEVOLTAGEDROP;
  temp *= temp;
  temp /= 2 * MYDUMMYLOADOHMS;

  return temp;
}

float Measure33Reference()
/* Take an average reading of the A/D sampling of the onboard 3.3V reference
 * From this value, determine the effective Vcc for the Arduino
 */
{
  int i, k;
  float v5, sum;

  sum = 0.0;
  i = 0;
  while (i < ITERATIONS)
  {
    k = analogRead(REFERENCEPIN); // Input from 3.3V reference attached to REFERENCEPIN
    sum += k;
    i++;
  }
  sum /= ITERATIONS;
  v5 = 1023 * V33 / sum;
  // Serial.println(v5); -- Debug output A/D converter calculated maximum range
  return (v5);
}

float autorange(float sum)
{
  if ((sum >= 0) && (sum <= 1))
    return 1;
  else if ((sum > 1) && (sum <= 10))
    return 10;
  else if ((sum > 10) && (sum <= 100))
    return 100;
}

void bargraph(const uint8_t x, uint8_t y, uint8_t w, uint8_t h, float level, float maxlevel)
{
  /* (x,y) is the start point for the bargraph
   * w = width of graph in pixels
   * h = height of graph in pixels
   * level = value for the graph (e.g. 0..1023)
   * maxlevel = maximum value for the graph (e.g. 1023)
   */
  int w_val;
  w_val = (int)(level * w / maxlevel);
  display.drawRect(x, y, w, h, WHITE);
  display.fillRect(x, y, w_val, h, WHITE);
}
