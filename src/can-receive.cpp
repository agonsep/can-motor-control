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
#define STEPS_PER_DEGREE 8.888889  // Adjust based on your motor specs (e.g., 200 steps/rev, 1.8°/step)

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
    
  } else if (message.startsWith("MV ")) {
    // Move command: MV 90 or MV -90
    String moveStr = message.substring(3);
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
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {  
    if (canMsg.can_id == 0x123) {      
      // Copy CAN data to buffer
      for (int i = 0; i < canMsg.can_dlc && i < 16; i++) {
        msgBuffer[i] = (char)canMsg.data[i];
      }
      processCanMessage();
    }       
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