#include <Arduino.h>
#include <Arduino_CAN.h>

const uint32_t CAN_ID_BASE = 0x123; 
const unsigned long SEND_INTERVAL = 5000; // Send every 2 seconds
unsigned long lastSendTime = 0;
int seqNum = 1;

char message[17]; 
static uint32_t msg_cnt = 0;

// Position tracking
long expectedPosition = 0;  // Current expected position in degrees

// Function to normalize angle to -180 to +180 degree range
long normalizeAngle(long angle) {
  while (angle > 180) {
    angle -= 360;
  }
  while (angle <= -180) {
    angle += 360;
  }
  return angle;
}

// Function to reset position tracking (call this if you manually reset the motor)
void resetPositionTracking() {
  expectedPosition = 0;
  Serial.println("*** Position tracking reset to 0° ***");
}

void generateMessage() {  
  // Define the allowed angles: 30, 45, 90, 180 degrees
  int allowedAngles[] = {30, 45, 90, 180};
  int numAngles = 4;
  
  // Randomly select one of the allowed angles
  int angleIndex = random(numAngles);
  int baseAngle = allowedAngles[angleIndex];
  
  // Randomly choose direction (positive or negative)
  int direction = random(2); // 0 or 1
  int degrees = (direction == 0) ? baseAngle : -baseAngle;
  
  // Update expected position
  long previousPosition = expectedPosition;
  expectedPosition += degrees;
  
  // Normalize position to keep it within -180° to +180° range
  expectedPosition = normalizeAngle(expectedPosition);
  
  // Generate MV command only (keep it short for single CAN frame)
  snprintf(message, sizeof(message), "MV%d", degrees); // Removed space to keep it shorter
  
  Serial.print("Move command: ");
  Serial.print(degrees);
  Serial.print("° | Previous pos: ");
  Serial.print(previousPosition);
  Serial.print("° | New expected pos: ");
  Serial.print(expectedPosition);
  Serial.print("° | ");
}

void sendMessage() {
  generateMessage();
  Serial.print("Generated command: ");
  Serial.println(message);

  // Get message length
  int msgLen = strlen(message);
  
  // Send the complete message in a single CAN frame (up to 8 bytes)
  // If message is longer than 8 bytes, we'll send multiple frames
  int idx = 0;
  int frame = 0;
  
  while (idx < msgLen) {
    int len = min(8, msgLen - idx);
    uint8_t msg_data[8] = {0}; // Initialize all bytes to 0
    
    for (int i = 0; i < len; i++) {
      msg_data[i] = message[idx + i];
    }
    
    CanMsg const msg(CanStandardId(CAN_ID_BASE + frame), len, msg_data);    
    if (int const rc = CAN.write(msg); rc < 0)
    {
      Serial.print("CAN.write(...) failed with error code ");
      Serial.println(rc);
      return; // Exit if send fails
    } else {
      Serial.print("Successfully sent CAN frame ");
      Serial.print(frame);
      Serial.print(" (ID 0x");
      Serial.print(CAN_ID_BASE + frame, HEX);
      Serial.print("): ");
      for (int i = 0; i < len; i++) {
        Serial.print((char)msg_data[i]);
      }
      Serial.println();
    }
    
    idx += len;
    frame++;
  }
  
  msg_cnt++;
  Serial.print("Total messages sent: ");
  Serial.println(msg_cnt);
  Serial.println("---");  
}

void setup() {
  Serial.begin(115200);
  Serial.println("CAN Motor Control Command Transmitter");
  Serial.println("Sending random MV (move) commands only");
  
  if (!CAN.begin(500000)) { // 500 kbps
    Serial.println("Starting CAN failed!");
    while (1);
  }
  
  randomSeed(analogRead(0));
  Serial.println("CAN initialized successfully!");
  Serial.println("Commands will be sent every 2 seconds...");
  Serial.println("MV commands: ±30, ±45, ±90, ±180 degrees only");
  Serial.println("Speed remains constant (set by receiver)");
  Serial.print("Initial motor position assumed to be: ");
  Serial.print(expectedPosition);
  Serial.println("°");
  Serial.println("Position tracking enabled - will show expected position after each move");
  Serial.println("---");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendMessage();
    lastSendTime = currentTime;
  }
}