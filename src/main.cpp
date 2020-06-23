//ESP32, MAX30102, BMP085 - based pulse-oximeter
//2020, Piotr Żęsa
//TODO: put screens into class
//TODO: tidy up the code
//TODO: remove unneeded libs
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <Adafruit_BMP085.h>
#include <sqlite3.h>
#include "pulse.h"
#include <TFT_eSPI.h>
const int graphWidth = 250;
const int graphHeight = 100;
const int startPos[2] = {18, 222}; //0 - X, 1 - Y
int currXPos = startPos[0] + 1;
int prevScreenVal = 0;
//UI colors
const int UI_MAIN = ILI9341_GREENYELLOW;
const int UI_TEXT1 = ILI9341_GREENYELLOW;
const int UI_TEXT2 = ILI9341_WHITE;
const int UI_ERROR = ILI9341_RED;
const int UI_BACKGROUND = ILI9341_BLACK;
const int UI_TEXT_MAIN = ILI9341_WHITE;
//SPI pin definitions
#define SD_CS 26
bool tempBool = true;
sqlite3 *db;
SPIClass SDSPI(HSPI);
TFT_eSPI tft = TFT_eSPI();
MAX30105 pox;             //oximeter object
Adafruit_BMP085 bmp;      //barometer and thermometer object
const byte RATE_SIZE = 8; //count of samples taken for SpO2 and pulse average
byte oxyBuff[RATE_SIZE], rateBuff[RATE_SIZE], rateSpot, oxySpot;
bool memCard = true;
bool dbOK = true;
int beatAvg = 0;
int oxyAvg = 0;
float temp = 0;
float pres = 0;
long lastBeat = 0;
float BPM, SpO2;
Pulse pulseIR;  //for manipulating IR samples
Pulse pulseRed; //for manipulating red samples
byte inByte;
float pulseTempData = 0;
float oxyTempData = 0;
float tempTempData = 0;
const char *data = "Callback function called";

static int callback(void *data, int argc, char **argv, char **azColName)
{
  int i;
  Serial.printf("%s: ", (const char *)data);
  for (i = 0; i < argc; i++)
  {
    Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}

int openDb(const char *filename, sqlite3 **db)
{
  int rc = sqlite3_open(filename, db);
  if (rc)
  {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
    return rc;
  }
  else
  {
    Serial.printf("Opened database successfully\n");
  }
  return rc;
}

char *zErrMsg = 0;
int db_exec(sqlite3 *db, const char *sql)
{
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK)
  {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  else
  {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken:"));
  Serial.println(micros() - start);
  return rc;
}
void writeToSD()
{
  File plik = SD.open("/obrazeg", FILE_APPEND);
  plik.println("palec wykryty");
  plik.close();
}
void graphWrite()
{
  tft.setCursor(10, 132);
  tft.fillRect(10, 132, graphWidth, graphHeight, ILI9341_YELLOW);
  tft.fillRect(startPos[0], startPos[1], 1, 1, ILI9341_RED);
  tft.drawLine(startPos[0], startPos[1], startPos[0], startPos[1] - 80, ILI9341_BLACK);  //podzialka pionowa
  tft.drawLine(startPos[0], startPos[1], startPos[0] + 201, startPos[1], ILI9341_BLACK); //podzialka pozioma
  {                                                                                      //podziałki na podziałce poziomej
    int currPos = startPos[0] + 1;
    for (int ctr = 0; ctr < 11; ctr++)
    {
      tft.drawLine(currPos, startPos[1], currPos, startPos[1] + 5, ILI9341_BLACK);
      currPos += 20;
    }
  }
}
void writeOnGraph(int _value)
{
  if (currXPos != startPos[0] + 201)
  {
    int screenVal = map(_value, 0, 100, 221, 142);
    tft.fillRect(currXPos, screenVal, 1, 1, ILI9341_BLUE);
    if (currXPos > startPos[0] + 20)
    {
      tft.drawLine(currXPos - 20, prevScreenVal, currXPos, screenVal, ILI9341_BLUE);
    }
    prevScreenVal = screenVal;
    currXPos += 20;
  }
  else
  {
    graphWrite();
    currXPos = startPos[0] + 1;
  }
}
//measurement screen with three windows
void measScreen()
{
  tft.fillScreen(UI_BACKGROUND);
  tft.drawRect(0, 0, 320, 10, UI_MAIN);     //taskbar
  tft.drawRect(0, 10, 160, 110, UI_MAIN);   //Pulse
  tft.drawRect(160, 10, 160, 110, UI_MAIN); //Oxy
  tft.drawRect(0, 120, 320, 120, UI_MAIN);  //Chart
  tft.setTextSize(1);
  tft.setTextColor(UI_TEXT1);
  tft.setCursor(2, 12);
  tft.println("Pulse:");
  tft.setCursor(2, 52);
  tft.println("BPM");
  tft.setCursor(162, 12);
  tft.println("SpO2:");
  tft.setCursor(162, 52);
  tft.println("%");
  tft.setCursor(2, 122);
  tft.println("Chart:");
}
//warning screen with two lines of warning text
void warnScreen(String wtLine1, String wtLine2)
{
  tft.setTextSize(1);
  tft.fillScreen(UI_BACKGROUND);
  tft.drawRect(10, 10, 300, 220, UI_ERROR);
  tft.drawRect(15, 15, 290, 210, UI_ERROR);
  tft.setCursor(150, 100);
  tft.setTextColor(UI_ERROR);
  tft.println("OSTRZEZENIE!");
  tft.setCursor(100, 130);
  tft.setTextColor(UI_TEXT_MAIN);
  tft.println(wtLine1);
  tft.setCursor(100, 140);
  tft.println(wtLine2);
}
void writeOnMeasScreen(int type, float data)
{
  //TYPE: 1 = oxy; 2 = pulse; 3 = char(tbc)
  //prevent unnecessary refreshing when current value equals the incoming one
  if (type == 1 && data == oxyTempData || type == 2 && data == pulseTempData)
  {
    return;
  }
  //erase old data
  int textSize;
  tft.setTextColor(UI_BACKGROUND);
  switch (type)
  {
  case 1:
    textSize = 3;
    tft.setTextSize(textSize);
    tft.setCursor(162, 20);
    tft.println(oxyTempData);
    tft.setCursor(162, 20);
    break;
  case 2:
    textSize = 3;
    tft.setTextSize(textSize);
    tft.setCursor(2, 20);
    tft.println(pulseTempData);
    tft.setCursor(2, 20);
    break;
  case 3:
    textSize = 1;
    tft.setTextSize(textSize);
    tft.setCursor(2, 1);
    tft.println(tempTempData);
    tft.setCursor(2, 1);
    break;
  }
  //write new data
  tft.setTextColor(UI_TEXT2);
  tft.println(data);
  if (type == 1)
  {
    oxyTempData = data;
  }
  else if (type == 2)
  {
    pulseTempData = data;
  }
  else if (type == 3)
  {
    tempTempData = data;
  }
}

void writeToSerial(float oxyMeas, float pulseMeas)
{
  float meas[2];
  oxyMeas = roundf(oxyMeas * 100);
  oxyMeas /= 100;
  pulseMeas = roundf(pulseMeas * 100);
  pulseMeas /= 100;
  meas[0] = oxyMeas;
  meas[1] = pulseMeas;
  byte *b = (byte *)&meas;
  Serial.write(b, 4 * 2);
}
void SDWrite(void *parameter)
{
  String query = (char *)parameter;

  vTaskDelete(NULL);
}
void dataCollect()
{
  //take raw values from sensor
  long irVal = pox.getIR();
  Serial.println(irVal);
  if (irVal > 13000)
  {
    if (!tempBool)
    {
      tempBool = true;
      measScreen();
      graphWrite();
      currXPos = startPos[0] + 1;
      beatAvg = 0;
      oxyAvg = 0;
    }
    long redVal = pox.getRed();
    //filter out DC offset
    long ac_irVal = pulseIR.DCFilter(irVal);
    long ac_redVal = pulseRed.DCFilter(redVal);
    //calculate AC component
    long dc_irVal = irVal - ac_irVal;
    long dc_redVal = redVal - ac_redVal;
    //check for heartbeat
    if (checkForBeat(irVal))
    {
      //calculate average BPM
      long delta = millis() - lastBeat;
      lastBeat = millis();
      BPM = 60 / (delta / 1000.0); //momentary BPM
      if (BPM < 255 && BPM > 20)
      {
        rateBuff[rateSpot++] = (byte)BPM;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte b = 0; b < RATE_SIZE; b++)
        {
          beatAvg += rateBuff[b];
        }
        beatAvg /= RATE_SIZE; //average BPM taken from 8 samples
      }
      //calculate average SpO2
      float R = (float(ac_redVal) / float(dc_redVal)) / (float(ac_irVal) / float(dc_irVal)); //"ratio of ratios" taken from MAX30102 datasheet
      float SpO2 = 104 - 17 * R;                                                             //momentary SPo2
      if (SpO2 < 100 && SpO2 > 90)
      {
        oxyBuff[oxySpot++] = (byte)SpO2;
        oxySpot %= RATE_SIZE;
        oxyAvg = 0;
        for (byte b = 0; b < RATE_SIZE; b++)
        {
          oxyAvg += oxyBuff[b];
        }
        oxyAvg /= RATE_SIZE; //average SpO2 taken from 8 samples
      }

      writeOnMeasScreen(2, beatAvg);
      writeOnMeasScreen(1, oxyAvg);
      writeOnGraph(oxyAvg);
      //FIXME: assign this to second core since it slows down whole rest
      String query = "INSERT INTO oneShot(pulse, spo2, temp, pres) VALUES(" + (String)beatAvg + ", " + (String)oxyAvg + ", " + (String)temp + ", " + (String)pres + ");";
      [[maybe_unused]] int rc = db_exec(db, query.c_str());
      //screenshot, doesn't work but doesn't matter
      /* uint8_t data[58000];
      tft.readRectRGB(1, 1, 160, 120, data);
      File pl = SD.open("/obrazeg", FILE_WRITE);
      pl.write(data, sizeof(data));
      Serial.println("OK!"); */
    }
  }
  else
  {
    // substituted by dedicated warning screen
    /* writeOnMeasScreen(1, 255);
    writeOnMeasScreen(2, 255); */
    if (tempBool)
    {
      tempBool = false;
      warnScreen("Nie wykryto obecnosci palca!", "Prosze polozyc palec.");
    }
  }
}
void setup()
{
  SDSPI.begin(14, 27, 13, -1);
  //serial interface init
  Serial.begin(9600);
  //display init
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(UI_TEXT_MAIN);
  tft.println("FajnyMiernik ver0.01 alpha by Piotr Zesa");
  tft.println("Initializing...");
  //bluetooth init
  //i2c bus init
  Wire.begin(21, 22);
  //oximeter sensor init
  tft.print("\n");
  tft.print("Oximeter module init...");
  if (!pox.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("POX NIE OK");
    tft.setTextColor(UI_ERROR);
    tft.println("Nie znaleziono urzadzenia MAX30102");
    while (1)
    {
    };
  }
  tft.print("OK!");
  tft.print("\n");
  tft.print("Memory card module init...");
  if (!SD.begin(SD_CS, SDSPI))
  {
    tft.setTextColor(UI_ERROR);
    tft.println("Blad karty pamieci!");
    tft.setTextColor(UI_TEXT_MAIN);
    memCard = false;
  }
  if (memCard)
  {
    tft.print("OK!");
    tft.print("\n");
  }
  tft.print("SQLite3 subsystem init...");
  if (!SD.exists("/db.db"))
  {
    File plik = SD.open("/db.db", FILE_WRITE);
    plik.close();
  }

  if (openDb("/sd/db.db", &db))
  {
    tft.setTextColor(UI_ERROR);
    tft.println("Nie mozna otworzyc bazy danych!");
    tft.setTextColor(UI_TEXT_MAIN);
    dbOK = false;
  }
  tft.print("OK!");
  tft.print("\n");
  tft.print("Barometer module init...");
  if (!bmp.begin())
  {
    Serial.println("BMP NIE OK");
    tft.setTextColor(UI_ERROR);
    tft.println("Nie znaleziono urzadzenia BMP085");
    while (1)
    {
    };
  }
  tft.print("OK!");
  pox.setup();
  pox.setPulseAmplitudeRed(0x05);
  pox.setPulseAmplitudeIR(0x05);

  Serial.println("Setup finished successfully!");
  if (dbOK)
  {
    [[maybe_unused]] int rc = db_exec(db, "CREATE TABLE IF NOT EXISTS oneShot(id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, username TEXT DEFAULT 'test', pulse FLOAT DEFAULT 255, spo2 FLOAT DEFAULT 255, temp FLOAT DEFAULT 255, pres FLOAT DEFAULT 255, timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP);");
  }
  if (!dbOK)
  {
    warnScreen("Blad odczytu bazy danych!", "Sprawdz zawartosc karty.");
  }
  if (!memCard)
  {
    warnScreen("Blad karty pamieci!", "Zapis parametrow niemozliwy.");
  }
  delay(3000);
  measScreen();
}

void loop()
{
  dataCollect();
  temp = bmp.readTemperature();
  pres = bmp.readPressure();
  if (fmod(double(temp), double(int(temp))) < 40)
  {
    temp = int(temp);
  }
  else if (fmod(double(temp), double(int(temp))) > 40)
  {
    temp = int(temp) + 1;
  }
  else
  {
    temp = int(temp) + 0.5;
  }
  if (temp != tempTempData)
  {
    writeOnMeasScreen(3, temp);
  }
}