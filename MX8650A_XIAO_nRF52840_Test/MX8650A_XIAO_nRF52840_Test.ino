/*
  MX8650A + Seeed Studio XIAO minimal motion test.

  Wiring:
    MX8650A pin 3 SDIO -> XIAO D2
    MX8650A pin 4 SCLK -> XIAO D3
    MX8650A pin 6 GND  -> XIAO GND
    MX8650A pin 7 VDD  -> XIAO 3V3

  This sketch bit-bangs the MX8650A two-wire, half-duplex serial protocol.
  No MX8650 Arduino library is required for the default test.
*/

#include <Arduino.h>

// The non-mbed Seeed nRF52 core needs TinyUSB to provide USB Serial.
#if defined(ARDUINO_ARCH_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

const uint8_t PIN_SDIO = D2;
const uint8_t PIN_SCLK = D3;

#if defined(ARDUINO_XIAO_ESP32C3)
const char *BOARD_LABEL = "XIAO ESP32C3";
#elif defined(ARDUINO_SEEED_XIAO_NRF52840) || defined(ARDUINO_SEEED_XIAO_NRF52840_SENSE)
const char *BOARD_LABEL = "XIAO nRF52840";
#else
const char *BOARD_LABEL = "XIAO";
#endif

const bool INVERT_X = false;
const bool INVERT_Y = false;
const bool SWAP_XY = false;
const uint32_t PRINT_INTERVAL_MS = 20;

const bool PRINT_ZERO_DELTAS = true;
const bool PRINT_DEBUG_REGISTERS = true;
const uint32_t DEBUG_INTERVAL_MS = 1000;

// MX8650A registers.
const uint8_t REG_PRODUCT_ID1 = 0x00;
const uint8_t REG_PRODUCT_ID2 = 0x01;
const uint8_t REG_MOTION_STATUS = 0x02;
const uint8_t REG_DELTA_X = 0x03;
const uint8_t REG_DELTA_Y = 0x04;
const uint8_t REG_OPERATION_MODE = 0x05;
const uint8_t REG_CONFIGURATION = 0x06;
const uint8_t REG_IMAGE_QUALITY = 0x07;
const uint8_t REG_OPERATION_STATE = 0x08;

// Operation_Mode: LED shutter enabled, required bits set to 01, sleep disabled.
const uint8_t OPERATION_DISABLE_SLEEP = 0xA0;

// Configuration register values. Bit 2 must stay 1.
const uint8_t CONFIG_800_CPI = 0x04;
const uint8_t CONFIG_1000_CPI = 0x05;
const uint8_t CONFIG_1200_CPI = 0x06;
const uint8_t CONFIG_1600_CPI = 0x07;
const uint8_t CONFIGURATION_VALUE = CONFIG_1200_CPI;

// Conservative timing for digitalWrite based bit-banging.
const uint8_t CLOCK_HALF_PERIOD_US = 4;
const uint8_t READ_SAMPLE_DELAY_US = 1;
const uint8_t POWER_UP_DELAY_MS = 60;
const uint16_t RESYNC_WAIT_MS = 2;

uint32_t lastPrintMs = 0;
uint32_t lastDebugMs = 0;

struct MX8650Motion {
  uint8_t status;
  int8_t dx;
  int8_t dy;
};

void printHex2(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void waitForSerialBriefly() {
  const uint32_t startMs = millis();
  while (!Serial && millis() - startMs < 3000) {
    delay(10);
  }
}

void mx8650ClockOutBit(bool value) {
  digitalWrite(PIN_SCLK, LOW);
  digitalWrite(PIN_SDIO, value ? HIGH : LOW);
  delayMicroseconds(CLOCK_HALF_PERIOD_US);
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(CLOCK_HALF_PERIOD_US);
}

bool mx8650ClockInBit() {
  digitalWrite(PIN_SCLK, LOW);
  delayMicroseconds(CLOCK_HALF_PERIOD_US);
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(READ_SAMPLE_DELAY_US);
  const bool value = digitalRead(PIN_SDIO) == HIGH;
  delayMicroseconds(CLOCK_HALF_PERIOD_US);
  return value;
}

void mx8650WriteByte(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    mx8650ClockOutBit((value >> bit) & 0x01);
  }
}

uint8_t mx8650ReadByte() {
  uint8_t value = 0;
  for (int bit = 7; bit >= 0; --bit) {
    if (mx8650ClockInBit()) {
      value |= (1 << bit);
    }
  }
  return value;
}

void mx8650Resync(uint16_t waitMs = RESYNC_WAIT_MS) {
  pinMode(PIN_SCLK, OUTPUT);
  pinMode(PIN_SDIO, OUTPUT);
  digitalWrite(PIN_SDIO, LOW);
  digitalWrite(PIN_SCLK, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SCLK, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_SCLK, HIGH);
  delay(waitMs);
}

uint8_t mx8650ReadRegister(uint8_t reg) {
  uint8_t value = 0;

  noInterrupts();
  pinMode(PIN_SDIO, OUTPUT);
  digitalWrite(PIN_SDIO, LOW);

  // Read command: bit 7 = 0, bits 6..0 = address.
  mx8650WriteByte(reg & 0x7F);

  // Release SDIO before the next falling edge so the sensor can drive data.
  pinMode(PIN_SDIO, INPUT);
  value = mx8650ReadByte();

  // Do not leave SDIO floating between transactions.
  pinMode(PIN_SDIO, OUTPUT);
  digitalWrite(PIN_SDIO, LOW);
  digitalWrite(PIN_SCLK, HIGH);
  interrupts();

  return value;
}

void mx8650WriteRegister(uint8_t reg, uint8_t value) {
  noInterrupts();
  pinMode(PIN_SDIO, OUTPUT);
  digitalWrite(PIN_SDIO, LOW);

  // Write command: bit 7 = 1, bits 6..0 = address.
  mx8650WriteByte(0x80 | (reg & 0x7F));
  mx8650WriteByte(value);

  digitalWrite(PIN_SDIO, LOW);
  digitalWrite(PIN_SCLK, HIGH);
  interrupts();

  delayMicroseconds(20);
}

void mx8650Begin() {
  pinMode(PIN_SCLK, OUTPUT);
  pinMode(PIN_SDIO, OUTPUT);
  digitalWrite(PIN_SDIO, LOW);
  digitalWrite(PIN_SCLK, HIGH);

  delay(POWER_UP_DELAY_MS);
  mx8650Resync();

  mx8650WriteRegister(REG_OPERATION_MODE, OPERATION_DISABLE_SLEEP);
  delay(3);
  mx8650WriteRegister(REG_CONFIGURATION, CONFIGURATION_VALUE);
  delay(3);

  // Clear any startup deltas.
  (void)mx8650ReadRegister(REG_MOTION_STATUS);
  (void)mx8650ReadRegister(REG_DELTA_X);
  (void)mx8650ReadRegister(REG_DELTA_Y);
}

bool mx8650ProductIdLooksOk(uint8_t pid1, uint8_t pid2) {
  return pid1 == 0x30 && (pid2 & 0xF0) == 0x50;
}

MX8650Motion mx8650ReadMotion() {
  MX8650Motion motion;

  // Read Motion_Status first. This freezes Delta_X/Delta_Y until both are read.
  motion.status = mx8650ReadRegister(REG_MOTION_STATUS);
  motion.dx = (int8_t)mx8650ReadRegister(REG_DELTA_X);
  motion.dy = (int8_t)mx8650ReadRegister(REG_DELTA_Y);

  return motion;
}

void applyAxisOptions(int8_t rawDx, int8_t rawDy, int16_t &dx, int16_t &dy) {
  dx = rawDx;
  dy = rawDy;

  if (SWAP_XY) {
    const int16_t tmp = dx;
    dx = dy;
    dy = tmp;
  }
  if (INVERT_X) {
    dx = -dx;
  }
  if (INVERT_Y) {
    dy = -dy;
  }
}

void printStartupReport() {
  const uint8_t pid1 = mx8650ReadRegister(REG_PRODUCT_ID1);
  const uint8_t pid2 = mx8650ReadRegister(REG_PRODUCT_ID2);
  const uint8_t opMode = mx8650ReadRegister(REG_OPERATION_MODE);
  const uint8_t config = mx8650ReadRegister(REG_CONFIGURATION);
  const uint8_t imageQuality = mx8650ReadRegister(REG_IMAGE_QUALITY);
  const uint8_t opState = mx8650ReadRegister(REG_OPERATION_STATE);

  Serial.print("MX8650A ");
  Serial.print(BOARD_LABEL);
  Serial.println(" trackball test");
  Serial.print("sdio_pin=");
  Serial.print((int)PIN_SDIO);
  Serial.print(", sclk_pin=");
  Serial.println((int)PIN_SCLK);

  Serial.print("pid1=0x");
  printHex2(pid1);
  Serial.print(", pid2=0x");
  printHex2(pid2);
  Serial.print(", op_mode=0x");
  printHex2(opMode);
  Serial.print(", config=0x");
  printHex2(config);
  Serial.print(", image_quality=");
  Serial.print(imageQuality);
  Serial.print(", op_state=0x");
  printHex2(opState);
  Serial.println();

  if (!mx8650ProductIdLooksOk(pid1, pid2)) {
    Serial.println("WARN: Product ID mismatch. Expected pid1=0x30 and pid2 high nibble=0x5.");
    Serial.println("WARN: Check SDIO/SCLK wiring, 3V3, GND, and whether the old mouse MCU is still driving the bus.");
  }
}

void printMotionLine(const MX8650Motion &motion) {
  int16_t dx = 0;
  int16_t dy = 0;
  applyAxisOptions(motion.dx, motion.dy, dx, dy);

  if (!PRINT_ZERO_DELTAS && dx == 0 && dy == 0 && (motion.status & 0x18) == 0) {
    return;
  }

  Serial.print("dx=");
  Serial.print(dx);
  Serial.print(", dy=");
  Serial.print(dy);

  if (motion.status & 0x18) {
    Serial.print(", status=0x");
    printHex2(motion.status);
    if (motion.status & 0x08) {
      Serial.print(", x_overflow");
    }
    if (motion.status & 0x10) {
      Serial.print(", y_overflow");
    }
  }

  Serial.println();
}

void printDebugReport() {
  const uint8_t pid1 = mx8650ReadRegister(REG_PRODUCT_ID1);
  const uint8_t pid2 = mx8650ReadRegister(REG_PRODUCT_ID2);
  const uint8_t imageQuality = mx8650ReadRegister(REG_IMAGE_QUALITY);
  const uint8_t opState = mx8650ReadRegister(REG_OPERATION_STATE);
  const uint8_t config = mx8650ReadRegister(REG_CONFIGURATION);

  Serial.print("debug pid1=0x");
  printHex2(pid1);
  Serial.print(", pid2=0x");
  printHex2(pid2);
  Serial.print(", image_quality=");
  Serial.print(imageQuality);
  Serial.print(", op_state=0x");
  printHex2(opState);
  Serial.print(", config=0x");
  printHex2(config);

  if (!mx8650ProductIdLooksOk(pid1, pid2)) {
    Serial.print(", pid_ng");
    mx8650Resync();
  }

  Serial.println();
}

void setup() {
  Serial.begin(115200);
  waitForSerialBriefly();

  mx8650Begin();
  printStartupReport();
}

void loop() {
  const uint32_t nowMs = millis();

  if (nowMs - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = nowMs;
    const MX8650Motion motion = mx8650ReadMotion();
    printMotionLine(motion);
  }

  if (PRINT_DEBUG_REGISTERS && nowMs - lastDebugMs >= DEBUG_INTERVAL_MS) {
    lastDebugMs = nowMs;
    printDebugReport();
  }
}
