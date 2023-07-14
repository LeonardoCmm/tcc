#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

#define WIFI_SSID "UFSM"
#define WIFI_PASSWORD ""

#define FIREBASE_HOST "link firebase"  
#define FIREBASE_AUTH "autenticação"

#define DHTPIN 4       // Define o pino de conexão do sensor DHT
#define DHTTYPE DHT11  // Define o tipo de sensor (DHT11 ou DHT22)

int ldr = 2;
int valorldr = 0;

int sensor1;
int sensor2;
int media_sensor;

int rele = 12;

unsigned long last_time;
FirebaseData fbdo;

const int numDados = 10;
int dados[numDados];
int dados2[numDados];

DHT_Unified dht(DHTPIN, DHTTYPE);
FirebaseData firebaseData;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

unsigned long intervaloColeta = 25 * 60;  // Intervalo de coleta em milissegundos (25 minutos)
unsigned long ultimaColeta = 0;                   // Variável para armazenar o tempo da última coleta
unsigned long timestamp;                           //obtem o tempo atual em forma de codigo
void setup() {
  esp_task_wdt_init(1 * 60 * 60, false);
  Serial.begin(115200);

  pinMode(rele, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  int timeout = 10;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    Serial.print(".");
    delay(1000);
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha na conexão Wi-fi. Reiniciando");
    delay(1000);
    ESP.restart();
  }

  Serial.println();
  Serial.print("Conectado ao WiFi, IP: ");
  Serial.println(WiFi.localIP());

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.RTDB.getInt(&fbdo, "dados/last_time");
  last_time = fbdo.intData();
  Serial.println(last_time);

  dht.begin();

  timeClient.begin();
  timeClient.update();
}

void loop() {

  timestamp = timeClient.getEpochTime();
  unsigned long deltaTime = timestamp - last_time;
  


  // Verifica se o intervalo de coleta foi atingido
  if (deltaTime>= intervaloColeta) {
    // Realiza as coletas e atualiza a variável ultimaColeta
    realizarColetas();
  Serial.println(timestamp);
  Serial.println(last_time);
  Serial.println(deltaTime);
  }

}


void realizarColetas() {
  int rele = 12;

  pinMode(rele, OUTPUT);
  timeClient.update();
 

  for (int i = 0; i < numDados; i++) {
    dados[i] = analogRead(32);
    dados2[i] = analogRead(33);
    Serial.println("Dados:");

    Serial.print("Dado ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(dados[i]);
    Serial.println(dados2[i]);

    delay(1000);  
  }

  int soma = 0;
  int soma2 = 0;
  for (int i = 0; i < numDados; i++) {
    soma += dados[i];
    soma2 += dados2[i];
  }
  float media = (float)soma / numDados;
  float media2 = (float)soma2 / numDados;
  media_sensor = ((media + media2) / 2);


  // Converte o valor lido em uma porcentagem de umidade (faça os ajustes necessários)
  float umidade_porcentagem = map(media_sensor, 2400, 1100, 0, 100);
  Serial.println(media_sensor);  // print the value in serial monitor
  Serial.println(umidade_porcentagem);

  if (media_sensor >= 1700) {  //valor minimo de umidade (definir)
    digitalWrite(rele, HIGH);  //relé desligado
    delay(200000);             //definir tempo de irrigação
  } else {
    digitalWrite(rele, LOW);  //relé ligado
  }

  digitalWrite(rele, LOW);

  sensors_event_t event;

  

  if (!isnan(media_sensor)) {
    FirebaseJson json;
    json.set("valor", umidade_porcentagem);
    json.set("dataHora", String(timestamp));                        
    Firebase.pushJSON(firebaseData, "/dados/umidade do solo", json);
  }

  dht.temperature().getEvent(&event);                             // Leitura da temperatura e amazena no objeto event
  if (!isnan(event.temperature)) {                                // verifica se o valor lido da umidade nao é invalido ou NaN (nao é um numero).                                                           
    FirebaseJson json;                                            //cria um objeto firbase para armazenar dados
    json.set("valor", event.temperature);                         //define o valor da umidade dentro do objeto
    json.set("dataHora", String(timestamp));                      // define a data e hora em uma string
    Firebase.pushJSON(firebaseData, "/dados/temperatura", json);  //evia os dados para o objeto
  }

  dht.humidity().getEvent(&event);  // Leitura da umidade
  if (!isnan(event.relative_humidity)) {
    FirebaseJson json;
    json.set("valor", event.relative_humidity);
    json.set("dataHora", String(timestamp));
    Firebase.pushJSON(firebaseData, "/dados/umidade", json);
  }


  last_time = timestamp;
  Firebase.RTDB.setInt(&fbdo, "dados/last_time", last_time);


  if (firebaseData.httpCode() == 200) {
    Serial.println("Dados enviados com sucesso para o Firebase!");
  } else {
    Serial.println(firebaseData.errorReason());
  }
}