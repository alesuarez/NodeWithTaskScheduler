#include <Arduino.h>
#include <TaskScheduler.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <CircularBuffer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#include "secrets.h"
#include "setup.h"
#include "commons.h"
#include "time.h"

void setupWifi();
void setupGpio();
void setupMqttBroker();

void sendBroker(String);
void send(String);

void keepWiFiAlive();
void checkDoorStatus();
void sendMqtt();
void keepMqttAlive();

String getTimestamp() ;

void processStatus();
void processAlarm(int);
int getCmd(String);
int getTime(String);

void processMessage(String);

char msg[25];

void callback(char *, byte *, unsigned int);

Task wifiTask(5000, TASK_FOREVER, &keepWiFiAlive);
Task checkDoorTask(1000, TASK_FOREVER, &checkDoorStatus);
Task sendMqttTask(3000, TASK_FOREVER, &sendMqtt);
Task mqttTask(5000, TASK_FOREVER, &keepMqttAlive);

WiFiClient  wifiClient;
HTTPClient http;
PubSubClient client(wifiClient);

Scheduler scheduler;

boolean isConnectionAvailable = false; //todo: create a function

CircularBuffer<String*, 5> buffer;

void setup() {
  Serial.begin(115200);
  
  setupWifi();
  setupMqttBroker();
  setupGpio();
  
   Serial.println(getTimestamp());

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
  pinMode(SENSOR_PIN, INPUT_PULLUP );        // Input pin.
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
  processMessage(incoming);
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
  int doorStatus = digitalRead(SENSOR_PIN);
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

String getTimestamp() {
    String timeS = "";
    
    http.begin(wifiClient, "http://worldtimeapi.org/api/timezone/America/Argentina/Tucuman");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          int beginS = payload.indexOf("datetime");
          int endS = payload.indexOf("day_of_week");
          timeS = payload.substring(beginS + 11, endS - 3);    
    }
    return timeS;
}

void processMessage(String msg) {
    int cmd = getCmd(msg);
    switch (cmd) {
        case CMD_STATUS:
          Serial.println("status");
            processStatus();
            break;
        case CMD_ALARM:
            int time = getTime(msg);
            Serial.print("Alarm: ");
            Serial.println(time);
            processAlarm(time);
            break;
    }

}

void processStatus() {

}

String buildStatus() {
    String output;
    StaticJsonDocument<256> doc;

    doc["cmd"] = 0;
    doc["timeStamp"] = "2022-03-17T17:23:42.860254-05:00";

    JsonArray devices = doc.createNestedArray("devices");

    JsonObject devices_0 = devices.createNestedObject();
    devices_0["id"] = "A1";
    devices_0["status"] = 0;

    JsonObject devices_1 = devices.createNestedObject();
    devices_1["id"] = "B1";
    devices_1["status"] = 1;

    serializeJson(doc, output);

    return output;
}

void processAlarm(int time) {

}

int getCmd(String msg) {
  Serial.print("cmd: ");
  StaticJsonDocument<32> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return -1;
  }
  int cmd =  doc["cmd"];
  Serial.println(cmd);
  return cmd; // 0
}

int getTime(String msg) {
    return 0;
}