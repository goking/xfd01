/*
  Notifier 
  
  A XFD for jenkins used project.
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

const int PLAY_REPEAT_NUM = 50;

const int HTTP_HEADER = 0;
const int HTTP_MAYBE_BLANKLINE = 1;
const int HTTP_BODY   = 2;

const int BUILD_STABLE = 0;
const int BUILD_UNSTABLE = 1;

// Enter a MAC address and IP address
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const int BUFFER_SIZE = 256;

boolean interrupted = false;

EthernetClient client;
char remoteHost[256];
String baseuri;
int port = 8080;

int lastBuildState = BUILD_STABLE;

void setup() {
  pinMode(SWITCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(SS_PIN, OUTPUT);
  if (!SD.begin(SD_CS_PIN)) {
    digitalWrite(LED_PIN, HIGH);
  }

  Serial.begin(9600);
  
  // start the Ethernet connection:
  while (Ethernet.begin(MAC) == 0) {
    delay(15000);
  }
  
  // wait for dhcp server
  delay(1000);
  
  String rhost = readFile("jenkins.txt");
  int slash = rhost.indexOf("/");
  if (slash != -1) {
    baseuri = rhost.substring(slash);
    rhost = rhost.substring(0, slash); 
  }
  int colon = rhost.indexOf(":");
  if (colon != -1) {
    port = stoi(rhost.substring(colon + 1));
    rhost = rhost.substring(0, colon);
  }
  rhost.toCharArray(remoteHost, sizeof(remoteHost));
  
  attachInterrupt(0, interrupt, FALLING);
}

void loop() {
  
  if (!client.connect(remoteHost, port)) {
    wait(15000, 100);
    return;
  }  
    
  // Make a HTTP request:
  String requestLine = "GET " + baseuri + "/lastBuild/api/json?tree=result HTTP/1.0";
  client.println(requestLine);
  client.println();
  int httpState = HTTP_HEADER;
  boolean lineIsBlank = true;
  String body = "";
  while (client.connected()) {
    // if there are incoming bytes available 
    // from the server, read them and print them:
    if (client.available()) {
      char c = client.read();
      if (httpState == HTTP_HEADER) {
        if (lineIsBlank && c == 0x0D) {
          httpState = HTTP_MAYBE_BLANKLINE;
        } else {
          lineIsBlank = (c == 0x0A);
        }
      } else if (httpState == HTTP_MAYBE_BLANKLINE) {
        httpState = (c == 0x0A) ? HTTP_BODY : HTTP_HEADER; 
      } else if (httpState == HTTP_BODY) {
        body += String(c);
      } else {
        // invalid state
        break;
      }
    } else {
      delay(100); 
    }
  }
  client.stop();
  Serial.println(body);
  if (body.startsWith("{\"result\":\"SUCCESS\"}")) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
    if (lastBuildState == BUILD_UNSTABLE) {
      interrupted = false;
      playClear();
    }
    lastBuildState = BUILD_STABLE;
  } else if (lastBuildState == BUILD_STABLE) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    interrupted = false;
    for (int i = 0; i < PLAY_REPEAT_NUM; ++i) {
      if (!playAlert()) break;
    }
    lastBuildState = BUILD_UNSTABLE;
  } else {
    // nothing to do. 
  }
  
  wait(15000, 2000);
}


String readFile(char* filename) {
  String data;
  
  // re-open the file for reading:
  File file = SD.open(filename);
  if (file) {
    while (file.available() && data.length() < BUFFER_SIZE) { 
      data += (char)file.read();
    }
    // close the file:
    file.close();
  }
  return data;
}

void wait(int duration, int interval) {
  int count = duration / interval;
  for (int i = 0; i < count; ++i) {
    digitalWrite(LED_PIN, HIGH);
    delay(interval / 2);
    digitalWrite(LED_PIN, LOW);
    delay(interval / 2);
  }  
}

int stoi(String str) {
  char buf[20];
  str.toCharArray(buf, sizeof(buf));
  return atoi(buf);
}

void interrupt() {
  interrupted = true;
}
