#include <avr/interrupt.h> 
#include <avr/io.h> 
#include <Mouse.h>
#include <EEPROM.h>

#define USART_BAUDRATE 9600
#define CARRIGE_RETURN 0x0D
#define LINE_FEED 0x0A
#define BACKSPACE 0x08
#define ESCAPE 0x1B
#define DELETE 0x7F   // Putty send "Delete" as "Backspace"
#define EEPROM_INIT 0x54  // Value set in EEPROM_INIT_ADDRESS after first turn on

#define EEPROM_INIT_ADDRESS 0
#define EEPROM_TIME_ADDRESS 1
#define EEPROM_INTENSITY_ADDRESS 5


byte incomingByte = 0; // for incoming serial data
char incomingBytes[10]; // ASCII bytes incoming on UART
byte incomingBytesTop = 0;  // "Pointer" to the end of incomingBytes
long time_val = 60; // Default time value
long intensity_val = 10;  // Default intensity value
long counted_seconds = 0;
bool is_move = false;

bool lol = false;

typedef enum
{
  MENU,
  TIME,
  INTENSITY,
} uartState_t;

static uartState_t uartState  = MENU;

const char uartHello[] = "Type 1 to do change time\r\nType 2 to do change intensity\r\nType 3 to show time\r\nType 4 to show intensity\r\n";
const char uartError[] = "Incorrect value\r\n";
const char intensitySelectedMsg[] = "Intensity selected [radius in px]\r\n";
const char timeSelectedMsg[] = "Time selected [s]\r\n";
const char timeSetMsg[] = "Time set to ";
const char intensitySetMsg[] = "Intensity set to ";
const char secMsg[] = " s\r\n";
const char pxMsg[] = " px\r\n";

void load_settings(){
  if (EEPROM.read(EEPROM_INIT_ADDRESS) == EEPROM_INIT) {
    time_val = EEPROM.read(EEPROM_TIME_ADDRESS);
    intensity_val = EEPROM.read(EEPROM_INTENSITY_ADDRESS);
  }
  else 
  {
    EEPROM.write(EEPROM_TIME_ADDRESS, time_val);
    EEPROM.write(EEPROM_INTENSITY_ADDRESS, intensity_val);
    EEPROM.write(EEPROM_INIT_ADDRESS, EEPROM_INIT);
  }
}

void clear_incoming_bytes(byte start = 0) {
  for (byte i = start; i < sizeof(incomingBytes); i++) {
    incomingBytes[i] = NULL;
  }
}

long read_value() {
  Serial.print((char)CARRIGE_RETURN);
  for (int i = 0; i < sizeof(incomingBytes); i++) {
    Serial.print(' ');
  }
  Serial.print((char)CARRIGE_RETURN);
  String incomingString = String(incomingBytes);
  long incomingValue = incomingString.toInt();
  clear_incoming_bytes();
  incomingBytesTop = 0;
  return incomingValue;
}

void menu() {
  switch (incomingByte)
  {
    case '1':
      uartState = TIME;
      Serial.print(timeSelectedMsg);
      break;
    case '2':
      uartState = INTENSITY;
      Serial.print(intensitySelectedMsg);
      break;
    case '3':
      Serial.print(time_val);
      Serial.print(secMsg);
      Serial.print(uartHello);
      break;
    case '4':
      Serial.print(intensity_val);
      Serial.print(pxMsg);
      Serial.print(uartHello);
      break;
    case CARRIGE_RETURN:
      Serial.print(uartHello);
      break;
    case LINE_FEED:
      Serial.print(uartHello);
      break;
    default:
      break;
  }
}

void read_change_time() {
  time_val = read_value();
  Serial.print(timeSetMsg);
  Serial.print(time_val);
  Serial.print(secMsg);
  Serial.print(uartHello);
  uartState = MENU;
  EEPROM.write(EEPROM_TIME_ADDRESS, time_val);
}

void read_change_intensity() {
  intensity_val = read_value();
  Serial.print(intensitySetMsg);
  Serial.print(intensity_val);
  Serial.print(pxMsg);
  Serial.print(uartHello);
  uartState = MENU;
  EEPROM.write(EEPROM_INTENSITY_ADDRESS, intensity_val);
}

void read_line() {
  if (incomingBytesTop == 0) {  // Empty Line
    Serial.print(uartError);
  }
  else
  {
    switch (uartState)
    {
      case TIME:
        read_change_time();
        break;

      case INTENSITY:
        read_change_intensity();
        break;
      
      default:
        break;
    }
  }
}

void uart_service() {
  while (Serial.available() > 0) {
    incomingByte = Serial.read();
    if (uartState == MENU) {
      menu();
    } else if (incomingByte == CARRIGE_RETURN || incomingByte == LINE_FEED || incomingBytesTop == sizeof(incomingBytes) - 1) { // Pressed "Enter" or buffer is full
      read_line();
    }
    else if (incomingByte == BACKSPACE || incomingByte == ESCAPE || incomingByte == DELETE) {   // Pressed "Backspace"
      if (incomingBytesTop != 0) {
        incomingBytesTop--;
        incomingBytes[incomingBytesTop] = NULL;
      }
      Serial.print((char)incomingByte); // Echo
    }
    else    // Pressed any other key
    {
      Serial.print((char)incomingByte); // Echo
      incomingBytes[incomingBytesTop] = incomingByte;
      incomingBytesTop++;
    }
  }
}

void move_mouse() {
  Mouse.move(1, 1, 0);
}

void set_timer1() {
  TCCR1A = 0;
  TCCR1B = 0;
  
  OCR1A = 15626;

  TCCR1B |= (1 << CS12) | (0 << CS11) | (1 << CS10); // 1024 Prescaler
  TCCR1B |= (1 << WGM12);
  TIMSK1 |= (1 << OCIE1A);   // enable timer overflow interrupt
}

void set_timer3() {
  TCCR3A = 0;
  TCCR3B = 0;
  
  OCR3A = 800;

  TCCR3B |= (1 << CS32) | (0 << CS31) | (1 << CS30); // 1024 Prescaler
  TCCR3B |= (1 << WGM32);
  TIMSK3 |= (1 << OCIE3A);   // enable timer overflow interrupt
}

void setup() {
  // put your setup code here, to run once:
  Mouse.begin();
  load_settings();
  clear_incoming_bytes();
  Serial.begin(USART_BAUDRATE);
  noInterrupts();
  set_timer1();
  set_timer3();
  interrupts();
}

void loop() {
  // put your main code here, to run repeatedly:
  uart_service();
  if (counted_seconds > time_val) {
    is_move = true;
    counted_seconds = time_val;
  } else {
    is_move = false;
  }
}

ISR(TIMER1_COMPA_vect)  // Przerwanie timera0 (porównanie rejestrów)
{
  counted_seconds++;
}

ISR(TIMER3_COMPA_vect)  // Przerwanie timera0 (porównanie rejestrów)
{
  if (is_move) {
    move_mouse();
  }
}
