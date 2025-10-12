#include <Arduino.h>
#include <Arduino_CAN.h>

const uint32_t CAN_ID_BASE = 0x123; 
const unsigned long SEND_INTERVAL = 2000; // Send every 2 seconds
unsigned long lastSendTime = 0;
int seqNum = 1;

char message[17]; 
static uint32_t msg_cnt = 0;

void generateMessage() {  
  // Randomly choose between MV (move) and SP (speed) commands
  int commandType = random(2); // 0 or 1
  
  if (commandType == 0) {
    // Generate MV command with random degrees (-180 to 180)
    int degrees = random(-180, 181); // -180 to 180 degrees
    snprintf(message, sizeof(message), "MV %d", degrees);
  } else {
    // Generate SP command with random speed (50 to 500)
    int speed = random(50, 501); // 50 to 500 steps per second
    snprintf(message, sizeof(message), "SP %d", speed);
  }
  
  Serial.print("Command type: ");
  Serial.print(commandType == 0 ? "MOVE" : "SPEED");
  Serial.print(" | ");
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
  Serial.println("Sending random MV (move) and SP (speed) commands");
  
  if (!CAN.begin(500000)) { // 500 kbps
    Serial.println("Starting CAN failed!");
    while (1);
  }
  
  randomSeed(analogRead(0));
  Serial.println("CAN initialized successfully!");
  Serial.println("Commands will be sent every 2 seconds...");
  Serial.println("MV commands: -180 to 180 degrees");
  Serial.println("SP commands: 50 to 500 steps/second");
  Serial.println("---");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendMessage();
    lastSendTime = currentTime;
  }
}