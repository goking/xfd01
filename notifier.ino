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
IPAddress server;
int port;

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
    Serial.println("Failed to configure Ethernet using DHCP. Try again...");
    // no point in carrying on, so do nothing forevermore:
    delay(15000);
  }
  
  // print your local IP address:
  Serial.print("IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();
  
  delay(1000);
  
  server = IPAddress(133,242,22,208);
  port = 8080;
  
/*
  readFile("ipaddr.bin").getBytes(IP, 5);
  Serial.println(IP[0],DEC);
  Serial.println(IP[1],DEC);
  Serial.println(IP[2],DEC);
  Serial.println(IP[3],DEC);

  Ethernet.begin(MAC, IP);
  server.begin();
  
  Serial.println("SD CARD!");
  _fqdn = readFile("fqdn.txt");
  Serial.println(_fqdn);
  _activateText = readFile("on.txt");
  Serial.println(_activateText);
  _inactivateText = readFile("off.txt");
  Serial.println(_inactivateText);
*/

  attachInterrupt(0, interrupt, FALLING);
}

void loop() {
  
  if (client.connect(server, port)) {
    Serial.println("connected");
    // Make a HTTP request:
    client.println("GET /jenkins/job/hiyoko/lastBuild/api/json?tree=result HTTP/1.0");
    client.println();
    Serial.println("request done. wait for response.");
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
          Serial.println("invalid state");
          break;
        }
      } else {
        delay(100); 
      }
    }
    client.stop();
    Serial.println(body);
    if (body.startsWith("{\"result\":\"SUCCESS\"}")) {
      Serial.println("stable");
      digitalWrite(LED_PIN, LOW);
      digitalWrite(RELAY_PIN, LOW);
      if (lastBuildState == BUILD_UNSTABLE) {
        interrupted = false;
        playClear();
      }
      lastBuildState = BUILD_STABLE;
    } else if (lastBuildState == BUILD_STABLE) {
      Serial.println("turn into unstable");
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(RELAY_PIN, HIGH);
      interrupted = false;
      for (int i = 0; i < PLAY_REPEAT_NUM; ++i) {
        boolean completed = playAlert();
        if (!completed) break;
      }
      lastBuildState = BUILD_UNSTABLE;
    } else {
      Serial.println("still unstable");
      // nothing to do. 
    }
    Serial.println("connection completed");
  } else {
    // kf you didn't get a connection to the server:
    Serial.println("connection failed");
    return;
  }  
  
  delay(15000);
 
/* 
  while (!server.established()) {
    delay(5);
  }  
  server.write("220 arduino smtp server\r\n");
  // listen for incoming clients 
  int result = NOP;
  while(server.established()) {
    Client client = server.available();
    if (client) { 
      result = processSmtp(&client);
      // give the web browser time to receive the data
      delay(5);
      // close the connection:
      client.stop();
      break;
    }
  }
  
*/
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

String readFile(char* filename) {
  String data;
  
  // re-open the file for reading:
  File file = SD.open(filename);
  if (file) {    
    // read from the file until there's nothing else in it:
    char buf[32];
    int pos = 0;
    while (file.available()) { 
      buf[pos++] = (char)file.read();
      if (pos >= 31 || !file.available()) {
        buf[pos] = 0;
        data += String(buf);
        pos = 0;
      }
    }
    // close the file:
    file.close();
  }
  return data;
}

*/

void interrupt() {
  interrupted = true;
}
