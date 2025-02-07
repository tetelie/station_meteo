#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Adresse I²C des périphériques
#define BME280_ADDRESS 0x76 // Adresse du BME280
#define SCREEN_ADDRESS 0x3C // Adresse de l'écran OLED

// Définir les dimensions de l'écran OLED (128x64 ou 128x32)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Wifi
//station : DC:DA:0C:58:C9:34
uint8_t broadcastAddress[] = { 0xDC, 0xDA, 0x0C, 0x58, 0xC9, 0x34 };

typedef struct struct_message {
  float t;
  float h;
  float p;
} struct_message;

struct_message data;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Initialiser les objets pour le BME280 et l'écran OLED
Adafruit_BME280 bme;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  Wire.begin(); // Initialiser le bus I²C

  // Mettre l'ESP32 en mode station
  WiFi.mode(WIFI_STA);  
  int channel = 6;  // Canal Wi-Fi (choisissez celui que vous souhaitez)
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }


  // Enregistrer la fonction de rappel pour l'envoi
  esp_now_register_send_cb(OnDataSent);

  // Configurer l'ESP32 récepteur (peer) pour l'envoi
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Ajouter le peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Initialisation du BME280
  if (!bme.begin(BME280_ADDRESS)) {
    Serial.println("Erreur : BME280 introuvable !");
    //while (1);
  }

  // Initialisation de l'écran OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Erreur : Écran OLED introuvable !");
    while (1);
  }

  // Configuration initiale de l'écran
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initialisation...");
  display.display();
  delay(1000);
}

void loop() {
  // Lecture des données du BME280
  float temperature = bme.readTemperature();
  float humidite = bme.readHumidity();
  float pression = bme.readPressure() / 100.0F;

  // Affichage sur le moniteur série
  Serial.print("Température : ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidité : ");
  Serial.print(humidite);
  Serial.println(" %");
  Serial.print("Pression : ");
  Serial.print(pression);
  Serial.println(" hPa");

  // Affichage de la température en gros
  display.clearDisplay();
  display.setTextSize(4); // Texte très grand pour la température
  display.setCursor(0, 0); // En haut de l'écran
  display.print(temperature, 1); // Température avec 1 décimale
  display.println("C");

  // Affichage de l'humidité en taille moyenne
  display.setTextSize(2); // Texte moyen
  display.setCursor(0, 35); // Position sous la température
  display.print("   ");
  display.print(humidite, 1); // Humidité avec 1 décimale
  display.println("%");

  // Affichage de la pression en taille petite
  display.setTextSize(1); // Texte plus petit
  display.setCursor(0, 55); // Position en bas de l'écran
  display.print("   Pres: ");
  display.print(pression, 1); // Pression avec 1 décimale
  display.println(" hPa");

  display.display(); // Afficher tout
  delay(2000); // Pause avant la prochaine mise à jour

  // Appel de la fonction pour envoyer les données via ESP-NOW
  sendData(temperature, humidite, pression);

  Serial.print("L'adresse MAC de l'ESP32 est : ");
  Serial.println(WiFi.macAddress());
}

void sendData(float t, float h, float p) {
  Serial.println("sendData");
  data.t = t;
  data.h = h;
  data.p = p;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&data, sizeof(data));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  } else {
    Serial.println("Error sending the data");
  }
}
