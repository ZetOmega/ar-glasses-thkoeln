#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

// Constants for the display screen dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Width to use for text display (exclude margins)
#define USABLE_SCREEN_WIDTH 64
#define MARGIN 32

// Define the display instance
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Initialize the Bluetooth Serial
BluetoothSerial ESP_BT;

// Define global variables for scroll speed, pause status, and clear status
int incoming = 0;
int scrollSpeed = 100;
bool isPaused = false;
bool shouldClear = false;

// Function prototypes
std::vector<String> splitTextIntoLines(String text, int charsPerLine);
void centerText(String text, int y);
void centeredPrint(String input);
String bluetoothReadLine();
void togglePause();
void clearDisplay();
void teleprompter(String text);
void handlePrompterCommand();
void updateScrollSpeed(int newSpeed);

/**
 * Center text on the screen within the usable width.
 * @param text: The text to be centered.
 * @param y: Y-coordinate for the text.
 */
void centerText(String text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - w) / 2; 
  if (x < MARGIN) x = MARGIN;
  display.setCursor(x, y);
  display.println(text);
}

/**
 * Function to print centered text on the screen.
 * @param input: String to be displayed.
 */
void centeredPrint(String input) {
  display.clearDisplay();
  const int charsPerLine = USABLE_SCREEN_WIDTH / 6; // Calculate maximum characters per line
  std::vector<String> lines = splitTextIntoLines(input, charsPerLine); // Split text into lines

  int y = (SCREEN_HEIGHT - lines.size() * 8) / 2; // Center vertically
  for (const String &line : lines) {
    centerText(line, y);
    y += 8; // Move to next line
  }
  display.display();
  Serial.println("Text '" + input + "' displayed");
}

/**
 * Setup function initializes serial communication, Bluetooth, and the display.
 */
void setup() {
  Serial.begin(115200);
  ESP_BT.begin("ESP32_Control");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);  // Infinite loop to signal failure
  }

  display.clearDisplay();
  display.setTextSize(1.1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.display();

  Serial.println("Setup complete");

  // Wait for Bluetooth connection
  while (!ESP_BT.hasClient()) {
    display.clearDisplay();
    centeredPrint("Waiting for connection...");
    display.display();
    delay(1000);  // Poll every second
  }

  // Display successfully connected message
  centeredPrint("Successfully Connected");
  Serial.println("Phone connected successfully.");
  delay(2000);  // Display message for 2 seconds
}

/**
 * Function to read a line of text from Bluetooth input.
 * @return: String received from Bluetooth.
 */
String bluetoothReadLine() {
  String text_received = "";
  while (ESP_BT.available()) {
    byte r = ESP_BT.read();
    if (r != 13 && r != 10 && char(r) != '\0')
      text_received += char(r);
  }
  return text_received;
}

/**
 * Function to toggle the pause status.
 */
void togglePause() {
  isPaused = !isPaused;
  Serial.print("Paused: ");
  Serial.println(isPaused ? "true" : "false");
}

/**
 * Function to clear the display and reset states.
 */
void clearDisplay() {
  shouldClear = true;
  isPaused = false;
  display.clearDisplay();
  display.display();
  Serial.println("Display cleared");
}

/**
 * Function to split text into lines that fit within the display width.
 * This function ensures that words are not cut off and stay within the usable width.
 * @param text: The text to be split.
 * @param charsPerLine: Maximum number of characters per line.
 * @return: Vector of strings, each representing a line.
 */
std::vector<String> splitTextIntoLines(String text, int charsPerLine) {
  std::vector<String> lines;
  String currentLine;
  String currentWord;

  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == ' ' || c == '\n') {
      if (currentLine.length() + currentWord.length() + 1 <= charsPerLine) {
        currentLine += currentWord + " ";
      } else {
        if (currentLine.length() > 0) {
          lines.push_back(currentLine);
        }
        currentLine = currentWord + " ";
      }
      currentWord = "";
    } else {
      currentWord += c;
    }
  }

  if (currentLine.length() + currentWord.length() <= charsPerLine) {
    currentLine += currentWord;
  } else {
    if (currentLine.length() > 0) {
      lines.push_back(currentLine);
    }
    currentLine = currentWord;
  }

  if (!currentLine.isEmpty()) {
    lines.push_back(currentLine);
  }

  return lines;
}

/**
 * Function to handle the teleprompter functionality.
 * @param text: The text to be displayed by the teleprompter.
 */
void teleprompter(String text) {
  display.clearDisplay();

  int textPixelHeight = 8;
  int charsPerLine = USABLE_SCREEN_WIDTH / 6;

  std::vector<String> lines = splitTextIntoLines(text, charsPerLine);

  int totalHeight = lines.size() * textPixelHeight;
  int pos = SCREEN_HEIGHT;

  shouldClear = false;  // Reset the clear flag

  Serial.println("Starting teleprompter...");

  // Displaying initial wait text
  centeredPrint("Please wait...");
  delay(2000);  // Wait for 2 seconds

  // Loop to handle scrolling
  while (pos > -totalHeight && !shouldClear) {
    display.clearDisplay();
    for (int i = 0; i < lines.size(); i++) {
      display.setCursor(MARGIN, pos + i * textPixelHeight);
      display.print(lines[i]);

      // Check for incoming commands after printing each line
      if (ESP_BT.available()) {
        incoming = ESP_BT.read();
        handlePrompterCommand();
      }
      if (shouldClear) {
        display.clearDisplay();
        display.display();
        Serial.println("Teleprompter cleared");
        return;
      }

      while (isPaused && !shouldClear) {
        delay(100);  // Poll every 100ms during pause
        if (ESP_BT.available()) {
          incoming = ESP_BT.read();
          handlePrompterCommand();
        }
      }
    }

    display.display();
    delay(scrollSpeed);
    pos -= 1;

    // Check for incoming commands
    if (ESP_BT.available()) {
      incoming = ESP_BT.read();
      handlePrompterCommand();
    }

    // Check if the clear command was issued during scrolling
    if (shouldClear) {
      display.clearDisplay();
      display.display();
      Serial.println("Teleprompter cleared");
      return;
    }
  }

  if (shouldClear) {
    Serial.println("Teleprompter cleared");
  } else {
    Serial.println("Teleprompter finished");
  }
}

/**
 * Function to handle commands received during the teleprompter session.
 */
void handlePrompterCommand() {
  if (incoming >= 12 && incoming < 20) {
    switch (incoming) {
      case 12:
        togglePause();
        break;
      case 13:
        updateScrollSpeed(scrollSpeed + 10); // Update command for increasing speed by 10
        break;
      case 14:
        updateScrollSpeed(scrollSpeed - 10); // Update command for decreasing speed by 10
        break;
      case 15:
        clearDisplay();
        break;
    }
    Serial.print("Command received: ");
    Serial.println(incoming);
  }
}

/**
 * Function to update the scroll speed.
 * @param newSpeed: The new scroll speed value.
 */
void updateScrollSpeed(int newSpeed) {
  if (newSpeed < 10) {
    newSpeed = 10;
  } else if (newSpeed > 500) {
    newSpeed = 500;
  }
  scrollSpeed = newSpeed;
  Serial.print("Scroll speed updated to: ");
  Serial.println(scrollSpeed);
}

/**
 * Main loop function to handle incoming Bluetooth commands and perform actions accordingly.
 */
void loop() {
  if (ESP_BT.available()) {
    incoming = ESP_BT.read();

    if (incoming < 10) {
      Serial.print("Button: ");
      Serial.println(incoming);
      switch (incoming) {
        case 1:
          centeredPrint("Buch");
          break;
        case 2:
          centeredPrint("Baum");
          break;
        case 3:
          centeredPrint("Banane");
          break;
      }
    } else if (incoming == 11) {
      Serial.println("Starting Teleprompter...");
      teleprompter(bluetoothReadLine());
      incoming = 0;
    } else if (incoming >= 12 && incoming < 20) {
      handlePrompterCommand();
    } else {
      Serial.println("Unknown command.");
    }
  }
}