#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <EEPROM.h>
#include <WiFiManager.h>   

// Setup pins
const int hall_pin = 2;
const int pwm_pin = 5 ;
 
// Update these with values suitable for your network.
const char* ssid = "SpaceLoft";
const char* password = "MunOrBust";
const char* mqtt_server = "192.168.2.22";
const char* mqtt_server_user = "mqtt";
const char* mqtt_server_pass = "mqpass123";

// Hall effect sensor variables
bool hall_on_state = false;
int fullMotionHallEffectCount = 40;

// EEPROM values
int address = 0;

// Loop control
int loopCounter = 0;
int basicLoopDelay = 10;
const int loopCounterReset = 10000;

// Servo values
Servo myservo;

// State control variables
int position_variable = 0;
int desired_position = 0;

WiFiManager wifiManager;
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {

  // We start by connecting to a WiFi network

  Serial.println("STARTING!!!");
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (!strcmp(topic, "home/office/blinds/set"))
  {
    if(!strncmp((char *)payload, "OPEN", length)){
      Serial.println("Blinds Opening");
      desired_position = 0;
    }else if(!strncmp((char *)payload, "CLOSE", length)){
      Serial.println("Blinds Closing");
      desired_position = 100;
    }
  }
  if (!strcmp(topic, "home/office/blinds/set_position"))
  {
    Serial.print("Set position to - ");
    String strPayload((char *) payload);
    int pos = strPayload.toInt();
    Serial.println(pos);
    desired_position = pos;
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("office-blinds", mqtt_server_user, mqtt_server_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("home/office/blinds/availability","online");
      
      client.subscribe("home/office/blinds/set_position");
      client.subscribe("home/office/blinds/set");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void initialize_last_known_position(){
  EEPROM.begin(32);
  EEPROM.get(address, position_variable);
  Serial.print("Current position variable is - ");
  Serial.println(position_variable);
}

void save_current_position_in_flash(){
  EEPROM.put(address, position_variable);
  EEPROM.commit();  
}

void performHallCheck(){
  if (digitalRead(hall_pin)==0){
    if (hall_on_state==false){
        hall_on_state = true;
    }
  }
}

void performServoCheck(){
  if(desired_position > position_variable){
    myservo.write(180);
  }else if(desired_position < position_variable){
    myservo.write(0);
  }else{
    myservo.write(90);
  }
}

void publishAvailability(){
  client.publish("home/office/blinds/availability", "online");
}

void loopClient(){
  client.loop();
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  wifiManager.autoConnect("BlindsController", "thereisnopass");


  Serial.println("Starting Arduino!!!");
  myservo.attach(pwm_pin);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  //
  pinMode(hall_pin, INPUT);
}

void performLoopTaskAtInterval(int intervalInMs, void (*f)()){
  if(loopCounter * basicLoopDelay % intervalInMs == 0){
    (*f)();
  }
}

void loop()
{
  loopCounter++;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi, disconnected. Try again...");
    WiFi.reconnect();
  }
  if (!client.connected()) {
    reconnect();
  }
  performLoopTaskAtInterval(20000, publishAvailability);
  performLoopTaskAtInterval(1000, loopClient);
  performHallCheck();
  performServoCheck();
  
  if(loopCounter >= loopCounterReset){
    loopCounter = 0;
  }
  delay(basicLoopDelay);
}
