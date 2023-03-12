#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SH110X.h"
#include <SoftwareSerial.h>
#include <SD.h>

SoftwareSerial BT(10,11); //Rx/Tx

#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
#define BAUDRATE 57600
#define keyStart  4
#define keyStop  5
#define keyTestStart  2
#define keyTestStop  3

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
File myFile;

// system variables
byte  generatedChecksum = 0;
byte  checksum = 0; 
int   payloadLength = 0;
byte  payloadData[64] = {0};
//byte  poorQuality = 0;
byte  attention = 0;
byte  meditation = 0;
boolean bigPacket = false;
boolean bMode = false; // 0-stop, 1-run test
boolean bSave = false; // 0-stop, 1-start
String cFileName;
String cFileRaw;
byte nTestNum = 1;
byte aAttention[255];
byte aMeditation[255];
byte cIdx;
byte nIdx;
int nSum;
byte nAvgAtt;
byte nAvgMed;

void getFileName(){
  int nFile = 1;
  String fByte;
  //read settings
  if (!SD.exists("conf.ini")) {
    myFile = SD.open("conf.ini", FILE_WRITE);
    myFile.write(nFile);
    myFile.close();
  }

  myFile = SD.open("conf.ini", FILE_READ);
  if (myFile.available()) {
    fByte = myFile.read();
    nFile = fByte.toInt();
  }
  myFile.close();
  cFileName = "test_" + String(nFile) + ".csv";
  cFileRaw = "test_" + String(nFile) + ".raw";
}

void getNextFileName(){
  int nFile = 1;
  String fByte;
  if (!SD.exists("conf.ini")) {
    getFileName();
    return;
  }
  myFile = SD.open("conf.ini", FILE_READ);
  if (myFile.available()) {
    fByte = myFile.read();
    nFile = fByte.toInt();
  }
  myFile.close();
  nFile++;
  SD.remove("conf.ini");
  myFile = SD.open("conf.ini", FILE_WRITE);
  myFile.write(nFile);
  myFile.close();
  cFileName = "test_" + String(nFile) + ".csv";
  cFileRaw = "test_" + String(nFile) + ".raw";
}

void showData(byte cMod){
  display.clearDisplay();
  display.setCursor(0, 0);
  if ((attention > 0) && (meditation > 0)) {
    display.print("Attention:  ");
    display.println(String(attention));
    display.print("Meditation: ");
    display.println(String(meditation));
  } else {
    if (cMod == 0) {
      display.print("Connection lost!");
    } else {
      display.print("Save data...");
    }
  }
  display.setCursor(0, 25);
  if (!bMode) {
    display.println("Test run: No");
  } else {
    display.println("Test run: Yes");
  }
  if (!bSave) {
    display.print("Measure [");
    display.print(String(nTestNum));
    display.print("]: No");
  } else {
    display.print("Measure [");
    display.print(String(nTestNum));
    if (bSave) {
      display.print("/");
      display.print(String(cIdx+1));
    }
    display.print("]: Yes");
  }
  display.setCursor(0, 50);
  display.print("File: ");
  display.println(cFileName);
  display.display();
}

void setup()   {
  Serial.begin(BAUDRATE);
  BT.begin(BAUDRATE);               // Software serial port  (ATMEGA328P)
  display.begin(i2c_Address, true); // Address 0x3C default
  delay(250);  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.print("MindWave clicker v1.0");
  display.setCursor(0, 20);
  
  if (!SD.begin(53)) {
    display.println("Insert card...");
    display.display();
    while (!SD.begin(53));
  }
  display.print("Waiting connection...");
  display.display();

  pinMode(keyStart, INPUT_PULLUP);
  pinMode(keyStop, INPUT_PULLUP);
  pinMode(keyTestStart, INPUT_PULLUP);
  pinMode(keyTestStop, INPUT_PULLUP);

  getFileName();
}

////////////////////////////////
// Read data from Serial UART //
////////////////////////////////
byte ReadOneByte() {
  int ByteRead;
  int nCount = 0;
  while(!BT.available()) {
    nCount++;
    // hard reset, if MW unaviable
    if (nCount > 1000) {
      return 0;
    }
  }
  ByteRead = BT.read();
  return ByteRead;
}

/////////////
//MAIN LOOP//
/////////////
void loop() {
 // poorQuality = 200;
  attention = 0;
  meditation = 0;
  bigPacket = false; 

  //read keys
  if (!bMode) {
    if (digitalRead(keyTestStart) == LOW) {
      bMode = true;
      bSave = false;    
      nTestNum = 1;
      myFile = SD.open(cFileRaw, FILE_WRITE);
      myFile.println("Number;Time;Attention;Meditation");
      myFile.close();
      myFile = SD.open(cFileName, FILE_WRITE);
      myFile.println("Number;Time;Attention;Meditation");
      myFile.close();
      showData(1);
      delay(50);
    }
  }  
  if (bMode) {
    if (digitalRead(keyTestStop) == LOW) {
      bMode = false;
      bSave = false;
      if (nTestNum > 1) getNextFileName();
      nTestNum = 1; 
      showData(1);
      delay(1000);
    }  
    if (!bSave) {
      if (digitalRead(keyStart) == LOW) {    
        bSave = true;
        showData(1);
        // clear array for new data
        cIdx = 0;
        while (cIdx < 255) {
          aAttention[cIdx] = 0;
          aMeditation[cIdx] = 0;
          cIdx++;
        }
        cIdx = 0;
        delay(50);
      }
    }
    if (bSave) {
      if (digitalRead(keyStop) == LOW) {
        bSave = false;
        showData(1);
        // save rawdata to file
        myFile = SD.open(cFileRaw, FILE_WRITE);
        myFile.print(nTestNum);
        myFile.print(";");
        myFile.print(cIdx);
        myFile.print(";");
        nIdx = 0;
        nSum = 0;
        //process attention (with calc average data)
        while (nIdx < cIdx) {
          myFile.print(aAttention[nIdx]);
          if (nIdx < cIdx-1) {
            myFile.print(",");
          } else {
            myFile.print(";");
          }
          nSum = nSum + aAttention[nIdx];
          nIdx++;
        }
        nAvgAtt = nSum / cIdx;
        //process meditation (with calc average data)
        nIdx = 0;
        nSum = 0;
        while (nIdx < cIdx) {
          myFile.print(aMeditation[nIdx]);
          if (nIdx < cIdx-1) {
            myFile.print(",");
          } else {
            myFile.print(";");
          }
          nSum = nSum + aMeditation[nIdx];
          nIdx++;
        }
        nAvgMed = nSum / cIdx;
        myFile.println(" ");
        myFile.close();
        // save average data to file
        myFile = SD.open(cFileName, FILE_WRITE);
        myFile.print(nTestNum);
        myFile.print(";");
        myFile.print(cIdx);
        myFile.print(";");
        myFile.print(nAvgAtt);
        myFile.print(";");
        myFile.print(nAvgMed);
        myFile.println(";");
        myFile.close();
        cIdx = 0;
        nTestNum++;
        delay(1000);
      }
    }
  }  

  // Look for sync bytes
  if (ReadOneByte() == 170) {
    if (ReadOneByte() == 170) {
      payloadLength = ReadOneByte();

      //Payload length can not be greater than 169
      if (payloadLength > 169) return;

      generatedChecksum = 0;        
      for (int i = 0; i < payloadLength; i++) {  
        payloadData[i] = ReadOneByte();            //Read payload into memory
        generatedChecksum += payloadData[i];
      }   
      checksum = ReadOneByte();                      //Read checksum byte from stream      
      generatedChecksum = 255 - generatedChecksum;   //Take one's compliment of generated checksum

      if (checksum == generatedChecksum) {    
       // poorQuality = 200;
        attention = 0;
        meditation = 0;
        bigPacket = false;  
        
        // Parse the payload
        for (int i = 0; i < payloadLength; i++) {    
          switch (payloadData[i]) {
          case 2:
            i++;            
            bigPacket = true;            
            break;
          case 4:
            i++;
            attention = payloadData[i];                        
            break;
          case 5:
            i++;
            meditation = payloadData[i];
            break;
          case 0x80:
            i = i + 3;
            break;
          case 0x83:
            i = i + 25;      
            break;
          default:
            break;
          }
        }

        // *** Add your code here ***
        if (bigPacket) {
          // put to OLED
          showData(0);
          // collect data
          if (bSave) {
            if (cIdx < 255) {
              aAttention[cIdx] = attention;
              aMeditation[cIdx] = meditation;
              cIdx++;
            }
          }
        }
      }
    }
  }
}
