#include <ESP8266WiFi.h>
#include <DNSServer.h>  
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  
#include <PubSubClient.h>
#include <ESP8266mDNS.h>

//Inicializacao do WiFiManager
WiFiManager wifiManager;

//Inicializacao so servidor http na porta 80
WiFiServer server(80);

//Status da GPIO
uint8_t status_gpio = 0;

//Servidor MQTT
const char* mqtt_server = "192.168.137.96";

WiFiClient espClient;
PubSubClient pubSubClient(espClient);

void setup() {
    //Configura a serial
    Serial.begin(115200);
 
    //Configura a GPIO como saida
    pinMode(D7, OUTPUT);
    
    //Coloca a GPIO em sinal logico baixo
    digitalWrite(D7, LOW);
 
    //Define o auto connect e o SSID do modo AP
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.autoConnect();
  
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if (!MDNS.begin("esp8266")) {
      Serial.println("Error setting up MDNS responder!");
      while(1) { 
        delay(1000);
      }
    }
    Serial.println("mDNS responder started");
      
    //Log na serial se conectar
    Serial.println("Conectado");
    
    //Inicia o webserver de controle da GPIO
    server.begin(); 

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);  

    //Configure MQTT
    pubSubClient.setServer(mqtt_server, 1883);
    pubSubClient.setCallback(callback);  
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
    digitalWrite(D7, HIGH);
    status_gpio = HIGH;
  } else {
    digitalWrite(D7, LOW);
    status_gpio = LOW;
  }

}

void reset_config(void) {
  //Reset das definicoes de rede
  wifiManager.resetSettings();
  delay(1500);
  ESP.reset();
}

void reconnect() {
  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (pubSubClient.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      pubSubClient.publish("tomada/nome", "tomada 01001");
      // ... and resubscribe
      pubSubClient.subscribe("tomada/sala");
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() { 
  if (!pubSubClient.connected()) {
    reconnect();
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
    digitalWrite(D7, HIGH);
    status_gpio = HIGH;
  } else if (req.indexOf("rele_off") != -1) {
    digitalWrite(D7, LOW);
    status_gpio = LOW;
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
  if(status_gpio)
    buf += "<div><h4>Lampada</h4><a href=\"?function=rele_off\"><button>Desligar</button></a></div>";
  else
    buf += "<div><h4>Lampada</h4><a href=\"?function=rele_on\"><button>Ligar</button></a></div>";

  buf += "<div><a href=\"?function=reset_wifi\"><button>Resetar WiFi</button></a></div>";
  buf += "<h4>Criado por Pablo Rodrigol</h4>";
  buf += "</html>\n";
 
  //Envia a resposta para o cliente
  client.print(buf);
  client.flush();
  client.stop();
 
  Serial.println("Cliente desconectado!");
}
