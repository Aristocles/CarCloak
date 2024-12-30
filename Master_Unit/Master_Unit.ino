#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include "titlepage.h" // Include the image header file
#include "lte.h"
#include "wifi.h"
#include "no_lte.h"
#include "no_wifi.h"

// UUIDs (must match the Slave)
#define SERVICE_UUID        "3e942a79-6939-4953-b5ed-1236d4cd3c00"
#define CHARACTERISTIC_CMD  "3e942a79-6939-4953-b5ed-1236d4cd3b00"
#define CHARACTERISTIC_ACK  "3e942a79-6939-4953-b5ed-1236d4cd3a00"

// GPIOs for built-in buttons
#define BUTTON1_PIN 0 // Example pin for button 1
#define BUTTON2_PIN 35 // Example pin for button 2

// EEPROM setup
#define EEPROM_SIZE 1
#define STATE_ADDR 0 // EEPROM address to store the relay state

// Variables
bool ackReceived[2] = {false, false}; // Flags for ACK receipt
bool currentState = false; // Track ON/OFF state
BLEAdvertisedDevice *slaveDevices[2] = {nullptr, nullptr};
BLEClient *clients[2] = {nullptr, nullptr};
BLERemoteCharacteristic *cmdCharacteristics[2] = {nullptr, nullptr};
int connectedSlaves = 0;

// TFT screen instance
TFT_eSPI tft = TFT_eSPI();

// Corrected image buffers (globally declared)
uint16_t correctedLte[64 * 64];
uint16_t correctedNoLte[64 * 64];
uint16_t correctedWifi[64 * 64];
uint16_t correctedNoWifi[64 * 64];

// Function to colour correct the smaller icons
void correctImage(const uint16_t *source, uint16_t *destination, int w, int h) {
  for (int i = 0; i < w * h; i++) {
    uint16_t pixel = source[i];
    destination[i] = (pixel >> 8) | (pixel << 8); // Swap bytes for RGB565
  }
}

// Function to colour correct images on the screen
void displayImageCorrected(const uint16_t* image, int x, int y, int w, int h) {
  // Send the image to the display row by row with corrected bytes
  for (int j = 0; j < h; j++) {
    uint16_t row[w];
    for (int i = 0; i < w; i++) {
      uint16_t pixel = image[j * w + i];
      row[i] = (pixel >> 8) | (pixel << 8); // Swap high and low bytes
    }
    tft.pushImage(x, y + j, w, 1, row); // Display one row at a time
  }
}

// Function to save state to EEPROM
void saveStateToEEPROM(bool state) {
  EEPROM.write(STATE_ADDR, state);
  EEPROM.commit();
  Serial.print("State saved to EEPROM: ");
  Serial.println(state ? "ON" : "OFF");
}

// Function to load state from EEPROM
bool loadStateFromEEPROM() {
  return EEPROM.read(STATE_ADDR) == 1;
}

// Function to show "Connecting" screen
void showConnectingScreen(int found, int connected) {
  // Display the title image
  displayImageCorrected(titlepage, 0, 0, 240, 135);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Connecting to Slaves...");
  //tft.setCursor(10, 30);
  //tft.printf("Found: %d\n", found);
  //tft.setCursor(10, 50);
  //tft.printf("Connected: %d\n", connected);
}

// Function to show "OFF" screen
void showOffScreen() {
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Cloak State: OFF");
  tft.println("");
  tft.setCursor(10, 30);
  tft.println("WiFi: Connected");
  tft.setCursor(23, 50);
  tft.println("LTE: Connected");

  // Display corrected images
  tft.pushImage(10, 70, 64, 64, correctedWifi);
  tft.pushImage(90, 70, 64, 64, correctedLte);
}

// Function to show "ON" screen
void showOnScreen() {
  tft.fillScreen(TFT_GREEN);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Cloak State: ON");
  tft.println("");
  tft.setCursor(10, 30);
  tft.println("WiFi: Disconnected");
  tft.setCursor(23, 50);
  tft.println("LTE: Disconnected");

  // Display corrected images
  tft.pushImage(10, 70, 64, 64, correctedNoWifi);
  tft.pushImage(90, 70, 64, 64, correctedNoLte);
}

// Function to show "Sending Command" screen
void showSendingCommandScreen() {
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("Sending...");
}

// BLE callbacks
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      for (int i = 0; i < 2; i++) {
        if (slaveDevices[i] == nullptr) {
          slaveDevices[i] = new BLEAdvertisedDevice(advertisedDevice);
          Serial.printf("Slave %d found!\n", i + 1);
          break;
        }
      }
    }
  }
};

void connectToSlave(int index) {
  Serial.printf("Connecting to Slave %d...\n", index + 1);
  clients[index] = BLEDevice::createClient();
  if (!clients[index]->connect(slaveDevices[index])) {
    Serial.printf("Failed to connect to Slave %d. Retrying...\n", index + 1);
    delete slaveDevices[index];
    slaveDevices[index] = nullptr;
    return;
  }

  Serial.printf("Connected to Slave %d!\n", index + 1);
  BLERemoteService *remoteService = clients[index]->getService(SERVICE_UUID);
  if (!remoteService) {
    Serial.printf("Failed to find service on Slave %d. Disconnecting...\n", index + 1);
    clients[index]->disconnect();
    return;
  }

  cmdCharacteristics[index] = remoteService->getCharacteristic(CHARACTERISTIC_CMD);
  if (!cmdCharacteristics[index]) {
    Serial.printf("Failed to find CMD characteristic on Slave %d. Disconnecting...\n", index + 1);
    clients[index]->disconnect();
    return;
  }

  connectedSlaves++;
}

void scanAndConnect() {
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  scan->setActiveScan(true);

  Serial.println("Scanning for Slaves...");
  scan->start(10, false); // Scan for 10 seconds

  for (int i = 0; i < 2; i++) {
    if (slaveDevices[i] != nullptr) {
      connectToSlave(i);
    }
  }

  showConnectingScreen(2, connectedSlaves);
  if (connectedSlaves < 2) {
    Serial.println("Not all Slaves connected. Retrying...");
    delay(5000);
    scanAndConnect();
  }
}

void sendCommandToSlaves(const char *command) {
  for (int i = 0; i < 2; i++) {
    if (clients[i] && clients[i]->isConnected() && cmdCharacteristics[i]) {
      cmdCharacteristics[i]->writeValue(command);
      Serial.printf("Command '%s' sent to Slave %d.\n", command, i + 1);
    }
  }
}

bool sleepMode = false; // Track sleep mode state

void setup() {
  Serial.begin(115200);
  Serial.println("Master starting...");

  // Initialize buttons
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  // Initialise EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Load the last state from EEPROM
  currentState = loadStateFromEEPROM();
  Serial.print("Loaded state from EEPROM: ");
  Serial.println(currentState ? "ON" : "OFF");

  // Initialise TFT display
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK); // Clear the screen
  showConnectingScreen(0, 0);

  // Preprocess all images
  correctImage(lte, correctedLte, 64, 64);
  correctImage(no_lte, correctedNoLte, 64, 64);
  correctImage(wifi, correctedWifi, 64, 64);
  correctImage(no_wifi, correctedNoWifi, 64, 64);

  // Initialise BLE
  BLEDevice::init("Master Unit");
  scanAndConnect();

  if (connectedSlaves == 2) {
    const char *initialCommand = currentState ? "ON" : "OFF";
    sendCommandToSlaves(initialCommand);
    currentState ? showOnScreen() : showOffScreen();
  }
}

void loop() {
  static unsigned long lastStatusUpdate = 0; // Track the last time the screen was updated
  // Check if all connected slaves are still active and reconnect if needed
  for (int i = 0; i < 2; i++) {
    if (clients[i] && !clients[i]->isConnected()) {
      Serial.printf("Slave %d disconnected. Attempting to reconnect...", i + 1);
      connectToSlave(i);
    }
  }
  static unsigned long lastCommandTime = 0; // Keep track of the last time command was sent

  // Update screen with connection status every 5 seconds
  if (millis() - lastStatusUpdate >= 5000) {
    bool anyDisconnected = false;
    for (int i = 0; i < 2; i++) {
      if (!clients[i] || !clients[i]->isConnected()) {
        anyDisconnected = true;
        break;
      }
    }

    if (anyDisconnected) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Slave Status:");

    for (int i = 0; i < 2; i++) {
      tft.setCursor(10, 40 + (i * 30));
      if (clients[i] && clients[i]->isConnected()) {
        tft.printf("Slave %d: Connected", i + 1);
      } else {
        tft.printf("Slave %d: Disconnected", i + 1);
      }
      connectToSlave(i);
    }

    tft.setCursor(10, 100);
    tft.printf("Command: %s", currentState ? "ON" : "OFF");

          lastStatusUpdate = millis();
    }
  }
  if (digitalRead(BUTTON1_PIN) == LOW) {
    currentState = !currentState;
    const char *command = currentState ? "ON" : "OFF";
    sendCommandToSlaves(command);
    saveStateToEEPROM(currentState);
    currentState ? showOnScreen() : showOffScreen();
    delay(500);
  }

  if (digitalRead(BUTTON2_PIN) == LOW) {
    sleepMode = !sleepMode;
    if (sleepMode) {
      tft.writecommand(0x10); // Enter sleep mode
      Serial.println("Entering sleep mode to conserve battery...");
    } else {
      tft.writecommand(0x11); // Exit sleep mode
      delay(120); // Delay for wake-up
      Serial.println("Exiting sleep mode, screen is back on.");
    }
    delay(500); // Debounce
  }

  // Periodically send the latest command to all connected slaves
  if (millis() - lastCommandTime >= 10000) { // Every 10 seconds
    const char *command = currentState ? "ON" : "OFF";
    sendCommandToSlaves(command);
    Serial.println("Periodic command sent to all slaves.");
    lastCommandTime = millis();
  }
}