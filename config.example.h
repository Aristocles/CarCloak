// =============================================================================
// CarCloak Configuration — copy this file to config.h and edit as needed
// =============================================================================
//
// This file lists every user-tuneable parameter found in the Master and Slave
// firmware.  Defaults match the values currently hard-coded in the .ino files.
// After copying, adjust the values below for your specific hardware setup,
// then #include "config.h" in Master_Unit.ino and Slave_unit.ino.
//
// ─── BLE Service / Characteristic UUIDs ─────────────────────────────────────
// These must be identical on the Master and all Slave units.
#define SERVICE_UUID        "3e942a79-6939-4953-b5ed-1236d4cd3c00"
#define CHARACTERISTIC_CMD  "3e942a79-6939-4953-b5ed-1236d4cd3b00"
#define CHARACTERISTIC_ACK  "3e942a79-6939-4953-b5ed-1236d4cd3a00"

// ─── BLE Device Names ───────────────────────────────────────────────────────
// Shown during scanning / advertising.  Change if running multiple CarCloak
// setups in proximity.
#define MASTER_DEVICE_NAME  "Master Unit"
#define SLAVE_DEVICE_NAME   "Slave Unit"

// ─── Master Unit — GPIO Pin Assignments ─────────────────────────────────────
// BUTTON1_PIN  – toggles relay state (cloak on/off)
// BUTTON2_PIN  – toggles display sleep mode
#define BUTTON1_PIN  0
#define BUTTON2_PIN  35

// ─── Slave Unit — GPIO Pin Assignments ──────────────────────────────────────
// RELAY_PIN1 / RELAY_PIN2  – GPIOs driving the relay transistor bases
// LED_PIN                  – status LED (NeoPixel or standard)
#define RELAY_PIN1  2
#define RELAY_PIN2  3
#define LED_PIN     8

// ─── Slave Unit — LED Configuration ─────────────────────────────────────────
// Number of LEDs in the NeoPixel chain (typically 1 for on-board RGB).
#define NUM_LEDS  1

// Set to true for NeoPixel (WS2812) RGB LED, false for a plain on/off LED.
#define USE_NEOPIXEL  false

// ─── EEPROM Settings ────────────────────────────────────────────────────────
// Byte count reserved in EEPROM and the address used to persist relay state.
#define EEPROM_SIZE       1
#define STATE_ADDR        0   // Master unit EEPROM address
#define RELAY_STATE_ADDR  0   // Slave unit EEPROM address

// ─── Timing / Behaviour ─────────────────────────────────────────────────────
// BLE scan duration in seconds (Master)
#define BLE_SCAN_DURATION_SEC    10

// Interval (ms) between periodic keep-alive commands sent to slaves (Master)
#define PERIODIC_CMD_INTERVAL_MS 10000

// Interval (ms) between slave-status screen refreshes (Master)
#define STATUS_UPDATE_INTERVAL_MS 5000

// Button debounce delay (ms) (Master)
#define BUTTON_DEBOUNCE_MS  500

// Delay before retrying slave scan after incomplete connection (ms)
#define SCAN_RETRY_DELAY_MS 5000

// ─── Number of Slave Units ──────────────────────────────────────────────────
// Maximum number of slave units the master will search for and manage.
#define NUM_SLAVES  2

// ─── Serial / Debug ─────────────────────────────────────────────────────────
// Baud rate for Serial.begin() on both Master and Slave units.
#define SERIAL_BAUD_RATE  115200

// ─── Display Rotation (Master) ──────────────────────────────────────────────
// TFT_eSPI rotation value (0–3). 3 = landscape with USB on the right.
#define TFT_ROTATION  3
