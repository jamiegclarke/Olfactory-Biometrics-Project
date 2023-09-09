/**
* Odor classification
* 
* Press the 5-way switch hat in to start collection process for 2 seconds.
* Sensor data is sent over serial port as CSV. Use a collection script to
* save the data into .csv files.
*
* Based on the work by Benjamin Cabé:
    https://github.com/kartben/artificial-nose
* 
* Shawn Hymel Collection code:
    https://github.com/ShawnHymel/ai-nose
    License: 0BSD (https://opensource.org/licenses/0BSD)
* 
* Date: September 9th 2023
*/

#include <Wire.h>

#include "Multichannel_Gas_GMXXX.h"
#include "seeed_bme680.h"
#include "sensirion_common.h"
#include "sgp30.h"

// Settings
#define BTN_START           0                         // 1: press button to start, 0: loop
#define BTN_PIN             WIO_5S_PRESS              // Pin that button is connected to
#define SAMPLING_FREQ_HZ    4                         // Sampling frequency (Hz)
#define SAMPLING_PERIOD_MS  1000 / SAMPLING_FREQ_HZ   // Sampling period (ms)
#define NUM_SAMPLES         8                         // 8 samples at 4 Hz is 2 seconds

// Constants
#define BME680_I2C_ADDR     uint8_t(0x76)             // I2C address of BME680
#define PA_IN_KPA           1000.0                    // Convert Pa to KPa

// Global objects
GAS_GMXXX<TwoWire> gas;               // Multichannel gas sensor v2
Seeed_BME680 bme680(BME680_I2C_ADDR); // Environmental sensor

void setup() {
  
  int16_t sgp_err;
  uint16_t sgp_eth;
  uint16_t sgp_h2;

  // Initialize button
  pinMode(BTN_PIN, INPUT_PULLUP);
  
  // Start serial
  Serial.begin(115200);

  // Initialize gas sensors
  gas.begin(Wire, 0x08);

  // Initialize environmental sensor
  while (!bme680.init()) {
    Serial.println("Trying to initialize BME680...");
    delay(1000);
  }

  // Initialize VOC and eCO2 sensor
  while (sgp_probe() != STATUS_OK) {
    Serial.println("Trying to initialize SGP30...");
    delay(1000);
  }

  // Perform initial read
  sgp_err = sgp_measure_signals_blocking_read(&sgp_eth, &sgp_h2);
  if (sgp_err != STATUS_OK) {
    Serial.println("Error: Could not read signal from SGP30");
    while (1);
  }
}

void loop() {

  float gm_no2_v;
  float gm_eth_v;
  float gm_voc_v;
  float gm_co_v;
  int16_t sgp_err;
  uint16_t sgp_tvoc;
  uint16_t sgp_co2;
  unsigned long timestamp;

  // Wait for button press
#if BTN_START
  while (digitalRead(BTN_PIN) == 1);
#endif

  // Print header
  Serial.println("timestamp,temp,humd,pres,co2,voc1,voc2,no2,eth,co");

  // Transmit samples over serial port
  for (int i = 0; i < NUM_SAMPLES; i++) {

    // Take timestamp so we can hit our target frequency
    timestamp = millis();

    // Read from GM-X02b sensors (multichannel gas)
    gm_no2_v = gas.calcVol(gas.getGM102B());
    gm_eth_v = gas.calcVol(gas.getGM302B());
    gm_voc_v = gas.calcVol(gas.getGM502B());
    gm_co_v = gas.calcVol(gas.getGM702B());
  
    // Read BME680 environmental sensor
    if (bme680.read_sensor_data()) {
      Serial.println("Error: Could not read from BME680");
      return;
    }
  
    // Read SGP30 sensor
    sgp_err = sgp_measure_iaq_blocking_read(&sgp_tvoc, &sgp_co2);
    if (sgp_err != STATUS_OK) {
      Serial.println("Error: Could not read from SGP30");
      return;
    }

    // Print CSV data with timestamp
    Serial.print(timestamp);
    Serial.print(",");
    Serial.print(bme680.sensor_result_value.temperature);
    Serial.print(",");
    Serial.print(bme680.sensor_result_value.humidity);
    Serial.print(",");
    Serial.print(bme680.sensor_result_value.pressure / PA_IN_KPA);
    Serial.print(",");
    Serial.print(sgp_co2);
    Serial.print(",");
    Serial.print(sgp_tvoc);
    Serial.print(",");
    Serial.print(gm_voc_v);
    Serial.print(",");
    Serial.print(gm_no2_v);
    Serial.print(",");
    Serial.print(gm_eth_v);
    Serial.print(",");
    Serial.print(gm_co_v);
    Serial.println();

    // Wait just long enough for our sampling period
    while (millis() < timestamp + SAMPLING_PERIOD_MS);
  }

  // Print empty line to transmit termination of recording
  Serial.println();

  // Make sure the button has been released for a few milliseconds
#if BTN_START
  while (digitalRead(BTN_PIN) == 0);
  delay(100);
#endif
}
