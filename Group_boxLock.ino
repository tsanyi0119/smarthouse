//0-10
//11-20
//21-30
//101
//201
//301
//MQTT_AP
#include<ESP8266WiFi.h>
#include<PubSubClient.h>
#include <EEPROM.h>
#define apSW D2      //AP模式按鈕
#define apLED D3     //AP模式LED顯示
#define boxLock D5   //relay腳位

bool apLastState = LOW;
bool apToggle = LOW;
bool modeLastState = LOW;
bool modeToggle = LOW;

const char *ssid = "Group_Box" ; // Enter your WiFi name 
const char *password = "12345678" ;   // Enter WiFi password
String homeSSID = "XXXXXXXXX";
String homePassWord = "XXXXXXXXXX";
String groupName = "";
String strTopic;
String sendValue;
bool homeWifiState = false;
bool boxLockState = false;

// MQTT Broker 
const char *mqtt_broker = "broker.emqx.io" ;
const char *topic = "/test" ;
const char *mqtt_username = "Group4" ;
const char *mqtt_password = "666666" ;
const int mqtt_port = 1883 ;

//millis
int startTime;
int Duration;
int ONTime = 1500;
int ConnectTime=10000; 
int OffTime=500;

WiFiClient espClient;
WiFiServer server(80);
PubSubClient client(espClient) ;

void setup() {
  pinMode(apLED, OUTPUT);
  pinMode(boxLock, OUTPUT);
  EEPROM.begin(1024);
  Serial.begin(115200);
  server.begin();
  getHomeWifiValue();
  strTopic = groupName + "_Box/test";
  Serial.println(strTopic);
  WiFi.begin (homeSSID.c_str(), homePassWord.c_str());
}

void loop() {
  if(WiFi.status() != WL_CONNECTED) {
   //沒連上homeWifi
   homeWifiState = false;
//   Serial.println ("未連接homeWifi");
   no_connect_homeWifi_mqtt();
  }else{
   homeWifiState = true;
//   Serial.println("已連上homeWifi");
   WiFi.softAPdisconnect(true);
   connect_homeWifi();
  }
  //開啟/關閉 AP模式
  bool apBtn = digitalRead(apSW);
  if(apBtn){
    apLastState = apBtn;
  }
  if(apBtn != apLastState){
    apToggle = !apToggle;
    if(apToggle == HIGH){
      digitalWrite(apLED,HIGH);
      open_AP_Mode(); //開啟AP模式副程式
    }else{
      //關閉AP
      digitalWrite(apLED,LOW);
      WiFi.softAPdisconnect(true);
    }           
    apLastState = LOW; 
  }
  //讀取AP請求
  get_AP_Request();

  client. loop();
}
void open_AP_Mode(){
  //清空EEPROM
  CleanEEPROM();
  //設定IP及wifi模式為AP
  IPAddress Ip(192, 168, 255, 1);   //設定固定IP
  IPAddress NMask(255, 0, 0, 0);    //設定遮罩
  WiFi.softAPConfig(Ip, Ip, NMask); //建立接入點信息
  WiFi.mode(WIFI_AP);               //設定WiFi模式
  WiFi.softAP(ssid, password);      //設定WiFi名稱及密碼
  IPAddress MyIP = WiFi.softAPIP(); //取得設備IP
  Serial.print("AP IP address: ");
  Serial.println(MyIP);
}
//關閉AP副程式
void off_AP_Mode(){  
  //從EEPROM取得WiFi將要連線的WiFi名稱/密碼
  getHomeWifiValue();
  //連線至家中WiFi
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
  //讀取請求訊息
  String req = clientAP.readStringUntil('\r');
  clientAP.flush();
  int n = req.length();
  String req_wifiName;
  String req_wifiPassWord;
  //匹配請求
  if ( req.indexOf("/wifiname/") != -1){
    //WiFi名稱
    Serial.println(req);
    req_wifiName = req.substring(14,n-9);             //擷取http後段的資料
    sendValue = String(req_wifiName.length())+'@';    //將@加入WiFi名稱數量之後做為之後燒入ROM的定位點
    Serial.println(sendValue);
    Write_String(0,sendValue);                        //將變數sendValue燒入ROM中
    Write_String(101,req.substring(14,n-9));          //將WiFi名稱燒入ROM中
  }else if(req.indexOf("/wifipassword/") != -1){
    //WiFi密碼
    Serial.println(req);
    req_wifiPassWord = req.substring(18,n-9);         //擷取http後段的資料
    sendValue = String(req_wifiPassWord.length())+'@';//將@加入WiFi密碼數量之後做為之後燒入ROM的定位點
    Serial.println(sendValue);
    Write_String(11,sendValue);                       //將變數sendValue燒入ROM中
    Write_String(201,req.substring(18,n-9));          //將WiFi名稱燒入ROM中
  }else if(req.indexOf("/groupname/")!= -1){
    //群組名稱
    Serial.println(req);
    req_wifiPassWord = req.substring(15,n-9);         //擷取http後段的資料
    sendValue = String(req_wifiPassWord.length())+'@';//將@加入WiFi密碼數量之後做為之後燒入ROM的定位點
    Serial.println(sendValue);
    Write_String(21,sendValue);                       //將變數sendValue燒入ROM中
    Write_String(301,req.substring(15,n-9));
  }else if(req.indexOf("/setwififinish/") != -1){
    apToggle = !apToggle;
    //最後的請求          
    digitalWrite(apLED,apToggle);
    off_AP_Mode();                                    //呼叫關閉AP副程式
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

//ROM寫入副程式:a是起始位，str為要保存的字符串
void Write_String(int a,String str){
  //把str的數據一個個保存在EEPROM
  for (int i = 0; i < str.length(); i++){
    EEPROM.write(a + i, str[i]);
  }
  EEPROM.commit();
}

//ROM讀出副程式a是起始位，b將讀幾位
String Read_String(int a, int b){ 
  String data = "";
  //從EEPROM中逐个讀取出值，並串接
  for (int i = 0; i < b; i++){
    if(EEPROM.read(a + i)=='@'){
      break;
    }else{
      data += char(EEPROM.read(a + i));
    }
  }
  return data;
}

//清除ROM中的資料的副程式
void CleanEEPROM(){
    for (size_t i = 0; i < EEPROM.length(); i++)
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}
//WiFi連線處理副程式
void connect_homeWifi(){
//  Serial . println ( "Connected to the WiFi network" );
  //connecting to a mqtt broker 
  client. setServer (mqtt_broker, mqtt_port);
  client. setCallback (callback);
  while(!client. connected()){
    String client_id = "esp8266-client-" ;
    client_id += String (WiFi.macAddress());
    Serial.printf ( "The client %s connects to the public mqtt broker\n" , client_id. c_str ());
    if (client.connect (client_id. c_str (), mqtt_username, mqtt_password)) {
       Serial.println ( "Public emqx mqtt broker connected" );
    } else {
       //未連接MQTT
       Serial.println ("未連接MQTT");
       no_connect_homeWifi_mqtt();          //未連接MQTT時要做的副程式
    }
//    client. publish (topic, "hello emqx" ); //傳送MQTT
    client. subscribe (strTopic.c_str());              //訂閱MQTT 
    break;
  }
  if(client. connected()){
    connect_homeWifi_mqtt();                //連接MQTT時要做的副程式
  }
}
//未連上WiFi或MQTT要做的副程式
void no_connect_homeWifi_mqtt(){
//  Serial.println ("未連上MQTT要做的事");
}

//連上MQTT要做的副程式
void connect_homeWifi_mqtt(){
//  Serial.println ("連上MQTT要做的事");
    Duration=millis()-startTime;
    if(Duration>=ONTime && boxLockState==true){
      digitalWrite(boxLock,LOW);
      boxLockState = false;
    }
}

//MQTT訂閱回傳
void callback(char *topic,byte *payload,unsigned int length){
     Serial.print("Message arrived in topic:" );
     Serial.println (topic);
     Serial.print ("Message:");
     String MqttValue = "";
     for(int i = 0 ; i < length; i++){
       Serial.print((char) payload[i]);
       MqttValue += (char) payload[i];
     }
     Serial.println();
     Serial.println("-----------------------");

     //解鎖
     if(MqttValue.toInt() == 0){
        digitalWrite(boxLock,LOW);
        boxLockState = false;
     }else if(MqttValue.toInt() == 1){
        digitalWrite(boxLock,HIGH);
        boxLockState = true;
        startTime=millis();
     }else{}
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
