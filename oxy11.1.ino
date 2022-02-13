#include "timmer.h"
#include <M5Stack.h>
#include <time.h>
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <MQTTClient.h>
#include "WiFi.h"
#include <ArduinoJson.h>

#define RXD2 36
#define TXD2 35
#define LED 21
// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

const byte push1 = 23;
boolean segundo = false, WiFiConected;
byte muestraPPG = 0;
boolean  estadoled=false;
String mensaje;

uint8_t pack[5], imenu = 7, minutos = 0; 
unsigned int segundos = 0, milis;

byte signalppg[10];
byte n  = 1;


//Fecha y Hora Local
char ntpServer[] = "ntp.ign.gob.ar"; //servidor NTP del inst geografico nacional
const long gmtOffset_sec = -3 * 3600; // -3 es el valor obtenido de GMT y la zona
const int  daylightOffset_sec = 0;
struct tm timeinfo;
String dateStr, yerStr, monStr, datStr, horas;
String timeStr, horStr, minStr = "99", secStr;

//SD - File
File file;
const char* fname = "/check.txt";
bool SD_EXIST = false; 
bool SD_REC = false;
int flag_onTask = 0;

int g_idx = 0, d_idx = 0;

void getTimeFromNTP()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  while (!getLocalTime(&timeinfo)) {
    delay(1000);
  }
  getLocalTime(&timeinfo);
  yerStr = (String)(timeinfo.tm_year - 100);
  monStr = (String)(timeinfo.tm_mon + 1);
  if (timeinfo.tm_mon + 1 < 10) monStr = "0" + (String)(timeinfo.tm_mon + 1);
  datStr = (String)timeinfo.tm_mday ;
  if (timeinfo.tm_mday < 10) datStr = "0" + (String)timeinfo.tm_mday;
  horas = (String)timeinfo.tm_hour;
  //horas = timeinfo.tm_hour;
  if (timeinfo.tm_hour < 10) horStr = "0" + (String)timeinfo.tm_hour;
  //minStr = (String)timeinfo.tm_min;
  minutos = timeinfo.tm_min;
  if (timeinfo.tm_min < 10) minStr = "0" + (String)timeinfo.tm_min;
  //secStr = (String)timeinfo.tm_sec;
  segundos = timeinfo.tm_sec;
  if (timeinfo.tm_sec < 10) secStr = "0" + (String)timeinfo.tm_sec;

  dateStr = datStr + "/" + monStr + "/" + yerStr;
  timeStr = horStr + ":" + (String)minutos + ":" + (String)segundos;
}

void hora()
{
  segundos++;
  if (segundos == 60)
  {
    segundos = 0;
    minutos++;
  }
    if (minutos == 60) 
    {
      int hour = horas.toInt();
      minutos = 0;
      hour++;
      if (hour == 24)
      {
        hour = 0;
      }
      horas = (String)hour;
    }
  if(WiFiConected){
    timeStr = horas + ":" + (String)minutos + ":" + (String)segundos;
    M5.Lcd.setTextColor(WHITE,BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print(dateStr + " " + timeStr);
  } else {
    timeStr = (String)horas + ":" + (String)minutos + ":" + (String)segundos;
    M5.Lcd.setTextColor(WHITE,BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print(timeStr);
  }
  timeStr = (String)horas + ":" + (String)minutos + ":" + (String)segundos;
  M5.Lcd.setTextColor(WHITE,BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print(dateStr + " " + timeStr);
}

void writeData() 
{
  // Escribe en la tarjeta SD (modo de adición de archivos)  
  String tiempo = (String)horas+":"+(String)minutos+":"+(String)segundos; 
  file.println(tiempo+", " + (String)pack[4] + ", " +  (String)pack[3]+ ", " + (String)pack[1]) ;
}

//verifica el paquete de datos recibido por el puero serie desde el oximetro
byte verificar()
{
  byte serial_reset;
  if ((pack[0] > 128) && (pack[0]<210)){
    serial_reset = 0; 
  } else {
    serial_reset = 1;
  }
  return serial_reset;
}

void lectura() 
{
  if (Serial2.available()>4){
    Serial2.readBytes(pack,5);
    UpdateData();
  }
}

void menu(){
  imenu++;
  if(imenu>5){
    imenu=1; 
  }
}

void frec_value() //si el bit 6 del byte 3 esta en alto, se debe agregar al byte 4
{
  if ((pack[2] & 0x60) == 0x60)
  { 
    pack[3] = pack[3] | 0x80;    
  }
}

void UpdateData()  {
   
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor( 6 + muestraPPG, 240 - (int(pack[1]*83/100) + 240 - 215));
  M5.Lcd.println(".");
//  M5.Lcd.drawFastHLine(6 + muestraPPG, 240 - (int(pack[1]*83/100) + 240 - 220), 1, YELLOW);
  muestraPPG = muestraPPG + 1;
  
  if (muestraPPG >= 220) 
  { 
    muestraPPG = 0;
    M5.Lcd.fillRect(6, 133, 224, 90, BLACK); 
  }
  
  if (segundo){
    int8_t nivel = 0;
    M5.Lcd.setTextSize(8);
    M5.Lcd.fillRect(10, 70, 300, 65, BLACK);
    M5.Lcd.setTextColor(CYAN);
    if (pack[4] > 100)
    {
      M5.Lcd.setCursor( 31,  80); M5.Lcd.println("--");
    } else if (pack[4] == 100)
    {
      M5.Lcd.setCursor( 31,  80); M5.Lcd.println("99");  
    } else 
    {
      M5.Lcd.setCursor( 31,  80); M5.Lcd.println(pack[4]);
    }
    M5.Lcd.setTextColor(WHITE);

    frec_value();
    if ((pack[3] == 127) || (pack[3] > 250))
    {
      M5.Lcd.setCursor( 190, 80); M5.Lcd.println("--");
    } else 
    {
      M5.Lcd.setCursor( 190,  80); M5.Lcd.println(pack[3]);
    }
//    if (  contador_segundos >= 5)
//    {
//      //publishMessage(); No vamos a publicar esta vez en AWS
//      contador_segundos = 0;      
//    }
    segundo = false;      
  }  
  if (SD_REC && SD_EXIST)   writeData(); //actualizo datos en la SD                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
} 

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  //Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    //Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  //Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    //Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
   //Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  //Serial.println("AWS IoT Connected!");
  digitalWrite(LED,LOW);
  delay(2000);
  digitalWrite(LED,HIGH);
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["%Sp02"] = pack[4];
  doc["bpm"] = pack[3];
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

//  StaticJsonDocument<200> doc;
//  deserializeJson(doc, payload);
//  const char* message = doc["message"];
}

////////////////////SETUP////////////////////////

void setup() {
  
  //plantilla de la pantalla
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  M5.begin();
  M5.Power.begin();
  M5.Lcd.begin();
  M5.Lcd.clear(BLACK); // 320x240
  M5.Lcd.drawLine( 10, 30,310, 30,WHITE);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor( 10,  10); M5.Lcd.println("");
  M5.Lcd.setCursor(287,  10); M5.Lcd.println("SD");
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.setCursor( 41,  50); M5.Lcd.println("%SpO");
  M5.Lcd.setTextSize(1); M5.Lcd.setCursor( 41+48,  50+10); M5.Lcd.println("2");
  M5.Lcd.setTextSize(10); M5.Lcd.setCursor(31, 80); M5.Lcd.println("");
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(202,  50); M5.Lcd.println("PRbpm");
  M5.Lcd.setTextSize(10); M5.Lcd.setCursor(190, 80); M5.Lcd.println("");
  M5.Lcd.setTextSize(1);
  M5.Lcd.fillRect( 40, 224, 50, 10, 0xeeee33);
  M5.Lcd.fillRect(135, 224, 50, 10, 0xeeee33);
  M5.Lcd.fillRect(230, 224, 50, 10, 0xeeee33);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor( 70-25+5, 226); M5.Lcd.println("REC DATA");
  M5.Lcd.setCursor(160-15+3, 226); M5.Lcd.println("BOT B");
  M5.Lcd.setCursor(250-19+7, 226); M5.Lcd.println("BOT C");
  pinMode(LED, OUTPUT);
  
  //descomentando la siguiente línea se realiza la conexión a AWS, con las licencias correspondientes
  //connectAWS();
  
  setup_timmer();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //Serial.println("Connecting to Wi-Fi");
  milis = millis();
  while ((WiFi.status() != WL_CONNECTED)) 
  {
    delay(500);
    Serial.println(".");
    if ((millis() - milis) > 5000) //solo espera 5 segundos para la conexión  
    {
      WiFiConected = false;
      break; 
    }
    WiFiConected = true;
  }

  if (WiFiConected) 
  {
    getTimeFromNTP();
  } else {
    horas = "0";
    minutos = 0;
    segundos = 0;
  }

  delay(100);
  
  //setup timmer
  //setup bluethooth
  //setup_bluetooth();

  // Check SD card existance
  Serial.print("Checking SD card existance..");
  file = SD.open("/check.txt", FILE_WRITE);
  //Serial.println(file);
  if (file)
  {
    SD_EXIST = true;
    file. close();
    //Serial.println("Se detectó SD");
  }
  else
  {
    M5.Lcd.drawLine( 282,16,315,16, WHITE); // SD Error de lectura de tarjeta
    //Serial.println("Error de lectura/no existe SD");
  }
  M5.Lcd.setBrightness(5);
}

////////////////////MAIN////////////////////////

void loop() { 
  M5.update();
  lectura();
  //getTime();
  
  if (verificar())
  {
    byte a = Serial2.read();
    pack[0]=143;
  }
  
  if (interruptCounter > 0) 
  {
    incrementatimmer();
    segundo = true;
    hora();
   // Serial.println(segundos);
  }
  
  //confirmBT();
  if(M5.BtnA.pressedFor(3500))
  {
    SD_REC = !SD_REC;
    if (SD_REC && SD_EXIST)
    {
      M5.Lcd.fillCircle(276, 16, 6, RED); 
      //file = SD.open("/"+datStr+"-"+monStr+"-"+yerStr+".csv", FILE_WRITE);
      if (WiFiConected){
        file = SD.open("/"+datStr+"-"+monStr+"-"+yerStr+"-"+(String)millis()+".csv", FILE_WRITE);
      } else { 
        file = SD.open("/"+(String)millis()+".csv", FILE_WRITE);
        segundos = 0;
        minutos = 0;
        horas = "0";
      }
      
      file.println("TIEMPO, %Sp02, HR, PPG");
    } 
    else if (!SD_EXIST)
    {
      SD_REC = !SD_REC;
    }
    else 
    {
      M5.Lcd.fillCircle(276, 16, 6, BLACK); 
      file.close();
    }
    delay(1000);
  }
}

//Interrupciones
// Operación de botón B Para lectura en tarjeta
    
