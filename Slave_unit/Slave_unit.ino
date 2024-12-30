#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

#define RELAY_PIN1 2 // GPIO pin for the relay
#define RELAY_PIN2 3 // GPIO pin for the relay
#define EEPROM_SIZE 1 // We only need 1 byte to store the relay state
#define RELAY_STATE_ADDR 0 // EEPROM address to store the relay state

// RGB or Normal LED config
#define LED_PIN        8     // GPIO8 for the onboard RGB LED or normal LED
#define NUM_LEDS    1       // Single LED

Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Set LED type (true for NeoPixel, false for regular LED)
bool useNeoPixel = false;

// BLE UUIDs
#define SERVICE_UUID        "3e942a79-6939-4953-b5ed-1236d4cd3c00"
#define CHARACTERISTIC_CMD  "3e942a79-6939-4953-b5ed-1236d4cd3b00"
#define CHARACTERISTIC_ACK  "3e942a79-6939-4953-b5ed-1236d4cd3a00"

BLEServer *server;                // BLE server instance
BLECharacteristic *ackCharacteristic;

bool relayState = false;          // Current state of the relay (off by default)
bool deviceConnected = false;     // Tracks connection state

// Function to save relay state to EEPROM
void saveRelayStateToEEPROM(bool state) {
  EEPROM.write(RELAY_STATE_ADDR, state);
  EEPROM.commit();
  Serial.print("Relay state saved to EEPROM: ");
  Serial.println(state ? "ON" : "OFF");
}

// Function to load relay state from EEPROM
bool loadRelayStateFromEEPROM() {
  return EEPROM.read(RELAY_STATE_ADDR) == 1; // Return true if stored value is 1
}

// Callback to handle events when a client connects/disconnects
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Master connected.");
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Master disconnected. Restarting advertising...");
    pServer->startAdvertising(); // Restart advertising on disconnection
  }
};

// Callback for when a command is received
class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String command = characteristic->getValue().c_str(); // Convert to Arduino String

    if (command == "ON") {
      digitalWrite(RELAY_PIN1, HIGH); // Energise relay
      digitalWrite(RELAY_PIN2, HIGH); // Energise relay
      relayState = true;
      setLEDColor("ON");
      Serial.println("Command received: ON");
      saveRelayStateToEEPROM(relayState); // Save state to EEPROM
    } else if (command == "OFF") {
      digitalWrite(RELAY_PIN1, LOW); // De-energise relay
      digitalWrite(RELAY_PIN2, LOW); // De-energise relay
      relayState = false;
      setLEDColor("OFF");
      Serial.println("Command received: OFF");
      saveRelayStateToEEPROM(relayState); // Save state to EEPROM
    } else {
      setLEDColor("ERR");
      Serial.println("Invalid command received");
      return;
    }

    // Send ACK back to Master
    ackCharacteristic->setValue("ACK");
    ackCharacteristic->notify();
    Serial.println("ACK sent to Master.");
  }
};

void setLEDColor(String commandState) {
  Serial.println();
  if (useNeoPixel) {
    if (commandState == "ON") {
      led.setPixelColor(0, led.Color(0, 255, 0)); // Green
      Serial.println("Command State: ON - NeoPixel LED Color: Green");
    } else if (commandState == "OFF") {
      led.setPixelColor(0, led.Color(255, 0, 0)); // Red
      Serial.println("Command State: OFF - NeoPixel LED Color: Red");
    } else {
      led.setPixelColor(0, led.Color(0, 0, 255)); // Blue
      Serial.println("Command State: Other - NeoPixel LED Color: Blue");
    }
    led.show(); // Update the NeoPixel LED with the new colour
  } else {
    if (commandState == "ON") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("Command State: ON - Regular LED ON");
    } else if (commandState == "OFF") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Command State: OFF - Regular LED OFF");
    } else {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Command State: Other - Regular LED OFF");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Starting Slave unit...");

  // Initialise LEDs
  if (useNeoPixel) {
    led.begin();              // Initialise the NeoPixel library
    led.show();               // Ensure the LED is off initially
  } else {
    digitalWrite(LED_PIN, HIGH); // Ensure the regular LED is off initially
  }
  setLEDColor("BOOT");

  delay(1000);

  // Initialise EEPROM
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("Initialising EEPROM...");

  // Load last relay state from EEPROM and set relay accordingly
  relayState = loadRelayStateFromEEPROM();
  digitalWrite(RELAY_PIN1, relayState ? HIGH : LOW);
  digitalWrite(RELAY_PIN2, relayState ? HIGH : LOW);
  Serial.print("Loaded relay state from EEPROM: ");
  Serial.println(relayState ? "ON" : "OFF");

  // Set LED colour based on relay state
  if (relayState) {
    setLEDColor("ON"); // Green for ON
  } else {
    setLEDColor("OFF"); // Red for OFF
  }

  // Initialise BLE
  BLEDevice::init("Slave Unit");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  BLEService *service = server->createService(SERVICE_UUID);

  // Create Command Characteristic
  BLECharacteristic *cmdCharacteristic = service->createCharacteristic(
      CHARACTERISTIC_CMD,
      BLECharacteristic::PROPERTY_WRITE
  );
  cmdCharacteristic->setCallbacks(new CommandCallback());

  // Create ACK Characteristic
  ackCharacteristic = service->createCharacteristic(
      CHARACTERISTIC_ACK,
      BLECharacteristic::PROPERTY_NOTIFY
  );
  ackCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  service->start();

  // Start advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();

  Serial.println("Slave is ready and advertising...");
}

void loop() {
  // Optional: Add debugging for connection status
  static bool prevConnected = false;
  if (deviceConnected != prevConnected) {
    prevConnected = deviceConnected;
    if (deviceConnected) {
      Serial.println("Device is connected.");
    } else {
      Serial.println("Device is not connected. Awaiting reconnection...");
    }
  }
  delay(1000); // Small delay for debugging output
}
