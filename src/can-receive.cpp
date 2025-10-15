#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include <AccelStepper.h>

// CAN setup
struct can_frame canMsg;
MCP2515 mcp2515(5); 
char msgBuffer[17]; 

// Stepper motor setup
#define STEP_PIN 3
#define DIR_PIN 4
// STEPS_PER_DEGREE calculation examples:
// - 200-step motor (1.8°/step): 200 ÷ 360 = 0.5556 steps/degree
// - 200-step motor with 1/16 microstepping: (200 × 16) ÷ 360 = 8.8889 steps/degree  
// - 400-step motor (0.9°/step): 400 ÷ 360 = 1.1111 steps/degree
// 
// TMC2208 Microstepping Settings (MS1/MS2 pins):
// MS2=LOW,  MS1=LOW  -> 1/8  microstepping -> 4.4444 steps/degree
// MS2=HIGH, MS1=LOW  -> 1/32 microstepping -> 17.7778 steps/degree  
// MS2=LOW,  MS1=HIGH -> 1/64 microstepping -> 35.5556 steps/degree
// MS2=HIGH, MS1=HIGH -> 1/16 microstepping -> 8.8889 steps/degree
#define STEPS_PER_DEGREE 4.4444  // Currently set for NO microstepping (full steps only)

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Motor control variables
float currentSpeed = 100.0;  // Default speed in steps per second
bool motorRunning = false;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 CAN Receiver with AccelStepper");

  // Initialize CAN
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  // Initialize stepper
  stepper.setMaxSpeed(1000.0);      // Maximum speed in steps per second
  stepper.setAcceleration(500.0);   // Acceleration in steps per second^2
  stepper.setSpeed(currentSpeed);   // Set initial speed

  Serial.println("MCP2515 and AccelStepper Initialized");
  Serial.println("Waiting for CAN frames...");
  Serial.println("Commands: SP <speed> (set speed), MV <degrees> (move degrees)");
}

void processCanMessage() {
  // Null-terminate the message
  msgBuffer[canMsg.can_dlc] = '\0';
  
  Serial.print("Received: ");
  Serial.println(msgBuffer);
  
  String message = String(msgBuffer);
  message.trim();
  
  if (message.startsWith("SP ")) {
    // Set Speed command: SP 500
    String speedStr = message.substring(3);
    float newSpeed = speedStr.toFloat();
    
    if (newSpeed > 0 && newSpeed <= 1000) {
      currentSpeed = newSpeed;
      stepper.setSpeed(currentSpeed);
      Serial.print("Speed set to: ");
      Serial.print(currentSpeed);
      Serial.println(" steps/sec");
    } else {
      Serial.println("Error: Speed must be between 1 and 1000");
    }
    
  } else if (message.startsWith("MV")) {
    // Move command: MV90 or MV-90 (no space)
    String moveStr = message.substring(2);
    float degrees = moveStr.toFloat();
    
    // Convert degrees to steps
    long steps = (long)(degrees * STEPS_PER_DEGREE);
    
    // Move relative to current position
    stepper.move(steps);
    motorRunning = true;
    
    Serial.print("Moving ");
    Serial.print(degrees);
    Serial.print(" degrees (");
    Serial.print(steps);
    Serial.println(" steps)");
    
  } else {
    Serial.println("Unknown command. Use SP <speed> or MV <degrees>");
  }
}

void loop() {
  // Check for CAN messages
  MCP2515::ERROR result = mcp2515.readMessage(&canMsg);
  if (result == MCP2515::ERROR_OK) {  
    // Accept messages from 0x123 to 0x126 (for multi-frame support)
    if (canMsg.can_id >= 0x123 && canMsg.can_id <= 0x126) {      
      // For single frame messages (ID 0x123), process directly
      if (canMsg.can_id == 0x123) {
        // Copy CAN data to buffer
        for (int i = 0; i < canMsg.can_dlc && i < 16; i++) {
          msgBuffer[i] = (char)canMsg.data[i];
        }
        processCanMessage();
      }
      // For multi-frame messages, we could implement frame assembly here
      // But for now, most MV commands should fit in single frame
    }       
  } else if (result != MCP2515::ERROR_NOMSG) {
    // Only report actual errors, not "no message" status
    Serial.print("CAN read error: ");
    Serial.println(result);
  }
  
  // Run stepper motor
  if (motorRunning) {
    if (stepper.distanceToGo() == 0) {
      motorRunning = false;
      Serial.println("Move completed");
    } else {
      stepper.run();
    }
  }
}