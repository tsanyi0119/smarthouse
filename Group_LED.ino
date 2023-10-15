// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// Released under the GPLv3 license to match the rest of the
// Adafruit NeoPixel library
#include<ESP8266WiFi.h>
#include<PubSubClient.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#define PIN D5

#define NUMPIXELS 16

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels
int sensorPin = A0;
int sensorValue = 0; 
#define SW D2
#define LED D3
#define SW2 D4
int LedMode = -1;
bool APLastState = LOW;
bool APToggle = LOW;
bool LedLastState = LOW;
bool LedToggle = LOW;

const char *ssid = "Group_LED" ; // Enter your WiFi name 
const char *password = "12345678" ;   // Enter WiFi password
String homeSSID = "XXXXXXXXXX";
String homePassWord = "XXXXXXXXXX";
String groupName = "";
String strTopic;
String sendValue;
bool homeWifiState = false;

// MQTT Broker 
const char *mqtt_broker = "broker.emqx.io" ;
const char *topic = "/test" ;
const char *mqtt_username = "TSANYI" ;
const char *mqtt_password = "0119" ;
const int mqtt_port = 1883 ;

//millis
int startTime;
int Duration;
int ConnectTime=10000; //可自訂led On的時間
int OffTime=500; //可自訂led Off的時間

WiFiClient espClient;
WiFiServer server(80);
PubSubClient client(espClient) ;

void setup() {
  pinMode(SW,INPUT);
  pinMode(SW2,INPUT);
  pinMode(LED,OUTPUT);
  EEPROM.begin(1024);
  Serial.begin(115200);
  server.begin();
  getHomeWifiValue();
  strTopic = "groupName" +String(topic)+"";
  startTime=millis();
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear(); // Set all pixel colors to 'off'
}

void loop() {
  Duration=millis()-startTime;
  if(WiFi.status() != WL_CONNECTED) {
   //沒連上homeWifi
   homeWifiState = false;
   Serial.println ("未連接homeWifi");
   no_connect_homeWifi_mqtt();
  }else{
   homeWifiState = true;
   Serial.println("已連上homeWifi");
   WiFi.softAPdisconnect(true);
   connect_homeWifi();
  }
  //開啟AP
  bool APbtn = digitalRead(SW);
  if(APbtn){
    APLastState = APbtn;
  }
  if(APbtn != APLastState){
    APToggle = !APToggle;
    if(APToggle == HIGH){
      digitalWrite(LED,HIGH);
      //開啟AP配對
      open_AP_Mode();
    }else{
      digitalWrite(LED,LOW);
      WiFi.softAPdisconnect(true);
    }           
    APLastState = LOW; 
  }

  
  bool LedBtn = digitalRead(SW2);
  if(LedBtn){
    LedLastState =LedBtn;
  }
  if(LedBtn != LedLastState){
    LedMode += 1;      
    LedLastState = LOW; 
  }
  
  get_AP_Request();

  client. loop();
}
void open_AP_Mode(){
  //清空EEPROM
  CleanEEPROM();
  //設定IP及wifi模式為AP
  IPAddress Ip(192, 168, 255, 1);
  IPAddress NMask(255, 0, 0, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress MyIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(MyIP);
}
void off_AP_Mode(){
  
  getHomeWifiValue();
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
}
void get_AP_Request(){
  WiFiClient clientAP = server.available();
  if (!clientAP) {
    return;
  }
  Serial.println("new client");
  while(!clientAP.available()){
    delay(1);
  }
  //讀取訊息
  String req = clientAP.readStringUntil('\r');
  clientAP.flush();
  int n = req.length();
  String req_wifiName;
  String req_wifiPassWord;
  //匹配請求
  if ( req.indexOf("/wifiname/") != -1){
    Serial.println(req);
    req_wifiName = req.substring(14,n-9);
    sendValue = String(req_wifiName.length())+'@';
    Serial.println(sendValue);
    Write_String(0,sendValue);
    Write_String(101,req.substring(14,n-9));
  }else if(req.indexOf("/wifipassword/") != -1){
    Serial.println(req);
    req_wifiPassWord = req.substring(18,n-9);
    sendValue = String(req_wifiPassWord.length())+'@';
    Serial.println(sendValue);
    Write_String(11,sendValue);
    Write_String(201,req.substring(18,n-9));
  }else if(req.indexOf("/groupname/")!= -1){
    //群組名稱
    Serial.println(req);
    req_wifiPassWord = req.substring(15,n-9);         //擷取http後段的資料
    sendValue = String(req_wifiPassWord.length())+'@';//將@加入WiFi密碼數量之後做為之後燒入ROM的定位點
    Serial.println(sendValue);
    Write_String(21,sendValue);                       //將變數sendValue燒入ROM中
    Write_String(301,req.substring(15,n-9));
  }else if(req.indexOf("/setwififinish/") != -1){
    APToggle = !APToggle;           
    digitalWrite(LED,APToggle);
    off_AP_Mode();
  }else {
    Serial.println("invalid request");
    clientAP.stop();
    return;
  }
  clientAP.flush();
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n";
  s += "OK";
  clientAP.print(s);
  delay(10);
  Serial.println("Client disonnected");
  s += "</html>\n";
}
//a是起始位，str为要保存的字符串
void Write_String(int a,String str){
  //把str所有数据逐个保存在EEPROM
  for (int i = 0; i < str.length(); i++){
    EEPROM.write(a + i, str[i]);
  }
  EEPROM.commit();
}
//a起始位 b讀幾位
String Read_String(int a, int b){ 
  String data = "";
  //从EEPROM中逐个取出每一位的值，并链接
  for (int i = 0; i < b; i++){
    if(EEPROM.read(a + i)=='@'){
      break;
    }else{
      data += char(EEPROM.read(a + i));
    }
  }
  return data;
}
void CleanEEPROM(){
    for (size_t i = 0; i < EEPROM.length(); i++)
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}
void connect_homeWifi(){
//  Serial . println ( "Connected to the WiFi network" );
  //connecting to a mqtt broker 
  client. setServer (mqtt_broker, mqtt_port);
  client. setCallback (callback);
  while(!client. connected()){
    String client_id = "esp8266-client-" ;
    client_id += String ( WiFi . macAddress ());
    Serial . printf ( "The client %s connects to the public mqtt broker\n" , client_id. c_str ());
    if (client. connect (client_id. c_str (), mqtt_username, mqtt_password)) {
       Serial . println ( "Public emqx mqtt broker connected" );
    } else {
       Serial.println ("未連接MQTT");
       no_connect_homeWifi_mqtt();
    }
    strTopic = groupName + "_LightStatus/test";
    client. subscribe (strTopic. c_str ());
    strTopic = groupName + "_LightBright/test";
    client. subscribe (strTopic. c_str ());
    break;
  }
  if(client. connected()){
    connect_homeWifi_mqtt();
  }
}
//未連上WiFi或MQTT要做的事
void no_connect_homeWifi_mqtt(){
//  Serial.println ("未連上WiFi或MQTT要做的事");
    switch(LedMode) {
      case 0:
        Serial.println("檯燈模式");
        sensorValue = analogRead(sensorPin);   //讀取類比輸入的值會得到0~1023
        sensorValue = map(sensorValue,0,1023,0,100);  //將0~1023轉化成0~255
        for(int i=0; i<NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(255, 255, 255));
          pixels.setBrightness(sensorValue);
          pixels.show();
        }
        break;
      case 1:
        Serial.println("小夜燈模式");
        for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(204, 153, 0));
        pixels.setBrightness(100);
        pixels.show();
        }        
        break;
      case 2:
        Serial.println("關燈");
        for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
        pixels.setBrightness(0);
        pixels.show();
        }
        LedMode = -1; 
        break;
      default:
        Serial.println("NUN");
    }
}
//連上MQTT要做的事
void connect_homeWifi_mqtt(){
  Serial.println ("連上MQTT要做的事");
  switch(LedMode) {
      case 0:
        Serial.println("檯燈模式");
        break;
      case 1:
        Serial.println("小夜燈模式");
        for(int i=0; i<NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(204, 153, 0));
          pixels.setBrightness(100);
          pixels.show();
        }        
        break;
      case 2:
        Serial.println("關燈");
        for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
        pixels.setBrightness(0);
        pixels.show();
        }
        LedMode = -1; 
        break;
      default:delay(1);
//        Serial.println("NUN");
    }
}
//MQTT訂閱回傳
void callback(char *topic, byte *payload, unsigned  int length)  {
     String modeString(topic);
     Serial.print ( "Message arrived in topic: " );
     Serial.println (topic);
     Serial.print ( "Message:" );
     String MqttValue = "";
     for ( int i = 0 ; i < length; i++) {
       Serial.print (( char ) payload[i]);
       MqttValue += (char) payload[i];
     }
     if(modeString.indexOf("_LightStatus/test") != -1){
       if(MqttValue.toInt() == 0){
         //檯燈模式
         LedMode = 0;
       }else if(MqttValue.toInt() == 1){
         //小夜燈模式l
         LedMode = 1;  
       }else if(MqttValue.toInt() == 2){
         //關閉
         LedMode = 2;
       }
     }else if(modeString.indexOf("_LightBright/test") != -1){
        for(int i=0; i<NUMPIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(255, 255, 255));
          pixels.setBrightness(MqttValue.toInt());
          pixels.show();
        }
     }else{}
     Serial.println ();
     Serial.println ( "-----------------------" );
}
//讀取ROM資料
void getHomeWifiValue(){
  //讀取wifi名稱
  String s1=Read_String(101,Read_String(0,10).toInt());
  Serial.print("燒入wifi名稱：");
  Serial.println(s1);
  homeSSID = s1;
  //讀取wifi密碼
  String s2=Read_String(201,Read_String(11,20).toInt());
  Serial.print("燒入wifi密碼：");
  Serial.println(s2);
  homePassWord = s2;
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
  //讀取Group名稱
  String s3=Read_String(301,Read_String(21,30).toInt());
  Serial.print("燒入Group名稱：");
  Serial.println(s3);
  groupName = s3;
}
