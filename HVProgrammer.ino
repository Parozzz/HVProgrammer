// AVR High-voltage Serial Fuse Reprogrammer
// Adapted from code and design by Paul Willoughby 03/20/2010
// http://www.rickety.us/2010/03/arduino-avr-high-voltage-serial-programmer/
// Fuse Calc:
// http://www.engbedded.com/fusecalc/

#define RST 8 // Output to level shifter for !RESET from transistor
#define SCI 13 // Target Clock Input
#define SDO 12 // Target Data Output
#define SII 11 // Target Instruction Input
#define SDI 10 // Target Data Input
#define VCC 9 // Target VCC

#define HFUSE 0x747C
#define LFUSE 0x646C
#define EFUSE 0x666E

// Define ATTiny series signatures
#define ATTINY13 0x9007 // L: 0x6A, H: 0xFF 8 pin
#define ATTINY24 0x910B // L: 0x62, H: 0xDF, E: 0xFF 14 pin
#define ATTINY25 0x9108 // L: 0x62, H: 0xDF, E: 0xFF 8 pin
#define ATTINY44 0x9207 // L: 0x62, H: 0xDF, E: 0xFFF 14 pin
#define ATTINY45 0x9206 // L: 0x62, H: 0xDF, E: 0xFF 8 pin
#define ATTINY84 0x930C // L: 0x62, H: 0xDF, E: 0xFFF 14 pin
#define ATTINY85 0x930B // L: 0x62, H: 0xDF, E: 0xFF 8 pin

#define RED_LED_PIN 3
#define BUTTON_PIN A0
#define GREEN_LED_PIN A2

int oldButtonRead;
long buttonPressedTimestamp;
long ledFlashReadyTimestamp;
long readingTimestamp;

void setup() {
  
  buttonPressedTimestamp = 0;
  ledFlashReadyTimestamp = 0;
  readingTimestamp = 0;
  
  buttonPressedTimestamp = millis();
  
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(VCC, OUTPUT);
  pinMode(RST, OUTPUT);
  pinMode(SDI, OUTPUT);
  pinMode(SII, OUTPUT);
  pinMode(SCI, OUTPUT);
  pinMode(SDO, OUTPUT); // Configured as input when in programming mode
  digitalWrite(RST, HIGH); // Level shifter is inverting, this shuts off 12V
  Serial.begin(19200);
  Serial.println("Code is modified by Rik. Visit riktronics.wordpress.com and electronics-lab.com for more projects");
  Serial.println("-------------------------------------------------------------------------------------------------");
  Serial.println("Enter any character to start process..");
}


void loop() {

  bool buttonPressed = false;

  int buttonRead = digitalRead(BUTTON_PIN);
  if (!buttonRead && oldButtonRead && (millis() - buttonPressedTimestamp) > 2000) //Every 2s it could be pressed
  {
    buttonPressed = true;
    buttonPressedTimestamp = millis();
  }
  oldButtonRead = buttonRead;

  if(readingTimestamp == 0 || (millis() - readingTimestamp > 5000)) //Wait 5sec from last flash for flashing leds
  {
    if(millis() % 1000 > 500)
    {
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }
    else
    {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }


  if (Serial.available() > 0 || buttonPressed) {
    Serial.read();

    Serial.println();
    Serial.println("========================");
    
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(500);

    Serial.println("Started the fuse writing");
    
    pinMode(SDO, OUTPUT); // Set SDO to output
    digitalWrite(SDI, LOW);
    digitalWrite(SII, LOW);
    digitalWrite(SDO, LOW);
    digitalWrite(RST, HIGH); // 12v Off
    digitalWrite(VCC, HIGH); // Vcc On
    delayMicroseconds(20);
    digitalWrite(RST, LOW); // 12v On
    delayMicroseconds(10);
    pinMode(SDO, INPUT); // Set SDO to input
    delayMicroseconds(300);

    Serial.print("Reading signature ... ");
    unsigned int sig = readSignature();
    Serial.println(sig, HEX);

    Serial.print("BEFORE => ");
    readFuses();

    bool signatureValid = false;
    if (sig == ATTINY13) 
    {
      Serial.println("ATtiny13/ATtiny13A detected");
      
      Serial.println("Writing fuses L_FUSE: 6A, H_FUSE: FF");
      writeFuse(LFUSE, 0x6A);
      writeFuse(HFUSE, 0xFF);

      digitalWrite(GREEN_LED_PIN, HIGH);
    } 
    else if (sig == ATTINY24 || sig == ATTINY44 || sig == ATTINY84 ||sig == ATTINY25 || sig == ATTINY45 || sig == ATTINY85) 
    {
      signatureValid = true;
      
      Serial.print("ATtiny");
      if (sig == ATTINY24) Serial.println("24");
      else if (sig == ATTINY44) Serial.print("44");
      else if (sig == ATTINY84) Serial.print("84");
      else if (sig == ATTINY25) Serial.print("25");
      else if (sig == ATTINY45) Serial.print("45");
      else if (sig == ATTINY85) Serial.print("85");
      Serial.println(" detected");

      Serial.println("Writing fuses L_FUSE: 62, H_FUSE: DF, E_FUSE: 0xFF");
      writeFuse(LFUSE, 0x62);
      writeFuse(HFUSE, 0xDF);
      writeFuse(EFUSE, 0xFF);
    }
    
    Serial.print("AFTER => ");
    readFuses();
    
    digitalWrite(SCI, LOW);
    digitalWrite(VCC, LOW); // Vcc Off
    digitalWrite(RST, HIGH); // 12v Off

    if(signatureValid)
    {
      digitalWrite(GREEN_LED_PIN, HIGH);
      Serial.println("Writing Successful");
    }
    else
    {
      digitalWrite(RED_LED_PIN, HIGH);
      Serial.println("Writing Failed");
    }
    
    readingTimestamp = millis();

    Serial.println("========================");
    Serial.println();
  }
}

byte shiftOut (byte val1, byte val2) {
  int inBits = 0;
  //Wait until SDO goes high

  int timeout = 0;
  while (!digitalRead(SDO))
  {
    delayMicroseconds(5);
    if(++timeout > 200) //100 is 5us x 200 => 1ms 
    {
        return 0xFF;
    }
  }
  
  unsigned int dout = (unsigned int) val1 << 2;
  unsigned int iout = (unsigned int) val2 << 2;
  for (int ii = 10; ii >= 0; ii--) {
    digitalWrite(SDI, !!(dout & (1 << ii)));
    digitalWrite(SII, !!(iout & (1 << ii)));
    inBits <<= 1; inBits |= digitalRead(SDO);
    digitalWrite(SCI, HIGH);
    digitalWrite(SCI, LOW);
  }
  return inBits >> 2;
}

void writeFuse (unsigned int fuse, byte val) 
{
  shiftOut(0x40, 0x4C);
  shiftOut( val, 0x2C);
  shiftOut(0x00, (byte) (fuse >> 8));
  shiftOut(0x00, (byte) fuse);
}

void readFuses () {

  Serial.print("Reading fuses ... ");

  byte val;
  shiftOut(0x04, 0x4C); // LFuse
  shiftOut(0x00, 0x68);
  val = shiftOut(0x00, 0x6C);
  Serial.print("LFuse: ");Serial.print(val, HEX);
  
  shiftOut(0x04, 0x4C); // HFuse
  shiftOut(0x00, 0x7A);
  val = shiftOut(0x00, 0x7E);
  Serial.print(", HFuse: ");Serial.print(val, HEX);
  
  shiftOut(0x04, 0x4C); // EFuse
  shiftOut(0x00, 0x6A);
  val = shiftOut(0x00, 0x6E);
  Serial.print(", EFuse: "); Serial.println(val, HEX);
}

unsigned int readSignature () {
  unsigned int sig = 0;
  byte val;
  for (int ii = 1; ii < 3; ii++) {
    shiftOut(0x08, 0x4C);
    shiftOut( ii, 0x0C);
    shiftOut(0x00, 0x68);
    val = shiftOut(0x00, 0x6C);
    sig = (sig << 8) + val;
  }
  return sig;
}
