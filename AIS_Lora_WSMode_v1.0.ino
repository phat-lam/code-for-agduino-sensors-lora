/*
 * Simple code for wsn id 01 monitoring environmental parameters (soil & air) in fruit farm. 
 * Reads sensors and sends data to gateway by RF wireless transceiver Lora 433MHz. 
 * Timer period: 30 seconds in normal mode, 10 seconds during irrigation.
 * 
 * Atmega328p-au MCU with arduino bootloader (or Arduino Uno).
 * Data transceiver: Lora SX1278 SPI-to-UART interface, frequency: 433MHz, default baud-rate: 9600 kbps.
 * Sensors: temperature & relative humidity SHT11, soil moisture and temperature sensor 5TM, rain sensor.
 * 
 * The circuit:
 * D3: 5TM data pin.
 * D6: SHT11 clock pin.
 * D7: SHT11 data pin
 * D8: output controls EM valve.
 * D9: output controls pump.
 * A4: OLED SDA pin.
 * A5: OLED SCL pin.
 * A0: analog output of rain sensor.
 * RXD(D0): TXD pin (RF transceiver Lora).
 * TXD(D1): RXD pin (RF transceiver Lora).
 * 
 * Created 28 Mar 2017 by AgriConnect.
 */
// Comes with IDE
#include <Wire.h>
#include <EEPROM.h>
// Timer lib
#include <SimpleTimer.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
// OLED I2C lib
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// T&RH SHT11 sensor lib
#include "SHT1x.h"
// Soil sensor 5TM lib
#include <SDI12.h>

/*-----( Declare Constants and Pin Numbers )------*/
// String length
#define ln 10
// Timer
int timer_long_period = 30000;
int timer_short_period = 10000;
// EEPROM address
int eprom_valve_add = 0;
int eprom_pump_add = 1;
// Network 0, node's ID 01
char ws_mote_address[] = "101";
// Network 0, node's ID 00
char gw_address[] = "100";
// T&RH SHT11 sensor
#define sht_io_clk 6
#define sht_io_data 7
// Rain sensor
int rainSensor_pin = A0;
// Soil sensor
const int data_pin = 3;
// OLED display
#define OLED_RESET 2
// GPIOs control actuators
#define valve_pin 8
#define pump_pin 9
/*-----( Declare Objects and Variables )----------*/
// Receiving data
char serial_buff[32];
// Transmitted data
char str_envi_data[30];
// Data (node ID, t, h, soil_dielectric, soil_temperature, rain, valve_status, pump_status) in char-array
static char dtostrfbuffer1[4];
static char dtostrfbuffer2[4];
static char dtostrfbuffer3[5];
static char dtostrfbuffer4[4];
static char dtostrfbuffer5[2];
static char dtostrfbuffer6[2];
static char dtostrfbuffer7[2];
static char dtostrfbuffer8[4];
// Rain sensor
int rainSensor_val = 0;
int rainSensor_status = 0;
// Soil sensor
SDI12 mySDI12(data_pin);
float f_soil_dielctric = 0, f_soil_temp = 0;
float f_soil_vwc;
// OLED display
Adafruit_SSD1306 display(OLED_RESET);

SimpleTimer timer1;
SimpleTimer timer2;
// Actuator's status
int valve_status = 0;
int light_status = 0;
// EPROM data
byte eprom_val;
// T&RH SHT11 sensor
SHT1x sht1x(sht_io_data, sht_io_clk);
/*----- SETUP: RUNS ONCE -------------------------*/
void setup()
{
  // Initialize the serial communications. Set speed to 9600 kbps.
  Serial.begin(9600);
  // Initialize SDI-12 communication.
  mySDI12.begin();
  // GPIO configuration
  pinMode(valve_pin, OUTPUT);
  pinMode(pump_pin, OUTPUT);
  // Set HIGH logic in the beginning
  digitalWrite(valve_pin, HIGH);
  digitalWrite(pump_pin, HIGH);
  // Write actuoator's status in the beginning
  EEPROM.write(eprom_valve_add, valve_status);
  EEPROM.write(eprom_pump_add, light_status);
  // OLED display initiation
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  // Clear OLED display
  display.clearDisplay();
  // Set text size and color
  display.setTextSize(1);
  display.setTextColor(WHITE);
  // Position on OLED
  display.setCursor(30,12);
  // Display "AgriConnect" on OLED
  display.println("AgriConnect");
  display.display();
  // // Initialize the timers
  timer1.setInterval(timer_short_period, timer_isr);
  timer2.setInterval(timer_long_period, timer_isr);
}
/*----- End SETUP ---------------------------------*/
/*----- LOOP: RUNS CONSTANTLY ---------------------*/
void loop()
{
  // if in irrigating process, node sends data every 10 seconds. Otherwise, node sends data every 30 seconds.
  eprom_val = EEPROM.read(eprom_valve_add);
  if(eprom_val == 1)
  {
    timer1.run();
  }
  else if(eprom_val == 0)
  {
    timer2.run();
  }
  // Receiving commands from gateway to control actuators
  if(Serial.available())
  {
    int c = Serial.available();
    if(c > 0)
    {
      Serial.readBytesUntil(ln, serial_buff, 32);
      //Serial.println(serial_buff);
      
      if((serial_buff[5] == '1' && rainSensor_status == 0) && valve_status == 0)
      {
        digitalWrite(valve_pin, LOW);
        valve_status = 1;
        EEPROM.write(eprom_valve_add, valve_status);
      }
      else if((serial_buff[5] == '0' || rainSensor_status == 1) && valve_status == 1)
      {
        digitalWrite(valve_pin, HIGH);
        valve_status = 0;
        EEPROM.write(eprom_valve_add, valve_status);
      }

      if(serial_buff[7] == '1')
      {
        digitalWrite(pump_pin, LOW);
        light_status = 1;
        EEPROM.write(eprom_pump_add, light_status);
      }
      else if(serial_buff[7] == '0')
      {
        digitalWrite(pump_pin, HIGH);
        light_status = 0;
        EEPROM.write(eprom_pump_add, light_status);
      }
      
      Serial.println(str_envi_data);
    }
  }
  // if raining, stop irrigating process.
  if(rainSensor_status == 1)
  {
    digitalWrite(valve_pin, HIGH);
    valve_status = 0;
    EEPROM.write(eprom_valve_add, valve_status);
  }
}
/*----- End LOOP ----------------------------------*/
/*-----( Declare User-written Functions )----------*/
void timer_isr()
{
  float f_tempC = sht1x.readTemperatureC();
  float f_humi = sht1x.readHumidity();
  dtostrf((int)(f_tempC * 10), 3, 0, dtostrfbuffer1);
  dtostrf((int)(f_humi * 10), 3, 0, dtostrfbuffer2);

  takeMeasurement('0');
  f_soil_vwc = 0.0000043 * pow(f_soil_dielctric, 3) - 0.00055 * pow(f_soil_dielctric, 2) + 0.0292 * f_soil_dielctric - 0.053;
  dtostrf(f_soil_dielctric * 100, 4, 0, dtostrfbuffer3);
  dtostrf(f_soil_temp * 10, 3, 0, dtostrfbuffer4);
  dtostrf(f_soil_vwc * 1000, 3, 0, dtostrfbuffer8);
  
  rainSensor_val = analogRead(rainSensor_pin);
  if(rainSensor_val > 400) rainSensor_status = 0;
  else rainSensor_status = 1;
  dtostrf(rainSensor_status, 1, 0, dtostrfbuffer5);

  dtostrf(valve_status, 1, 0, dtostrfbuffer6);
  dtostrf(light_status, 1, 0, dtostrfbuffer7);
    
  sprintf(str_envi_data, "S%sT%sH%sD%sP%sR%sV%sL%sE", ws_mote_address, dtostrfbuffer1, dtostrfbuffer2, dtostrfbuffer3, dtostrfbuffer4, dtostrfbuffer5, dtostrfbuffer6, dtostrfbuffer7);
  Serial.println(str_envi_data);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Soil:");
  display.setTextSize(2);
  display.setCursor(32,0);
  display.print(dtostrfbuffer8);
  display.setTextSize(1);
  display.setCursor(70,7);
  display.print("%vwc");
  display.setCursor(90,0);
  display.print(dtostrfbuffer4);
  display.print(" oC");
  display.setCursor(0,12);
  display.print("---------------------");
  display.setCursor(0,17);
  display.print("Air:");display.print("     ");
  display.print(valve_status, DEC);display.print(":");
  display.print(light_status, DEC);
  display.setCursor(0,25);
  display.print(dtostrfbuffer2);display.print(" %RH  ");
  display.print(dtostrfbuffer1);display.print(" oC     ");
  display.print(rainSensor_status, DEC);
  display.setCursor(96,17);
  display.print("Rain:");
  display.display();
}

void takeMeasurement(char i)
{
  String command = "";
  command += i;
  // SDI-12 measurement command format  [address]['M'][!]
  command += "M!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of measurments available, 0-9]
  while(!mySDI12.available()>5);
  delay(100);
  //consume address
  mySDI12.read(); 
  // find out how long we have to wait (in seconds).
  int wait = 0; 
  wait += 100 * mySDI12.read()-'0';
  wait += 10 * mySDI12.read()-'0';
  wait += 1 * mySDI12.read()-'0';
  // ignore # measurements, for this simple examlpe
  mySDI12.read();
  // ignore carriage return
  mySDI12.read();
  // ignore line feed
  mySDI12.read();
  
  long timerStart = millis();
  while((millis() - timerStart) > (1000 * wait))
  {
    //sensor can interrupt us to let us know it is done early
    if(mySDI12.available()) break;
  }
  // in this example we will only take the 'DO' measurement  
  mySDI12.flush(); 
  command = "";
  command += i;
  // SDI-12 command to get data [address][D][dataOption][!]
  command += "D0!";
  mySDI12.sendCommand(command);
  // wait for acknowlegement
  while(!mySDI12.available()>1);
  // let the data transfer
  delay(300);
  printBufferToScreen();
  mySDI12.flush();
}
void printBufferToScreen()
{
  String buffer = "";
  String buffer1 = "";
  String buffer2 = "";
  // consume address
  mySDI12.read();
  while(mySDI12.available())
  {
    char c = mySDI12.read();
    if(c == '+' || c == '-')
    {
      buffer += '/';
      if(c == '-') buffer += '-';
    } 
    else
    {
      buffer += c;
    }
    delay(100);
  }
 buffer1 = buffer.substring(buffer.indexOf("/") + 1, buffer.lastIndexOf("/"));
 buffer2 = buffer.substring(buffer.lastIndexOf("/") + 1, buffer.lastIndexOf("/") + 5);

 f_soil_dielctric = buffer1.toFloat();
 f_soil_temp = buffer2.toFloat();
}
// gets identification information from a sensor, and prints it to the serial port
// expects a character between '0'-'9', 'a'-'z', or 'A'-'Z'.
char printInfo(char i)
{
  int j; 
  String command = "";
  command += (char) i;
  command += "I!";
  for(j = 0; j < 1; j++)
  {
    mySDI12.sendCommand(command);
    delay(30);
    if(mySDI12.available()>1) break;
    if(mySDI12.available()) mySDI12.read();
  }

  while(mySDI12.available())
  {
    char c = mySDI12.read();
    if((c!='\n') && (c!='\r')) Serial.write(c);
    delay(5);
  } 
}
// converts allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
// to a decimal number between 0 and 61 (inclusive) to cover the 62 possible addresses
byte charToDec(char i)
{
  if((i >= '0') && (i <= '9')) return i - '0';
  if((i >= 'a') && (i <= 'z')) return i - 'a' + 10;
  if((i >= 'A') && (i <= 'Z')) return i - 'A' + 37;
}

// THIS METHOD IS UNUSED IN THIS EXAMPLE, BUT IT MAY BE HELPFUL.
// maps a decimal number between 0 and 61 (inclusive) to
// allowable address characters '0'-'9', 'a'-'z', 'A'-'Z',
char decToChar(byte i)
{
  if((i >= 0) && (i <= 9)) return i + '0';
  if((i >= 10) && (i <= 36)) return i + 'a' - 10;
  if((i >= 37) && (i <= 62)) return i + 'A' - 37;
}
