/*
  Notifier
  
  Turns a read relay and sounds on when recive http request.
  
*/

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

// Pin assign
const int LED_PIN     = 9;
const int SWITCH_PIN  = 7;
const int BUZZER_PIN  = 6;
const int RELAY_PIN   = 5;
const int SD_CS_PIN   = 4;

// Enter a MAC address and IP address
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte IP[4];

const int REQUEST_LINE_SIZE = 100;

File ipFile;
Server server(80);

void setup() {
  pinMode(SWITCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.begin(9600);
  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  pinMode(SS_PIN, OUTPUT);
   
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
    
  // re-open the file for reading:
  ipFile = SD.open("test.txt");
  if (ipFile) {
    Serial.println("test.txt:");
    
    // read from the file until there's nothing else in it:
    for (int i = 0; i < sizeof(IP); i++) {
      if (!ipFile.available()) {
         Serial.println("file is too short.");
         break;
      }
      IP[i] = ipFile.read();
    }
    // close the file:
    ipFile.close();
  } else {
  	// if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  
  Ethernet.begin(MAC, IP);
  server.begin();
}

void loop() {
  // listen for incoming clients
  Client client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    boolean isRequestLine = true;
    boolean requestIsNotify = false;
    char requestLine[REQUEST_LINE_SIZE];
    int requestLineIndex = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println();
          client.println("start sound & signal");
          client.print("req: ");
          client.println(requestLine);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          if (isRequestLine) {
            requestLine[requestLineIndex] = 0;
            String req(requestLine);
            if (req.startsWith("GET / ")) {
              requestIsNotify = true;
            }
          }
          isRequestLine = false;
          digitalWrite(LED_PIN, LOW);
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
          if (isRequestLine && requestLineIndex < (sizeof(requestLine) - 1)) {
            requestLine[requestLineIndex++] = c;
          }
          digitalWrite(LED_PIN, HIGH);
        }
      }
    }
    // give the web browser time to receive the data
    delay(5);
    // close the connection:
    client.stop();
    
    digitalWrite(LED_PIN, LOW);
    if (requestIsNotify) {
      digitalWrite(RELAY_PIN, HIGH);
      resetSwitchState();
      while (soundTone());
      digitalWrite(RELAY_PIN, LOW);
    }
  }
}
