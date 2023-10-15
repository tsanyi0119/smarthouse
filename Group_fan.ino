//MQTT_AP
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include "DHT.h"
#define SW D2
#define LED D3
#define SW2 D4
#define DHTPIN D8
#define DHTTYPE DHT11
int sensorPin = A0;
int sensorValue = 0;
bool APLastState = LOW;
bool APToggle = LOW;
bool FanLastState = LOW;
bool FanToggle = LOW;

bool APState = false;

const char *ssid = "Group_Fan" ; // Enter your WiFi name
const char *password = "12345678" ;   // Enter WiFi password
String homeSSID = "Gary";
String homePassWord = "0979900069";
String groupName = "";
String strTopic;
String sendValue;
int FanMode = -1;
float temp;
bool homeWifiState = false;

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io" ;
const char *topic = "/test" ;
const char *mqtt_username = "Group4" ;
const char *mqtt_password = "666666" ;
const int mqtt_port = 1883 ;

//millis
int startTime;
int Duration;
int ConnectTime = 10000;
int OffTime = 500;
int SendTime = 1000;

WiFiClient espClient;
WiFiServer server(80);
PubSubClient client(espClient) ;
DHT dht(DHTPIN, DHTTYPE);
int value;
int dir;
#define ENA D5
#define N1 D6
#define N2 D7

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(SW, INPUT);
  pinMode(SW2, INPUT);
  pinMode(ENA, OUTPUT);  //接到ENA
  pinMode(N1, OUTPUT);  //接到N1
  pinMode(N2, OUTPUT);  //接到N2
  digitalWrite(N1, HIGH);  //設定馬達正轉
  digitalWrite(N2, LOW);
  EEPROM.begin(1024);
  Serial.begin(115200);
  server.begin();
  dht.begin();
  temp = dht.readTemperature();
  startTime=millis();
  getHomeWifiValue();
//  strTopic = groupName + "_Fan/test";
  Serial.println(strTopic);
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
}

void loop() {
  Duration = millis() - startTime;
  if(Duration>=SendTime){
      temp = dht.readTemperature();
      String tempTopic = groupName + "_FanTemp/test";
      Serial.println(String(temp));
      char msg[String(temp).length()];
      String(temp).toCharArray(msg,String(temp).length());
      client. publish(tempTopic.c_str(),msg);
      startTime=millis();
  }
   
  if (WiFi.status() != WL_CONNECTED) {
    //沒連上homeWifi
    homeWifiState = false;
    //   Serial.println ("未連接homeWifi");
    no_connect_homeWifi_mqtt();
  } else {
    homeWifiState = true;
    //   Serial.println("已連上homeWifi");
    WiFi.softAPdisconnect(true);
    connect_homeWifi();
  }
  //開啟AP
  bool APbtn = digitalRead(SW);
  if (APbtn) {
    APLastState = APbtn;
  }
  if (APbtn != APLastState) {
    APToggle = !APToggle;
    if (APToggle == HIGH) {
      APState = true;
      digitalWrite(LED, HIGH);
      //開啟AP配對
      open_AP_Mode();
    } else {
      digitalWrite(LED, LOW);
      APState = false;
      WiFi.softAPdisconnect(true);
    }
    APLastState = LOW;
  }
  bool FanBtn = digitalRead(SW2);
  if (FanBtn) {
    FanLastState = FanBtn;
  }
  if (FanBtn != FanLastState) {
    FanMode += 1;
    FanLastState = LOW;
  }

  get_AP_Request();

  client. loop();
}
void open_AP_Mode() {
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
void off_AP_Mode() {

  getHomeWifiValue();
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
}
void get_AP_Request() {
  WiFiClient clientAP = server.available();
  if (!clientAP) {
    return;
  }
  Serial.println("new client");
  while (!clientAP.available()) {
    delay(1);
  }
  //讀取訊息
  String req = clientAP.readStringUntil('\r');
  clientAP.flush();
  int n = req.length();
  String req_wifiName;
  String req_wifiPassWord;
  //匹配請求
  if ( req.indexOf("/wifiname/") != -1) {
    Serial.println(req);
    req_wifiName = req.substring(14, n - 9);
    sendValue = String(req_wifiName.length()) + '@';
    Serial.println(sendValue);
    Write_String(0, sendValue);
    Write_String(101, req.substring(14, n - 9));
  } else if (req.indexOf("/wifipassword/") != -1) {
    Serial.println(req);
    req_wifiPassWord = req.substring(18, n - 9);
    sendValue = String(req_wifiPassWord.length()) + '@';
    Serial.println(sendValue);
    Write_String(11, sendValue);
    Write_String(201, req.substring(18, n - 9));
  }else if(req.indexOf("/groupname/")!= -1){
    //群組名稱
    Serial.println(req);
    req_wifiPassWord = req.substring(15,n-9);         //擷取http後段的資料
    sendValue = String(req_wifiPassWord.length())+'@';//將@加入WiFi密碼數量之後做為之後燒入ROM的定位點
    Serial.println(sendValue);
    Write_String(21,sendValue);                       //將變數sendValue燒入ROM中
    Write_String(301,req.substring(15,n-9));
  }else if (req.indexOf("/setwififinish/") != -1) {
    APToggle = !APToggle;
    digitalWrite(LED, APToggle);
    off_AP_Mode();
  } else {
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
void Write_String(int a, String str) {
  //把str所有数据逐个保存在EEPROM
  for (int i = 0; i < str.length(); i++) {
    EEPROM.write(a + i, str[i]);
  }
  EEPROM.commit();
}
//a起始位 b讀幾位
String Read_String(int a, int b) {
  String data = "";
  //从EEPROM中逐个取出每一位的值，并链接
  for (int i = 0; i < b; i++) {
    if (EEPROM.read(a + i) == '@') {
      break;
    } else {
      data += char(EEPROM.read(a + i));
    }
  }
  return data;
}
void CleanEEPROM() {
  for (size_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}
void connect_homeWifi() {
  //  Serial . println ( "Connected to the WiFi network" );
  //connecting to a mqtt broker
  client. setServer (mqtt_broker, mqtt_port);
  client. setCallback (callback);
  while (!client. connected()) {
    String client_id = "esp8266-client-" ;
    client_id += String ( WiFi . macAddress ());
    Serial . printf ( "The client %s connects to the public mqtt broker\n" , client_id. c_str ());
    if (client. connect (client_id. c_str (), mqtt_username, mqtt_password)) {
      Serial . println ( "Public emqx mqtt broker connected" );
    } else {
      Serial.println ("未連接MQTT");
      no_connect_homeWifi_mqtt();
    }
    //    client. publish (strTopic.c_str(), "hello emqx" );
    //    client. subscribe (strTopic.c_str());
//    client. publish (topic, "hello emqx" );
    strTopic = groupName + "_FanStatus/test";
    client. subscribe (strTopic. c_str ());
    strTopic = groupName + "_FanSpeed/test";
    client. subscribe (strTopic. c_str ());
    break;
  }
  if (client. connected()) {
    connect_homeWifi_mqtt();
  }
}
//未連上WiFi或MQTT要做的事
void no_connect_homeWifi_mqtt() {
  //  Serial.println ("未連上WiFi或MQTT要做的事");
  switch(FanMode) {
      case 0:
//        Serial.println(temp);
        Serial.println("未連上WiFi或MQTT溫控模式");
        if(temp >= 20 && temp <= 22){
          analogWrite(ENA, 100);
        }
        if(temp >= 23 && temp <= 25){
          analogWrite(ENA, 190);
        }
        if(temp >= 26){
          analogWrite(ENA, 255);
        }
        break;
      case 1:
        Serial.println("未連上WiFi或MQTT調速模式");
        if(APState == false){
          sensorValue = analogRead(sensorPin);   //讀取類比輸入的值會得到0~1023
          sensorValue = map(sensorValue, 0, 1023, 0, 255); //將0~1023轉化成0~255
          analogWrite(ENA, sensorValue);
        }        
        break;
      case 2:
        Serial.println("未連上WiFi或MQTT關閉");
        analogWrite(ENA, 0);
        FanMode = -1; 
        break;
      default:
        Serial.println("NUN");
    }
  

}
//連上MQTT要做的事
void connect_homeWifi_mqtt() {
  //  Serial.println ("連上MQTT要做的事");
  switch(FanMode) {
      case 0:
        Serial.println("溫控模式");
        if(temp >= 20 && temp <= 22){
          analogWrite(ENA, 100);
        }
        if(temp >= 23 && temp <= 25){
          analogWrite(ENA, 190);
        }
        if(temp >= 26){
          analogWrite(ENA, 255);
        }
        break;
      case 1:
        Serial.println("調速模式");
      
        break;
      case 2:
        Serial.println("關閉");
        analogWrite(ENA, 0);
        FanMode = -1; 
        break;
      default:
        Serial.println("NUN");
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
  
  if(modeString.indexOf("_FanStatus/test") != -1){
    if(MqttValue.toInt() == 0){
      //溫控模式
      FanMode = 0;
    }else if(MqttValue.toInt() == 1){
      //調速模式
      FanMode = 1;  
    }else if(MqttValue.toInt() == 2){
      //關閉
      FanMode = 2;
    }
  }else if(modeString.indexOf("_FanSpeed/test") != -1){
    analogWrite(ENA, MqttValue.toInt());
  }else{}
  Serial.println ();
  Serial.println ( "-----------------------" );
}
//讀取ROM資料副程式
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
