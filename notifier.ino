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
//    Serial.println("DHCP error");
    // no point in carrying on, so do nothing forevermore:
    delay(15000);
  }
  
  //print your local IP address:
//  Serial.print("myaddr: ");
//  Serial.println(Ethernet.localIP());
  
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
//  Serial.println("remote host: " + rhost);
//  Serial.print("remote port: ");
//  Serial.println(port, DEC);
  rhost.toCharArray(remoteHost, sizeof(remoteHost));
  
  attachInterrupt(0, interrupt, FALLING);
}

void loop() {
  
  if (!client.connect(remoteHost, port)) {
    // kf you didn't get a connection to the server:
    wait(15000, 100);
    return;
  }  
    
  // Make a HTTP request:
  String requestLine = "GET " + baseuri + "/lastBuild/api/json?tree=result HTTP/1.0";
  Serial.println(requestLine);
  client.println(requestLine);
  client.println();
//    Serial.println("wait for response");
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
//          Serial.println("invalid state");
        break;
      }
    } else {
      delay(100); 
    }
  }
  client.stop();
  Serial.println(body);
  if (body.startsWith("{\"result\":\"SUCCESS\"}")) {
//    Serial.println("stable");
    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
    if (lastBuildState == BUILD_UNSTABLE) {
      interrupted = false;
      playClear();
    }
    lastBuildState = BUILD_STABLE;
  } else if (lastBuildState == BUILD_STABLE) {
//    Serial.println("fall unstable");
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    interrupted = false;
    for (int i = 0; i < PLAY_REPEAT_NUM; ++i) {
      if (!playAlert()) break;
    }
    lastBuildState = BUILD_UNSTABLE;
  } else {
//    Serial.println("still unstable");
    // nothing to do. 
  }
  
  wait(15000, 2000);
}


/*
int processSmtp(Client* client) {
  boolean currentLineMayBeSubject = true;
  boolean readingHeader = true;
  boolean currentLineIsBlank = false;
  int result = NOP;
  String linebuf;
  int lineindex = 0;
  char c;
  int smtpState = STATE_PREAMBLE;
  while (client->connected() && smtpState != STATE_QUIT) {
    if (!client->available()) {
      delay(1);
      continue;
    }
    c = client->read();
    switch (smtpState) {
      case STATE_PREAMBLE:
        linebuf += String(c);
        if (linebuf.endsWith("\r\n")) {
          smtpState = sendResponseFor(client, &linebuf);
        }
        break;
      case STATE_HEADER_MAYBE_SUBJECT:
        linebuf += String(c);
        if (HEADER_SUBJECT.startsWith(linebuf)) {
          if (linebuf.length() == HEADER_SUBJECT.length()) {
            String subject = parseHeaderValue(client);
            if (subject.startsWith(_activateText)) {
              result = ACTIVE;
            } else if (subject.startsWith(_inactivateText)) {
              result = INACTIVE;
            }
            Serial.print("Subject: ");
            Serial.println(subject);
            Serial.println(result, DEC);
            smtpState = STATE_FIND_BODYEND;
          }
        } else if (linebuf.equals("\r")) {
          smtpState = STATE_FIND_BODYEND;
        } else if (linebuf.equals(".")) {
          smtpState = STATE_MAYBE_BODYEND;
        } else {
          smtpState = STATE_SKIP_HEADER;
        }
        break;
      case STATE_SKIP_HEADER:
        if (c == '\n') {
          smtpState = STATE_HEADER_MAYBE_SUBJECT; 
        }
        break;
      case STATE_FIND_BODYEND:
        if (currentLineIsBlank && c == '.') {
          linebuf = String(c);
          smtpState = STATE_MAYBE_BODYEND;
        }
        break;
      case STATE_MAYBE_BODYEND:
        linebuf += String(c);
        if (linebuf.equals(".\r\n")) {
          client->println("250 Ok");
          smtpState = STATE_PREAMBLE;
        } else if (!linebuf.equals(".\r")) {
          smtpState = STATE_FIND_BODYEND;
        }
        break;
    }
    if (c == '\n') {
      linebuf = String();
      currentLineIsBlank = true;
    } else {
      currentLineIsBlank = false;
    }
  }
  return result;
}

// 
int sendResponseFor(Client* client, String* line) {
  int nextState = STATE_PREAMBLE;
  Serial.print(*line);
  if (line->startsWith(SMTP_HELO) || line->startsWith(SMTP_EHLO)) {
    client->print("250 ");
    client->println(_fqdn);
  } else if (line->startsWith(SMTP_MAIL)) {
    client->println("250 Ok");
  } else if (line->startsWith(SMTP_RCPT)) {
    client->println("250 Ok");
  } else if (line->startsWith(SMTP_DATA)) {
    client->println("354 Send it");
    nextState = STATE_HEADER_MAYBE_SUBJECT;
  } else if (line->startsWith(SMTP_QUIT)) {
    client->println("221 Ok");
    nextState = STATE_QUIT;
  }
  return nextState;
}

const String Q_ENCODE_HEAD = "=?UTF-8?Q?";
const String Q_ENCODE_TAIL = "?=";

String parseHeaderValue(Client* client) {
  String raw;
  while (client->connected() && client->available()) {
    char c = client->read();
    if (c == '\n') break;
    if (c != '\r') {
      raw += String(c);
    }
  }
  String value;
  int index = 0;
  while (true) {
    int head = raw.indexOf(Q_ENCODE_HEAD, index);
    if (head == -1) {
      if (index == 0) {
        value = raw;
      }
      break;
    }
    index = head + Q_ENCODE_HEAD.length();
    int tail = raw.indexOf(Q_ENCODE_TAIL, index);
    while (index < tail) {
      if (raw.charAt(index) == '=') {
        char buf[3];
        int v;
        String hex = raw.substring(index + 1, index + 3);
        hex.toCharArray(buf, 3);
        sscanf(buf, "%x", &v);
        value += String((char)v);
        index += 4;
      } else {
        value += String(raw.charAt(index++));
      }   
    }
  }
  return value;
}

*/

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
