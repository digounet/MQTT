#include <ESP8266WiFi.h>
#include <DNSServer.h>  
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  
#include <PubSubClient.h>

String MQTT_TOPIC = "mobiapps/";
char* MQTT_SERVER = "iot.eclipse.org";
uint8_t GPIO_PORT = D7;
String clientName = "IoT_Device_";
uint8_t gpioStatus = LOW;
String topicOut = "";

String macToStr(const uint8_t* mac);
void setupSerial();
void setupGpio();
void setupWifi();    
void setupHttpServer();
void setupMqtt();
void callback(char* topic, byte* payload, unsigned int length);

WiFiManager wifiManager;
WiFiServer server(80);
WiFiClient wifiClient;
PubSubClient pubSubClient(MQTT_SERVER, 1883, callback, wifiClient);

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

void setup() {
  setupSerial();
  setupGpio();
  setupWifi();    
  setupHttpServer();
  setupMqtt();
}

void setupSerial() {
  Serial.begin(115200);
}

void setupGpio() {
  pinMode(GPIO_PORT, OUTPUT);
  digitalWrite(GPIO_PORT, gpioStatus);  
}

void setupWifi() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  //clientName += "-";
  //clientName += String(micros() & 0xff, 16);

  clientName.replace(":","");

  Serial.println("");
  Serial.println("Client name: "+ clientName);
  Serial.println("");
  
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect(clientName.c_str());

  //Log na serial se conectar
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());        
}

void setupHttpServer() {
  server.begin();   
}

void setupMqtt() {
  pubSubClient.setServer(MQTT_SERVER, 1883);
  pubSubClient.setCallback(callback);    
  reconnectMqtt(); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  Serial.println((char)payload[0]);

  if ((char)payload[0] == '1') {
    digitalWrite(GPIO_PORT, HIGH);
    gpioStatus = HIGH;
  } else {
    digitalWrite(GPIO_PORT, LOW);
    gpioStatus = LOW;
  }

}

void reset_config(void) {
  //Reset das definicoes de rede
  wifiManager.resetSettings();
  delay(1500);
  ESP.reset();
}

void broadCastId(void) {
  if (pubSubClient.publish(topicOut.c_str(), clientName.c_str())) {
    Serial.println("Published: " + topicOut);
  } else {
    Serial.println("Publish failed: " + topicOut);
  }  
}

void reconnectMqtt() {
  if (pubSubClient.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");

    String topicIn = MQTT_TOPIC + clientName;
    topicOut = MQTT_TOPIC + "deviceName";
    
    if (pubSubClient.publish(topicOut.c_str(), clientName.c_str())) {
      Serial.println("Published: " + topicOut);
    } else {
      Serial.println("Publish failed: " + topicOut);
    }

    if (pubSubClient.subscribe(topicIn.c_str())) {
      Serial.println("Subscribed to: " + topicIn);      
    } else {
      Serial.println("Subscribe failed: " + topicIn);
    }
    
  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }
}

void loop() { 
  if (!pubSubClient.connected()) {
    reconnectMqtt();
  }
  pubSubClient.loop();
    
  //Aguarda uma nova conexao
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
 
  Serial.println("Nova conexao requisitada...");
 
  while(!client.available()){
    delay(1);
  }
 
  Serial.println("Nova conexao OK...");
 
  //Le a string enviada pelo cliente
  String req = client.readStringUntil('\r');
  //Mostra a string enviada
  Serial.println(req);
  //Limpa dados/buffer
  client.flush();
 
  //Trata a string do cliente em busca de comandos
  if (req.indexOf("rele_on") != -1){
    digitalWrite(GPIO_PORT, HIGH);
    gpioStatus = HIGH;
  } else if (req.indexOf("rele_off") != -1) {
    digitalWrite(GPIO_PORT, LOW);
    gpioStatus = LOW;
  } else if (req.indexOf("reset_wifi") != -1) {
    reset_config();  
  } else {
    Serial.println("Requisicao invalida");
  }
 
  //Prepara a resposta para o cliente
  String buf = "";
  buf += "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n";
  buf += "<html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\r\n";
  buf += "<title>WebServer ESP8266</title>";
  buf += "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:80%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;}</style>";
  buf += "</head>";
  buf += "<h3> ESP8266 Web Server</h3>";
 
  //De acordo com o status da GPIO envia o comando
  if(gpioStatus)
    buf += "<div><h4>Lampada</h4><a href=\"?function=rele_off\"><button>Desligar</button></a></div>";
  else
    buf += "<div><h4>Lampada</h4><a href=\"?function=rele_on\"><button>Ligar</button></a></div>";

  buf += "<div><a href=\"?function=broadCastId\"><button>Publicar ID</button></a></div>";
  buf += "<div><a href=\"?function=reset_wifi\"><button>Resetar WiFi</button></a></div>";
  buf += "<h4>Criado por Pablo Rodrigol</h4>";
  buf += "</html>\n";
 
  //Envia a resposta para o cliente
  client.print(buf);
  client.flush();
  client.stop();
 
  Serial.println("Cliente desconectado!");
}
