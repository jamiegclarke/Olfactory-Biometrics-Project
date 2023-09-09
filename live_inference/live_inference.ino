/**
* Odor classification
* Author: James Goodrick-Clarke
* 
* Based on the work by Benjamin Cabé:
    https://github.com/kartben/artificial-nose
* 
 * Based on the work by Shawn Hymel:
    https://github.com/ShawnHymel/ai-nose
    License: 0BSD (https://opensource.org/licenses/0BSD)
* 
* Date: September 9th 2023
*/


// Libraries needed
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Arduino.h>
#include "Multichannel_Gas_GMXXX.h"
#include "seeed_bme680.h"
#include "sensirion_common.h"
#include "sgp30.h"
#include "RTC_SAMD51.h"
#include "DateTime.h"
#include "TFT_eSPI.h"                                 
#include "finalnose_inferencing.h" // Name of Edge Impulse library
#include <SPI.h>
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>


// Fill in own password, ssid and GoveeApiKey, which can be obtained from a developer request through the Goove app
const char* ssid = "";
const char* password = "";
const char* goveeApiKey = "";

// This text is the amazon root certificate the Goove API uses. With this certificate we can perform HTTPS requests instead of HTTP
const char* test_root_ca = \
                            "-----BEGIN CERTIFICATE-----\n"
                            "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
                            "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
                            "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
                            "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
                            "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
                            "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
                            "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
                            "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
                            "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
                            "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
                            "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
                            "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
                            "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
                            "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
                            "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
                            "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
                            "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
                            "rqXRfboQnoZsG4q5WTP468SQvvG5\n"
                            "-----END CERTIFICATE-----\n";

WiFiClientSecure client;

// Settings
#define DEBUG               1                         // 1 to print out debugging info
#define DEBUG_NN            false                     // Print out EI debugging info
#define ANOMALY_THRESHOLD   3000                       // Scores above this are an "anomaly"
#define SAMPLING_FREQ_HZ    4                         // Sampling frequency (Hz)
#define SAMPLING_PERIOD_MS  1000 / SAMPLING_FREQ_HZ   // Sampling period (ms)
#define NUM_SAMPLES         EI_CLASSIFIER_RAW_SAMPLE_COUNT  // 4 samples at 4 Hz
#define READINGS_PER_SAMPLE EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME // 8

// Constants
#define BME680_I2C_ADDR     uint8_t(0x76)             // I2C address of BME680
#define PA_IN_KPA           1000.0                    // Convert Pa to KPa


// Preprocessing constants (drop the timestamp column)
float mins[] = {
  400236.0, 26.26, 32.4, 400.0, 0.0, 1.54, 0.52, 1.2, 0.55
};
float ranges[] = {
  8519750.0, 11.95, 50.84, 11313.0, 5204.0, 0.7, 1.0, 1.65, 1.57
};

//SD card settings
const int chipSelect = 10;  // Chip select pin for SD card
String auth[3];  // Assuming you have 3 entries in the string
String names[3];
int numbers[3];
int entryCount = 0;

// Global objects
GAS_GMXXX<TwoWire> gas;               // Multichannel gas sensor v2
Seeed_BME680 bme680(BME680_I2C_ADDR); // Environmental sensor
TFT_eSPI tft;                         // Wio Terminal LCD
RTC_SAMD51 rtc;
char str_buf[40];

void setup() {
  
  int16_t sgp_err;
  uint16_t sgp_eth;
  uint16_t sgp_h2;

  // Start serial
  Serial.begin(115200);
  while (!Serial) {
  }

  // Set root Certificate
  client.setCACert(test_root_ca);

  // Initiate WiFi (early on in prograaming)
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initiate Date and Time
  rtc.begin();
  DateTime now = DateTime(F(__DATE__), F(__TIME__));
     //!!! notice The year is limited to 2000-2099
    Serial.println("adjust time!");
    rtc.adjust(now);

  // Configure SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  // Configure LCD
  tft.begin();
  tft.setRotation(3);
  tft.setFreeFont(&FreeSansBoldOblique24pt7b);

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

  // Configure User Data
  String records = "";
  records = readfile("AUTH.csv");
  formatdata(records);
}

// This function loops constantly throughout the programs running, constantly checking for new smells
void loop() {

  // Set local variables
  float gm_no2_v;
  float gm_eth_v;
  float gm_voc_v;
  float gm_co_v;
  int16_t sgp_err;
  uint16_t sgp_tvoc;
  uint16_t sgp_co2;
  unsigned long timestamp;
  static float raw_buf[NUM_SAMPLES * READINGS_PER_SAMPLE];
  static signal_t signal;
  float temp;
  int max_idx = 0;
  float max_val = 0.0;
  
  // Collect samples and perform inference
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

    // Store raw data into the buffer
    raw_buf[(i * READINGS_PER_SAMPLE) + 0] = bme680.sensor_result_value.temperature;
    raw_buf[(i * READINGS_PER_SAMPLE) + 1] = bme680.sensor_result_value.humidity;
    raw_buf[(i * READINGS_PER_SAMPLE) + 2] = sgp_co2;
    raw_buf[(i * READINGS_PER_SAMPLE) + 3] = sgp_tvoc;
    raw_buf[(i * READINGS_PER_SAMPLE) + 4] = gm_voc_v;
    raw_buf[(i * READINGS_PER_SAMPLE) + 5] = gm_no2_v;
    raw_buf[(i * READINGS_PER_SAMPLE) + 6] = gm_eth_v;
    raw_buf[(i * READINGS_PER_SAMPLE) + 7] = gm_co_v;

    // Perform preprocessing step (normalization) on all readings in the sample
    for (int j = 0; j < READINGS_PER_SAMPLE; j++) {
      temp = raw_buf[(i * READINGS_PER_SAMPLE) + j] - mins[j];
      raw_buf[(i * READINGS_PER_SAMPLE) + j] = temp / ranges[j];
    }

    // Wait just long enough for our sampling period
    while (millis() < timestamp + SAMPLING_PERIOD_MS);
  }

  // Print out our preprocessed, raw buffer
#if DEBUG
  for (int i = 0; i < NUM_SAMPLES * READINGS_PER_SAMPLE; i++) {
    //Serial.print(raw_buf[i]);
    if (i < (NUM_SAMPLES * READINGS_PER_SAMPLE) - 1) {
      //Serial.print(", ");
    }
    
  }
  //Serial.println();
#endif

  // Turn the raw buffer in a signal which we can the classify
  int err = numpy::signal_from_buffer(raw_buf, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0) {
      ei_printf("ERROR: Failed to create signal from buffer (%d)\r\n", err);
      return;
  }

  // Run inference
  ei_impulse_result_t result = {0};
  err = run_classifier(&signal, &result, DEBUG_NN);
  if (err != EI_IMPULSE_OK) {
      ei_printf("ERROR: Failed to run classifier (%d)\r\n", err);
      return;
  }

  // Print the predictions
  ei_printf("Predictions ");
  ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)\r\n",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    ei_printf("\t%s: %.3f\r\n", 
             result.classification[i].label, 
            result.classification[i].value);
  }

  // Print anomaly detection score
#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("\tanomaly acore: %.3f\r\n", result.anomaly);
  
#endif

  // Find maximum prediction
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > max_val) {
      max_val = result.classification[i].value;
      max_idx = i;
      

    }
  }

  // Once smell has been classified, call authentication
  authenticate(result.classification[max_idx].label, result, max_idx, max_val);

}

// CSV Reading function: Reads files and stores data into char datatype
String readfile(String name){
  File dataFile = SD.open(name, FILE_READ);  // Replace with your CSV file's name
          String Content = "";
          
          if (dataFile) {
            while (dataFile.available()) {
              char c = dataFile.read(); // Read file in
              Content += c;
            }
            dataFile.close();
          } else {
            Serial.println("Error opening file for reading.");
          }

  return(Content);
}


// Formats CSV data into two list 'dictionary'
void formatdata(String records){

  const char delimiter = '\n';  // Newline character

  int startIndex = 0;
  int endIndex = records.indexOf(delimiter);
  

  // Split the string into individual lines and store in the array
  while (endIndex != -1) {
    String line = records.substring(startIndex, endIndex);
    auth[entryCount] = line;
    
    startIndex = endIndex + 1;
    endIndex = records.indexOf(delimiter, startIndex);
    
    entryCount++;
  }

  // Process the array and create the 'dictionary'
  for (int i = 0; i < entryCount; i++) {
    int commaIndex = auth[i].indexOf(',');
    names[i] = auth[i].substring(0, commaIndex);
    numbers[i] = auth[i].substring(commaIndex + 1).toInt();
  }

}

// function to add a record to the audit file
void audit(String name, String status) {
  String existingContent = readfile("audit.csv");
  
  // Date and Time package
  DateTime now = rtc.now();

  // Construct new data     
  String newEntry = name + "," + now.day() + "-" + now.month() + "-" + now.year() + "," + 
                                  now.second() + "-" + now.minute() + "-" + now.hour() + ", " + status + "\n";
  Serial.println(newEntry);

  // Append new data to existing content
  String updatedContent = existingContent + newEntry;

  // Write updated content back to the file
  File newDataFile = SD.open("audit.csv", FILE_WRITE);  // Reopen the file for writing
  if (newDataFile) {
    newDataFile.print(updatedContent);
    newDataFile.close();
    Serial.println("Data added successfully.");
    } 
  else {
    Serial.println("Error opening file for writing.");
    }

}

// function to authenticate user based on CSV file
void authenticate(String Value, ei_impulse_result_t result, int max_idx, float max_val) {


  // Find and use the associated number based on a given name
   
  int check = 3;  // Default value if name is not found
  
  // Search loop to find user in database
  for (int i = 0; i < entryCount; i++) {
    if (names[i] == Value) {
      check = numbers[i];
      break;  // Exit loop if name is found
    }
  }

  // Print predicted label and value to LCD if not anomalous
  if (result.anomaly < ANOMALY_THRESHOLD) {
      Serial.println(result.classification[max_idx].label);
    
    // Nose detects ambient background
    if (result.classification[max_idx].label == "background") {
        //turnSmartBulb("blue");  //Uncomment if you wish to send API request
        tft.fillScreen(TFT_BLUE); // Turn background of Wio Terminal Blue
        tft.setTextColor(TFT_CYAN, TFT_BLUE); // Turn writing Blue
        tft.setTextDatum(MC_DATUM); // Set text font
        tft.setTextFont(4); // Set text size
        sprintf(str_buf, "%.3f", max_val*100);
        String out = "Ambient (" + (String(str_buf).substring(0, 3)) + "%)"; // Print classifcation accuracy percentage
        tft.drawString(out, tft.width() / 2, 180);
        check = 3; // Set status back to default
    }
    else {
      // User is authenticated
      if (check == 1) {
        //turnSmartBulb("green");  //Uncomment if you wish to send API request
        tft.fillScreen(TFT_GREEN); // Turn background of Wio Terminal green
        tft.setTextColor(TFT_OLIVE, TFT_GREEN); // Turn text green
        tft.setTextDatum(MC_DATUM); // Set text font
        tft.setTextFont(4); // Set text size
        tft.drawString("User Authorised", tft.width() / 2, 180); // Print Authorised message
        sprintf(str_buf, "%.3f", max_val*100);
        String out2 = String(result.classification[max_idx].label) + " (" + (String(str_buf).substring(0, 3)) + "%)";
        tft.setTextFont(4); // Print classification percentage
        tft.drawString(out2, tft.width() / 2, 200);
        check = 3; // set status back to default
        audit(String(result.classification[max_idx].label), String("1")); // call audit request


      } 
      // User is not authenticated and not known in the database
      else if (check == 2) {
        //turnSmartBulb("red");   //Uncomment if you wish to send API request
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_MAROON, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.drawString("Access Denied", tft.width() / 2, 200);
        check = 3;
        audit(String("unknown"), String("0"));
        
        
      }
      // User is not authenticated and known in the database
      else {
        //turnSmartBulb("red");   //Uncomment if you wish to send API request
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_MAROON, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.drawString(String(result.classification[max_idx].label) + " Access Denied", tft.width() / 2, 200);
        check = 3;
        audit(String(result.classification[max_idx].label), String("0"));
      }
      
    }

    // Delay added to slow down screen update after an authentication attempt
    delay(5000);

  }else {

    // Smell is unknown
    tft.fillScreen(TFT_ORANGE);
    tft.setTextColor(TFT_WHITE, TFT_ORANGE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString("Unknown", tft.width() / 2, 180);
    delay(5000);
  }

}


// Changes Goove smartbulb based on authentication
void turnSmartBulb(String color) {

  String requestBody = "";

  // Creates correct colour request based on authentication
  if (color == "red") {
    requestBody = "{\"device\":\"MACADDRESS\",\"model\":\"H6008\",\"cmd\":{\"name\":\"color\",\"value\":{\"r\":255,\"g\":0,\"b\":0}}}";

  } 
  else if (color == "blue") {
    requestBody = "{\"device\":\"MACADDRESS\",\"model\":\"H6008\",\"cmd\":{\"name\":\"color\",\"value\":{\"r\":0,\"g\":0,\"b\":255}}}";

  }
  else {
    requestBody = "{\"device\":\"MACADDRESS\",\"model\":\"H6008\",\"cmd\":{\"name\":\"color\",\"value\":{\"r\":0,\"g\":255,\"b\":0}}}";

  }


  // Creates HTTPS request
  HTTPClient https;

  String url = "https://developer-api.govee.com/v1/devices/control";
  https.begin(url);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Govee-API-Key", goveeApiKey);

  
  Serial.println("making request");
  //PUT Request
  int httpResponseCode = https.PUT(requestBody);

  // Error output for HTTPS request
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  } else {
    Serial.printf("Error: %s\n", https.errorToString(httpResponseCode).c_str());
  }

  https.end();
}
