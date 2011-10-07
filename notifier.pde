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

const int PLAY_REPEAT_NUM = 10;

const int STATE_PREAMBLE             = 0;
const int STATE_HEADER_MAYBE_SUBJECT = 1;
const int STATE_SKIP_HEADER          = 2;
const int STATE_FIND_BODYEND         = 3;
const int STATE_MAYBE_BODYEND        = 4;
const int STATE_QUIT                 = 5;

const int ACTIVE = 1;
const int INACTIVE = 0;
const int NOP = -1;

const String SMTP_HELO = "HELO";
const String SMTP_MAIL = "MAIL";
const String SMTP_RCPT = "RCPT";
const String SMTP_DATA = "DATA";
const String SMTP_QUIT = "QUIT";

const String HEADER_SUBJECT = "Subject: ";

// Enter a MAC address and IP address
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte IP[5]; // 4 bytes + String Terminator

String _fqdn, _activateText, _inactivateText;
int _state = INACTIVE;

const int BUFFER_SIZE = 256;

Server server(25);

boolean interrupted = false;

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

  readFile("ipaddr.bin").getBytes(IP, 5);
  
  Ethernet.begin(MAC, IP);
  server.begin();
  
  Serial.begin(9600);
  Serial.println("SD CARD!");
  _fqdn = readFile("fqdn.txt");
  Serial.println(_fqdn);
  _activateText = readFile("on.txt");
  Serial.println(_activateText);
  _inactivateText = readFile("off.txt");
  Serial.println(_inactivateText);

  attachInterrupt(0, interrupt, FALLING);
}

void loop() {
  // listen for incoming clients
  Client client = server.available();
  if (client) {
    int result = processSmtp(&client);
    // give the web browser time to receive the data
    delay(5);
    // close the connection:
    client.stop();
    
    if (result == ACTIVE) {
      if (_state == INACTIVE) {
        interrupted = false;
        digitalWrite(LED_PIN, HIGH);
        /*
        digitalWrite(RELAY_PIN, HIGH);
        for (int i = 0; i < PLAY_REPEAT_NUM; ++i) {
          boolean completed = playTone();
          if (!completed) break; 
        }
        */
      }
      _state = ACTIVE;
    } else if (result == INACTIVE) {
      digitalWrite(LED_PIN, LOW);
      //digitalWrite(RELAY_PIN, LOW);
      _state = INACTIVE;
    }
  }
}

int processSmtp(Client* client) {
  boolean currentLineMayBeSubject = true;
  boolean readingHeader = true;
  boolean requestIsNotify = false;
  int result = NOP;
  String linebuf;
  int lineindex = 0;
  char c;
  if (client->connected()) {
    client->println("220 arduino smtp server");
  }
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
        if (linebuf.length() == 0 && c == '.') {
          linebuf = String(c);
          smtpState = STATE_MAYBE_BODYEND;
        }
      case STATE_MAYBE_BODYEND:
        linebuf += String(c);
        if (linebuf.equals(".\r\n")) {
          smtpState = STATE_PREAMBLE;
        } else if (!linebuf.equals(".\r")) {
          smtpState = STATE_FIND_BODYEND;
        }
        break;
    }
    if (c == '\n') {
      linebuf = String(); 
    }
  }
  return result;
}

// 
int sendResponseFor(Client* client, String* line) {
  int nextState = STATE_PREAMBLE;
  if (line->startsWith(SMTP_HELO)) {
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
    if (head == -1) break;
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
}

String readFile(char* filename) {
  String data;
  
  // re-open the file for reading:
  File file = SD.open(filename);
  if (file) {    
    Serial.println("file open");
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

void interrupt() {
  interrupted = true;
}
