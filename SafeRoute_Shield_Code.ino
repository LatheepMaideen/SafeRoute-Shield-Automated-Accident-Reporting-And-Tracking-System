
#include <SoftwareSerial_Class.h>
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"


const int microphonePin = A1;   // Analog pin for the microphone sensor
const int ledPin = 13;          // Digital pin for an LED (for testing purposes)
const int threshold = 50;      // Adjust this threshold value based on your environment



MPU6050 mpu;
int16_t ax, ay, az;
int16_t gx, gy, gz;

struct MyData {
  byte X;
  byte Y;
  byte Z;
};

MyData data;

SoftwareSerial gpsSerial(2, 3);  // RX, TX for GPS
HardwareSerial& gsmSerial = Serial1;  // Serial1 for GSM module

const int vibrationPin = 4;  // Connect the sensor to digital pin 4

const int alcoholSensorPin = A0;     // Analog pin to which the MQ-3 sensor is connected
const int baselineValue = 10;       // Calibration baseline value (analog reading in clean air)
const int maxSensorValue = 1023;      // Maximum analog reading from the sensor
const int maxAlcoholPercentage = 100; // Maximum percentage for alcohol concentration
const int criticalLevelPercentage = 6; // Critical alcohol concentration level

// Global variables for latitude and longitude
float latitude = 0.0;
float longitude = 0.0;


float customAtof(String str) {
  int index = 0;
  float result = 0.0;
  float sign = 1.0;

  if (str.charAt(0) == '-') {
    sign = -1.0;
    index = 1;
  }

  while (index < str.length() && isDigit(str.charAt(index))) {
    result = result * 10.0 + (str.charAt(index) - '0');
    index++;
  }

  if (index < str.length() && str.charAt(index) == '.') {
    float fraction = 0.1;
    index++;

    while (index < str.length() && isDigit(str.charAt(index))) {
      result = result + (str.charAt(index) - '0') * fraction;
      fraction *= 0.1;
      index++;
    }
  }

  return sign * result;
}

// Function declarations
float customAtof(String str);
void processNMEASentence();
void sendSMS(const char* message);
bool waitForResponse(const char* response, unsigned int timeout);

// MyData data;
int alcoholPercentage; // Declare alcoholPercentage as a global variable



void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);

  Wire.begin();
  mpu.initialize();
  pinMode(microphonePin, INPUT);
  pinMode(vibrationPin, INPUT);
  delay(2000);

  Serial.println("Initializing SIM800C...");
  gsmSerial.println("AT"); // Send AT command to check communication
  delay(1000);

  if (waitForResponse("OK", 5000)) {
    Serial.println("SIM800C is ready.");
  } else {
    Serial.println("Error: Unable to communicate with SIM800C.");
    while (1);
  }
}

void loop() {
  
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  data.X = map(ax, -17000, 17000, 0, 255);
  data.Y = map(ay, -17000, 17000, 0, 255);
  data.Z = map(az, -17000, 17000, 0, 255);

  Serial.print("Axis X = ");
  Serial.print(data.X);
  Serial.print("  ");
  Serial.print("Axis Y = ");
  Serial.print(data.Y);
  Serial.print("  ");
  Serial.print("Axis Z  = ");
  Serial.println(data.Z);


  // Continuously read GPS data
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (c == '\n') {
      processNMEASentence();
    }
  }



  int soundLevel = analogRead(microphonePin);
  Serial.print("Sound Level: ");
  Serial.println(soundLevel);



  int vibrationValue = digitalRead(vibrationPin);
  int alcoholSensorValue = analogRead(alcoholSensorPin);


  Serial.print(" Alcohol Sensor Value: ");
  Serial.println(alcoholSensorValue);
  // Calculate percentage based on calibration
  

  Serial.print("Alcohol Percentage: ");
  Serial.print(alcoholPercentage);
  Serial.println("%");




  // Check if the alcohol level exceeds the critical level
  if (data.Z < 50 && vibrationValue == HIGH) {
    sendCurrentAxyz();
    delay(1000); // Send SMS every 5 seconds (for testing purposes)
  }

  if (data.Y < 50 && vibrationValue == HIGH) {
    sendCurrentAxyz();
    delay(1000); // Send SMS every 5 seconds (for testing purposes)
  }

  if (data.X < 50 && vibrationValue == HIGH) {
    sendCurrentAxyz();
    delay(1000); // Send SMS every 5 seconds (for testing purposes)
  }

   if (soundLevel > threshold) {
    Serial.println("Accident sound detected!");
    sendSound();
    delay(1000);
    // Generate Google Maps link
    }

  if (alcoholPercentage > criticalLevelPercentage) {
    MQ3SMS();
    delay(1000); // Send SMS every 5 seconds (for testing purposes)
  } 
  delay(1000);
}

// Function to process NMEA sentence
void processNMEASentence() {
  char buffer[80];  // Buffer to hold the NMEA sentence
  char *tokens[15];  // Array to hold individual data fields

  // Read the NMEA sentence into the buffer
  gpsSerial.readBytesUntil('\n', buffer, sizeof(buffer));

  // Tokenize the sentence
  char *token = strtok(buffer, ",");
  int count = 0;
  while (token != NULL && count < 15) {
    tokens[count] = token;
    token = strtok(NULL, ",");
    count++;
  }

  // Check if it's a GPGGA sentence
  if (count > 6 && strcmp(tokens[0], "$GPGGA") == 0) {
    // Extract relevant information
    Serial.print("Time: ");
    Serial.println(tokens[1]);

    // Extract latitude and convert to decimal degrees
    latitude = customAtof(tokens[2]);
    int degrees = int(latitude / 100);  // Extract degrees
    float minutesLatitude = latitude - degrees * 100;  // Extract minutes
    latitude = degrees + minutesLatitude / 60;  // Convert to decimal degrees
    Serial.print("Latitude (DD): ");
    Serial.println(latitude, 6);

    // Extract longitude and convert to decimal degrees
    longitude = customAtof(tokens[4]);
    degrees = int(longitude / 100);  // Extract degrees
    float minutesLongitude = longitude - degrees * 100;  // Extract minutes
    longitude = degrees + minutesLongitude / 60;  // Convert to decimal degrees
    Serial.print("Longitude (DD): ");
    Serial.println(longitude, 6);

    Serial.print("Fix Quality: ");
    Serial.println(tokens[6]);
    Serial.print("Altitude: ");
    Serial.println(tokens[9]);

    // Send current Axyz after processing GPS data
    // sendCurrentAxyz();
  }
}

// Function to send SMS with the given message
void sendSMS(const char* message) {
  Serial.println("Sending SMS...");

  gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  gsmSerial.print("AT+CMGS=\"+918778871911\"\r"); // Replace with the recipient's phone number
  delay(1000);

  gsmSerial.print(message);
  delay(1000);

  gsmSerial.write(26); // Send Ctrl+Z to indicate the end of the message
  delay(1000);

  if (waitForResponse("OK", 10000)) {
    Serial.println("SMS sent successfully!");
  } else {
    Serial.println("Error: Failed to send SMS.");
  }
}

// Function to send current Axyz
void sendCurrentAxyz() {
  Serial.println("Sending SMS...");

  gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  gsmSerial.print("AT+CMGS=\"+918778871911\"\r"); // Replace with the recipient's phone number
  delay(1000);

  // Generate Google Maps link
  char googleMapsLink[60];
  sprintf(googleMapsLink, "https://maps.google.com/maps?q=%.6f,%.6f", latitude, longitude);

  // Print the Google Maps link
  Serial.println("Google Maps Link: " + String(googleMapsLink));

  gsmSerial.println("Acceleration and Vibration detected");
  delay(1000);

  gsmSerial.println(googleMapsLink);
  delay(1000);

  gsmSerial.write(26); // Send Ctrl+Z to indicate the end of the message
  delay(1000);
}


// Function to send current Axyz
void MQ3SMS() {
  Serial.println("Sending SMS...");

  gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  gsmSerial.print("AT+CMGS=\"+918778871911\"\r"); // Replace with the recipient's phone number
  delay(1000);

  // Convert alcoholPercentage to a string
  String alcoholString = String(alcoholPercentage);

  // Debugging: print the alcohol value to Serial Monitor
  Serial.print("Debug - Alcohol Percentage: ");
  Serial.println(alcoholString);

  // Send the alcohol percentage as part of the SMS message
  gsmSerial.println("Alcohol Percentage: " + alcoholString + "%");

  // Debugging: print a message indicating the SMS is being sent
  Serial.println("Debug - Sending SMS message");

  // Send Ctrl+Z to indicate the end of the message
  gsmSerial.write(0x1A);

  // Debugging: print a message indicating the Ctrl+Z is being sent
  Serial.println("Debug - Ctrl+Z sent");

  delay(1000);
}



// Function to send Sound detection above threshold msg
void sendSound() {
  Serial.println("Sending SMS...");

  gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  gsmSerial.print("AT+CMGS=\"+918778871911\"\r"); // Replace with the recipient's phone number
  delay(1000);

  // Generate Google Maps link
  char googleMapsLink[60];
  sprintf(googleMapsLink, "https://maps.google.com/maps?q=%.6f,%.6f", latitude, longitude);

  // Print the Google Maps link
  Serial.println("Google Maps Link: " + String(googleMapsLink));

  gsmSerial.println("Accident Sound Detected");
  delay(1000);

  gsmSerial.println(googleMapsLink);
  delay(1000);

  gsmSerial.write(26); // Send Ctrl+Z to indicate the end of the message
  delay(1000);
}

// Function to wait for a response from the GSM module
bool waitForResponse(const char* response, unsigned int timeout) {
  unsigned long startTime = millis();
  char buffer[64];
  buffer[0] = '\0'; // Initialize buffer

  while (millis() - startTime < timeout) {
    if (gsmSerial.available()) {
      char c = gsmSerial.read();
      strncat(buffer, &c, 1);
      if (strstr(buffer, response)) {
        return true;
      }
    }
  }

  return false;
}