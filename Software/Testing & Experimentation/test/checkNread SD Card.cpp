#include <SPI.h>
#include <SD.h>

#define SD_CS    48
#define SD_MOSI  47
#define SD_CLK   21
#define SD_MISO  14

SPIClass spiSD(FSPI);

// File browser
#define MAX_FILES 100
String fileNames[MAX_FILES];
int fileCount = 0;
int selectedIndex = 0;

// Input debounce
unsigned long lastActionTime = 0;
const unsigned long debounceDelay = 500;

// Escape sequence handling
int escapeState = 0; // 0 = normal, 1 = got ESC, 2 = got [
char escBuffer[3];   // holds ESC [ A or ESC [ B

void listFilesToArray(File dir) {
  fileCount = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry || fileCount >= MAX_FILES) break;
    if (!entry.isDirectory()) {
      fileNames[fileCount++] = String(entry.name());
    }
    entry.close();
  }
}

void printMenu() {
  Serial.println("\n===== File Browser =====");
  for (int i = 0; i < fileCount; i++) {
    if (i == selectedIndex) Serial.print("> ");
    else Serial.print("  ");
    Serial.println(fileNames[i]);
  }
  Serial.println("\nControls: [↑/↓] Navigate  [Enter] Read  [Space] Diagnostics");
}

void readAndPrintFile(const String& filename) {
  File file = SD.open("/" + filename);
  if (!file) {
    Serial.println("❌ Failed to open file.");
    return;
  }

  Serial.printf("📖 Reading %s:\n", filename.c_str());
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
  Serial.println("\n===== End of File =====");
}

void runDiagnostics() {
  Serial.println("\n===== Running SD Diagnostics =====");

  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("❌ SD card initialization failed!");
    return;
  }

  Serial.println("✅ SD card initialized successfully.");

  uint8_t cardType = SD.cardType();
  Serial.print("📦 Card Type: ");
  switch (cardType) {
    case CARD_MMC: Serial.println("MMC"); break;
    case CARD_SD: Serial.println("SDSC"); break;
    case CARD_SDHC: Serial.println("SDHC/SDXC"); break;
    default: Serial.println("Unknown");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("💾 Card Size: %llu MB\n", cardSize);

  uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
  uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);

  Serial.printf("📂 Total space: %llu MB\n", totalBytes);
  Serial.printf("📁 Used space: %llu MB\n", usedBytes);

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("❌ Failed to open root directory.");
    return;
  }

  listFilesToArray(root);
  selectedIndex = 0;
  printMenu();
  root.close();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Initializing SPI...");
  spiSD.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  runDiagnostics();
}

void loop() {
  while (Serial.available()) {
    char input = Serial.read();
    unsigned long now = millis();

    if (escapeState == 0) {
      if (input == 27) { // ESC
        escapeState = 1;
      } else if (input == '\n' || input == '\r') {
        if (now - lastActionTime >= debounceDelay) {
          lastActionTime = now;
          if (fileCount > 0) readAndPrintFile(fileNames[selectedIndex]);
        }
      } else if (input == ' ') {
        if (now - lastActionTime >= debounceDelay) {
          lastActionTime = now;
          runDiagnostics();
        }
      }
    } else if (escapeState == 1) {
      if (input == '[') {
        escapeState = 2;
      } else {
        escapeState = 0;
      }
    } else if (escapeState == 2) {
      escapeState = 0;
      if (now - lastActionTime < debounceDelay) return;
      lastActionTime = now;

      if (input == 'A') {
        // Up arrow
        selectedIndex = (selectedIndex - 1 + fileCount) % fileCount;
        printMenu();
      } else if (input == 'B') {
        // Down arrow
        selectedIndex = (selectedIndex + 1) % fileCount;
        printMenu();
      }
    }
  }
}
