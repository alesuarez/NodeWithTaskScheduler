#include <Arduino.h>
#include <TaskScheduler.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <CircularBuffer.h>

#include "secrets.h"

#define ESP8266_GPIO4             4
#define DOOR_OPEN                 1

void setupWifi();
void setupGpio();
void setupMqttBroker();

void sendBroker(String);
void send(String);

void keepWiFiAlive();
void checkDoorStatus();
void sendMqtt();
void keepMqttAlive();

char msg[25];

void callback(char *, byte *, unsigned int);

Task wifiTask(5000, TASK_FOREVER, &keepWiFiAlive);
Task checkDoorTask(1000, TASK_FOREVER, &checkDoorStatus);
Task sendMqttTask(3000, TASK_FOREVER, &sendMqtt);
Task mqttTask(5000, TASK_FOREVER, &keepMqttAlive);

WiFiClient  wifiClient;
PubSubClient client(wifiClient);

Scheduler scheduler;

boolean isConnectionAvailable = false;

CircularBuffer<String*, 5> buffer;

void setup() {
  Serial.begin(115200);
  
  setupWifi();
  setupMqttBroker();
  setupGpio();
  
  scheduler.init();

  scheduler.addTask(wifiTask);
  scheduler.addTask(checkDoorTask);
  scheduler.addTask(sendMqttTask);
  scheduler.addTask(mqttTask);

  wifiTask.enable();
  checkDoorTask.enable();
  sendMqttTask.enable();
  mqttTask.enable();
}

void loop() {
  scheduler.execute();
  client.loop();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  Serial.println("Trying to connect..");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void setupGpio() {
  Serial.println("Configurando GPIO");  
  pinMode(ESP8266_GPIO4, INPUT_PULLUP );        // Input pin.
  Serial.println("Configuracion de GPIO finalizada exitosamente");
}

void setupMqttBroker() {
  	Serial.println("Configurando cliente MQTT");  
  
  	client.setServer(mqtt_server, mqtt_port);
  	client.setCallback(callback);
  
  	Serial.println("Configuracion de cliente MQTT finalizada exitosamente");
  	Serial.println("\n\n\n");
}

void callback(char* topic, byte* payload, unsigned int length) {

	String incoming = "";

	for (unsigned int i = 0; i < length; i++) {
		incoming += (char)payload[i];
	}
  Serial.print("Mensaje recibido: ");
	Serial.print(topic);
	Serial.println("");
	incoming.trim();
  Serial.println("");
  Serial.println("");
  Serial.println(incoming);
  Serial.println("");
  Serial.println("");
}

void sendMqtt() {
  if (!buffer.isEmpty() && isConnectionAvailable) {
    String sendToBroker = *(buffer.pop());
    Serial.print("Send mqtt pepaso");
    Serial.println(sendToBroker);
    sendBroker(sendToBroker);
  }
}

void sendBroker(String text) {
    Serial.print("Sending to mqtt: ");
    Serial.println(text); 
		text.toCharArray(msg, 25);
		client.publish(root_topic_publish_front_door_status, msg);
}

void send(String text) {
  if (buffer.isFull()) {
    buffer.pop();
  }
  String *newText = new String(text);
  Serial.print("newText");
  buffer.push(newText);
}

void checkDoorStatus() {
  static int previusStatus = 0;
  int doorStatus = digitalRead(ESP8266_GPIO4);
  Serial.println(doorStatus);
  if (doorStatus ^ previusStatus) {
    if (doorStatus == DOOR_OPEN) {
      send("The door is open");
    } else {
      send("The door is close");
    }
    previusStatus = doorStatus;
  }
  
}

void keepWiFiAlive() {
  static int count = 0;
  if (WiFi.status() != WL_CONNECTED) {
    isConnectionAvailable = false;
    Serial.println("Trying to connect..");
    if (count > 10) {
      WiFi.begin(ssid, password);
      count = 0;
    }
    count++;
  } else {
    isConnectionAvailable = true;
    Serial.println("Wifi connected");
    send("Wifi connected");
  }
}

void keepMqttAlive() {
  if (!client.connected()) {
    isConnectionAvailable = false;
    Serial.println("Mqtt not connected");
    String clientId = "IOTICOS_H_W_";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Conectado!");
      //if(client.subscribe(root_topic_subscribe, QoS_LEVEL)){
      if(client.subscribe(root_topic_subscribe)){
        Serial.println("Suscripcion ok");
        isConnectionAvailable = true;
      } else {
        Serial.println("fallo Suscripciión");
      }
    } else {
      Serial.println(" Intentamos de nuevo en 5 segundos");
    }
  }
  Serial.println("Mqtt is alive");
}