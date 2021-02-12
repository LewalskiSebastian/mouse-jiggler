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
#define MAX_LONG 2147483647 // Max long variable value

#define EEPROM_INIT_ADDRESS 0
#define EEPROM_TIME_ADDRESS 1
#define EEPROM_INTENSITY_ADDRESS 5
#define EEPROM_SPEED_ADDRESS 9
#define EEPROM_RIGHT_ADDRESS 13

byte incomingByte = 0; // for incoming serial data
char incomingBytes[10]; // ASCII bytes incoming on UART
byte incomingBytesTop = 0;  // "Pointer" to the end of incomingBytes
long time_val = 60; // Default time value
long intensity_val = 10;  // Default intensity value
long speed_val = 30;  // Default speed value
bool is_right = true;  // Default movement direction
long counted_seconds = 0;
unsigned int counted_impulses = 0;
bool is_move = false;

typedef enum
{
  MENU,
  TIME,
  INTENSITY,
  SPEEDO, // Cannot be just SPEED :/
} uartState_t;

static uartState_t uartState  = MENU;

const char uartHello[] = "Type 1 to do change time\r\nType 2 to do change intensity\r\nType 3 to do change speed\r\nType 4 to do change direction\r\nType 5 to show values\r\n";
const char uartError[] = "Incorrect value\r\n";
const char intensitySelectedMsg[] = "Intensity selected [px]\r\n";
const char timeSelectedMsg[] = "Time selected [s]\r\n";
const char speedSelectedMsg[] = "Speed selected\r\n";
const char timeSetMsg[] = "Time set to ";
const char intensitySetMsg[] = "Intensity set to ";
const char speedSetMsg[] = "Speed set to ";
const char directionSetMsg[] = "Direction set to ";
const char secMsg[] = " s\r\n";
const char pxMsg[] = " px\r\n";
const char rightMsg[] = " right\r\n";
const char leftMsg[] = " left\r\n";

void load_settings(){
  if (EEPROM.read(EEPROM_INIT_ADDRESS) == EEPROM_INIT) {
    time_val = EEPROM.read(EEPROM_TIME_ADDRESS);
    intensity_val = EEPROM.read(EEPROM_INTENSITY_ADDRESS);
    speed_val = EEPROM.read(EEPROM_SPEED_ADDRESS);
    is_right = EEPROM.read(EEPROM_RIGHT_ADDRESS);
  }
  else 
  {
    EEPROM.write(EEPROM_TIME_ADDRESS, time_val);
    EEPROM.write(EEPROM_INTENSITY_ADDRESS, intensity_val);
    EEPROM.write(EEPROM_SPEED_ADDRESS, speed_val);
    EEPROM.write(EEPROM_RIGHT_ADDRESS, is_right);
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
    case '1':   // Chagne time choosen
      uartState = TIME;
      Serial.print(timeSelectedMsg);
      break;
    case '2':   // Chagne intensity choosen
      uartState = INTENSITY;
      Serial.print(intensitySelectedMsg);
      break;
    case '3':   // Chagne speed choosen
      uartState = SPEEDO;
      Serial.print(speedSelectedMsg);
      break;
    case '4':   // Chagne direction choosen
      Serial.print(directionSetMsg);
      is_right = !is_right;
      if (is_right) {
        Serial.print(rightMsg);
      }
      else
      {
        Serial.print(leftMsg);
      }
      EEPROM.write(EEPROM_RIGHT_ADDRESS, is_right);
      Serial.print(uartHello);
      break;
    case '5':   // Show values choosen
      Serial.print(timeSetMsg);
      Serial.print(time_val);
      Serial.print(secMsg);
      Serial.print(intensitySetMsg);
      Serial.print(intensity_val);
      Serial.print(pxMsg);
      Serial.print(speedSetMsg);
      Serial.println(speed_val);
      Serial.print(directionSetMsg);
      if (is_right) {
        Serial.print(rightMsg);
      }
      else
      {
        Serial.print(leftMsg);
      }
      break;
    case CARRIGE_RETURN:  // Enter pressed
      Serial.print(uartHello);
      break;
    case LINE_FEED:       // Enter pressed
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
  counted_seconds = 0;
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

void read_change_speed() {
  speed_val = read_value();
  Serial.print(speedSetMsg);
  Serial.println(speed_val);
  Serial.print(uartHello);
  uartState = MENU;
  EEPROM.write(EEPROM_SPEED_ADDRESS, speed_val);
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

      case SPEEDO:
        read_change_speed();
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
  double s = sin(counted_impulses*2*PI/speed_val);
  double c = cos(counted_impulses*2*PI/speed_val);
  if (is_right) {
    Mouse.move(intensity_val*c, intensity_val*s, 0);
  }
  else 
  {
    Mouse.move(intensity_val*s, intensity_val*c, 0);
  }
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
  } else {
    is_move = false;
  }
}

ISR(TIMER1_COMPA_vect)  // Przerwanie timera0 (porównanie rejestrów)
{
  counted_seconds++;
  if (counted_seconds < 0) {
    counted_seconds = MAX_LONG;
  }
}

ISR(TIMER3_COMPA_vect)  // Przerwanie timera0 (porównanie rejestrów)
{
  if (is_move) {
    counted_impulses = ++counted_impulses%speed_val;
    move_mouse();
  }
}
