/**************************CrowPanel ESP32 HMI Display Example Code************************
Version     :	1.1
Suitable for:	CrowPanel ESP32 HMI Display
Product link:	https://www.elecrow.com/esp32-display-series-hmi-touch-screen.html
Code	  link:	https://github.com/Elecrow-RD/CrowPanel-ESP32-Display-Course-File
Lesson	link:	https://www.youtube.com/watch?v=WHfPH-Kr9XU
Description	:	The code is currently available based on the course on YouTube, 
				        if you have any questions, please refer to the course video: Introduction 
				        to ask questions or feedback.
**************************************************************/


#include <Wire.h>
#include <SPI.h>
#include <PCA9557.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


/**************************LVGL and UI************************
if you want to use the LVGL demo. you need to include <demos/lv_demos.h> and <examples/lv_examples.h>. 
if not, please do not include it. It will waste your Flash space.
**************************************************************/
#include <lvgl.h>
#include "ui.h"
// #include <demos/lv_demos.h>
// #include <examples/lv_examples.h>
/**************************LVGL and UI END************************/

/*******************************************************************************
   Config the display panel and touch panel in gfx_conf.h
 ******************************************************************************/
#include "gfx_conf.h"

#include "key.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];
static lv_disp_drv_t disp_drv;

PCA9557 Out;    //for touch timing init


/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
   uint32_t w = ( area->x2 - area->x1 + 1 );
   uint32_t h = ( area->y2 - area->y1 + 1 );

   tft.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);

   lv_disp_flush_ready( disp );

}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
   uint16_t touchX, touchY;
   bool touched = tft.getTouch( &touchX, &touchY);
   if( !touched )
   {
      data->state = LV_INDEV_STATE_REL;
   }
   else
   {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touchX;
      data->point.y = touchY;

      Serial.print( "Data x " );
      Serial.println( touchX );

      Serial.print( "Data y " );
      Serial.println( touchY );
   }
}

// Configuration du WiFi
const char* ssid = "Elie";
const char* password = "123456789";
bool wifiConnected = false;                   // État de la connexion WiFi
// Tableaux pour traduire les jours et mois
const char* jours[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
const char* jours_s[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};

int today = 0;

const char* mois[] = {"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"};
// Clé API et URL de base
const char* baseUrl = "https://api.openweathermap.org/data/3.0/onecall";
// Coordonnées GPS (exemple : Paris)
const float latitude = 43.365;
const float longitude = 1.787;

// Structure pour mapper les icônes
typedef struct {
    const char *icon;            // Nom de l'icône OpenWeatherMap
    const lv_img_dsc_t *img_src; // Source de l'image LVGL
} WeatherIconMap;

// Table de correspondance
const WeatherIconMap weather_icon_map[] = {
    {"01d", &ui_img_01d_png}, // 01d - assets/01d@2x.png
    {"01n", &ui_img_01n_png}, // 01n - assets/01n@2x.png
    {"02d", &ui_img_02d_png}, // 02d - assets/02d@2x.png
    {"02n", &ui_img_02n_png}, // 02n - assets/02n@2x.png
    {"03d", &ui_img_03d_png}, // 03d - assets/03d@2x.png
    {"03n", &ui_img_03n_png}, // 03n - assets/03n@2x.png
    {"04d", &ui_img_04d_png}, // 04d - assets/04d@2x.png
    {"04n", &ui_img_04n_png}, // 04n - assets/04n@2x.png
    {"09d", &ui_img_09d_png}, // 09d - assets/09d@2x.png
    {"09n", &ui_img_09n_png}, // 09n - assets/09n@2x.png
    {"10d", &ui_img_10d_png}, // 10n - assets/10n@2x.png
    {"10n", &ui_img_10n_png}, // 10n - assets/10n@2x.png
    {"11d", &ui_img_11d_png}, // 11d - assets/11d@2x.png
    {"11n", &ui_img_11n_png}, // 11n - assets/11n@2x.png
    {"13d", &ui_img_13d_png}, // 13d - assets/13d@2x.png
    {"13n", &ui_img_13n_png}, // 13n - assets/13n@2x.png
    {"50d", &ui_img_50d_png}, // 50d - assets/50d@2x.png
    {"50n", &ui_img_50n_png},  // 50n - assets/50n@2x.png
};

const lv_img_dsc_t* get_weather_icon(const char *icon_code) {
    // Parcourir la table de correspondance
    for (size_t i = 0; i < sizeof(weather_icon_map) / sizeof(weather_icon_map[0]); i++) {
        if (strcmp(weather_icon_map[i].icon, icon_code) == 0) {
            return weather_icon_map[i].img_src; // Retourne la source de l'image
        }
    }
    return NULL; // Retourne NULL si aucune correspondance n'est trouvée
}

// { 0xDC, 0xDA, 0x0C, 0x58, 0xC9, 0x34 };
// Adresses MAC des capteurs
uint8_t macAddresses[][6] = {
    {0xC0, 0x49, 0xEF, 0xB3, 0x4F, 0xDC}, // Salon
    {0xCC, 0xDB, 0xA7, 0x9E, 0x28, 0x2C}, // Chambre Maloë
    {0xCC, 0xDB, 0xA7, 0x9E, 0x1F, 0xBC}, // Chambre Maloë
    //CC:DB:A7:9E:1F:BC
    // Ajoutez d'autres adresses ici
};

const int numPeers = sizeof(macAddresses) / sizeof(macAddresses[0]); // Nombre de pairs



typedef struct struct_message {
  float t;
  float h;
  float p;
} struct_message;

struct_message data;

lv_color_t getColorForTemperature(float temperature) {
    if (temperature < 18) {
        return lv_color_make(0, 0, 255);  // Bleu (RGB: 0, 0, 255)
    } else if (temperature >= 18 && temperature <= 22) {
        return lv_color_make(0, 255, 0);  // Vert (RGB: 0, 255, 0)
    } else if (temperature > 22 && temperature <= 26) {
        return lv_color_make(255, 255, 0);  // Jaune (RGB: 255, 255, 0)
    } else {
        return lv_color_make(255, 0, 0);  // Rouge (RGB: 255, 0, 0)
    }
}

unsigned long lastDataReceivedTime = 0; // Temps de la dernière réception de données
const unsigned long timeoutDuration = 5000; // Délai avant de considérer le capteur déconnecté (en millisecondes)
bool sensorConnected = false; // État du capteur

unsigned long lastDataReceivedTime2 = 0; // Temps de la dernière réception de données
const unsigned long timeoutDuration2 = 5000; // Délai avant de considérer le capteur déconnecté (en millisecondes)
bool sensorConnected2 = false; // État du capteur

unsigned long lastDataReceivedTime3 = 0; // Temps de la dernière réception de données
const unsigned long timeoutDuration3 = 5000; // Délai avant de considérer le capteur déconnecté (en millisecondes)
bool sensorConnected3 = false; // État du capteur


/*void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&data, incomingData, sizeof(data));

    // Mise à jour de l'horodatage
    lastDataReceivedTime = millis();

    // Si le capteur était déconnecté, on le marque comme connecté et met à jour la couleur
    if (!sensorConnected) {
        sensorConnected = true;
        lv_obj_set_style_bg_color(ui_status_dot, lv_color_make(0, 255, 0), 0); // Vert pour connecté
        Serial.println("Capteur reconnecté !");
    }
  
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Float: ");
  Serial.println(data.t);
  Serial.print("Float: ");
  Serial.println(data.h);
  Serial.print("Float: ");
  Serial.println(data.p);
  Serial.println();

  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%.1f", data.t);
  lv_label_set_text(ui_temp1, tempStr); 
  lv_obj_set_style_text_color(ui_temp1, getColorForTemperature(data.t), 0); // Appliquer la couleur de la température

  int humInt = (int) data.h;
  snprintf(tempStr, sizeof(tempStr), "%d", humInt);
  lv_label_set_text(ui_humidity1, tempStr);

  int pInt = (int) data.p;
  snprintf(tempStr, sizeof(tempStr), "%d", pInt);
  lv_label_set_text(ui_altitude1, tempStr);

}*/

char* replace_special_characters(const char* str) {
    if (str == NULL) return NULL;

    // Allouer une nouvelle chaîne pour le résultat
    size_t length = strlen(str);
    char* result = (char*)malloc((length + 1) * sizeof(char));
    if (result == NULL) return NULL; // Gestion de l'échec d'allocation

    // Parcourir la chaîne source et copier/modifier les caractères
    for (size_t i = 0; i < length; i++) {
        switch (str[i]) {
            case 'é': case 'è': case 'ê': case 'ë':
                result[i] = 'e';
                break;
            case 'à': case 'â': case 'ä':
                result[i] = 'a';
                break;
            case 'ù': case 'û': case 'ü':
                result[i] = 'u';
                break;
            case 'î': case 'ï':
                result[i] = 'i';
                break;
            case 'ô': case 'ö':
                result[i] = 'o';
                break;
            case 'ç':
                result[i] = 'c';
                break;
            default:
                result[i] = str[i];
                break;
        }
    }

    // Ajouter le caractère nul à la fin
    result[length] = '\0';
    return result;
}


void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connexion au WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    wifiConnected = true;
    Serial.println("\nWiFi connecté !");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());
}

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;    // Fuseau horaire : +1 heure pour la France
const int daylightOffset_sec = 3600; // Décalage pour l'heure d'été (1 heure)

// Fonction pour initialiser l'heure
void initTime() {
    Serial.println("Synchronisation avec le serveur NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Échec de la synchronisation NTP");
        return;
    }
    Serial.println("Heure synchronisée !");
}

void refreshTime()
{
  if(!wifiConnected) return;
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Impossible d'obtenir l'heure locale");
        return;
    }
    char hourStr[64];
    strftime(hourStr, sizeof(hourStr), "%H", &timeInfo);
    lv_label_set_text(ui_Label5, hourStr);

    char minStr[64];
    strftime(minStr, sizeof(minStr), "%M", &timeInfo);
    lv_label_set_text(ui_Label2, minStr);

    char dateStr[64];

    today = timeInfo.tm_wday;

    snprintf(dateStr, sizeof(dateStr), "%s %d %s",
             jours[timeInfo.tm_wday],    // Nom du jour (en français)
             timeInfo.tm_mday,          // Jour du mois
             mois[timeInfo.tm_mon]);    // Nom du mois (en français)

    lv_label_set_text(ui_Label4, dateStr); // Mise à jour du texte du label
}

void queryWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(baseUrl) + 
                     "?lat=" + String(latitude) + 
                     "&lon=" + String(longitude) + 
                     "&lang=fr&units=metric&exclude=minutely,hourly&appid=" + String(apiKey);

        Serial.println("Envoi de la requête : ");
        Serial.println(url);

        http.begin(url); // Initialiser la connexion
        int httpResponseCode = http.GET(); // Effectuer la requête GET

        if (httpResponseCode == 200) { // Code 200 = succès
            String response = http.getString();
            Serial.println("Réponse reçue :");
            Serial.println(response);

            // Traitement du JSON
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, response);

            if (error) {
                Serial.print("Erreur de parsing JSON : ");
                Serial.println(error.c_str());
                return;
            }

            // Extraction des données
            float temp = doc["current"]["temp"]; // Température actuelle
            float windSpeed = doc["current"]["wind_speed"]; // Vitesse du vent

            const char* icon = doc["current"]["weather"][0]["icon"]; // Description météo

            const char* icon1 = doc["daily"][1]["weather"][0]["icon"]; // Description météo
            const char* icon2 = doc["daily"][2]["weather"][0]["icon"]; // Description météo
            const char* icon3 = doc["daily"][3]["weather"][0]["icon"]; // Description météo
            const char* icon4 = doc["daily"][4]["weather"][0]["icon"]; // Description météo

            char temperature1[20]; // Tableau pour contenir le résultat
            char temperature1_final[20]; // Tableau pour contenir le résultat
            float temp1 = doc["daily"][1]["temp"]["day"]; // Température actuelle
            float temp1_min = doc["daily"][1]["temp"]["min"]; // Température actuelle
            float temp1_max = doc["daily"][1]["temp"]["max"]; // Température actuelle
            sprintf(temperature1, "%.0f°C", temp1);
            sprintf(temperature1_final, "%.0f / %.0f°c", temp1_min, temp1_max);
            lv_label_set_text(ui_Label9, temperature1); 
            lv_label_set_text(ui_Label10, temperature1_final); 

            float temp2 = doc["daily"][2]["temp"]["day"]; // Température actuelle
            float temp2_min = doc["daily"][2]["temp"]["min"]; // Température actuelle
            float temp2_max = doc["daily"][2]["temp"]["max"]; // Température actuelle
            sprintf(temperature1, "%.0f°C", temp2);
            sprintf(temperature1_final, "%.0f / %.0f°c", temp2_min, temp2_max);
            lv_label_set_text(ui_Label12, temperature1); 
            lv_label_set_text(ui_Label13, temperature1_final); 

            float temp3 = doc["daily"][3]["temp"]["day"]; // Température actuelle
            float temp3_min = doc["daily"][3]["temp"]["min"]; // Température actuelle
            float temp3_max = doc["daily"][3]["temp"]["max"]; // Température actuelle
            sprintf(temperature1, "%.0f°C", temp3);
            sprintf(temperature1_final, "%.0f / %.0f°c", temp3_min, temp3_max);
            lv_label_set_text(ui_Label15, temperature1); 
            lv_label_set_text(ui_Label16, temperature1_final); 

            float temp4 = doc["daily"][4]["temp"]["day"]; // Température actuelle
            float temp4_min = doc["daily"][4]["temp"]["min"]; // Température actuelle
            float temp4_max = doc["daily"][4]["temp"]["max"]; // Température actuelle
            sprintf(temperature1, "%.0f°C", temp4);
            sprintf(temperature1_final, "%.0f / %.0f°c", temp4_min, temp4_max);
            lv_label_set_text(ui_Label19, temperature1_final); 
            lv_label_set_text(ui_Label18, temperature1); 

            
            lv_label_set_text(ui_Label8, jours_s[((today+2)%7)]); 
            lv_label_set_text(ui_Label11, jours_s[((today+3)%7)]); 
            lv_label_set_text(ui_Label14, jours_s[((today+4)%7)]); 
            lv_label_set_text(ui_Label17, jours_s[((today+5)%7)]); 


            const char* desc = doc["current"]["weather"][0]["description"]; // Description météo

            

            char temperature[20]; // Tableau pour contenir le résultat

            // Convertir le float en string sans chiffres après la virgule
            sprintf(temperature, "%.0f", temp);
             
            lv_label_set_text(ui_Label6, temperature); 
            lv_label_set_text(ui_Label7, replace_special_characters(desc)); 
            lv_label_set_text(ui_Label3, " Avignonet-Lauragais, Haute Garonne"); 

            
            const lv_img_dsc_t *img_src = get_weather_icon(icon);
            lv_img_set_src(ui_Image1, img_src);

            lv_img_set_src(ui_Image2, get_weather_icon(icon1));
            lv_img_set_src(ui_Image3, get_weather_icon(icon2));
            lv_img_set_src(ui_Image4, get_weather_icon(icon3));
            lv_img_set_src(ui_Image5, get_weather_icon(icon4));






            // Affichage
            Serial.println("Données météo :");
            Serial.printf("Température : %.1f °C\n", temp);
            Serial.printf("Conditions : %s\n", replace_special_characters(desc));
            Serial.printf("Vent : %.1f m/s\n", windSpeed);
        } else {
            Serial.print("Erreur HTTP : ");
            Serial.println(httpResponseCode);
        }

        http.end(); // Fermer la connexion
    } else {
        Serial.println("WiFi non connecté !");
    }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    memcpy(&data, incomingData, sizeof(data));

        // Mise à jour de l'horodatage

    // Afficher l'adresse MAC de l'expéditeur
    Serial.print("Data received from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Identifier l'expéditeur
    if (memcmp(mac, macAddresses[0], 6) == 0) {
          lastDataReceivedTime = millis();

    // Si le capteur était déconnecté, on le marque comme connecté et met à jour la couleur
    if (!sensorConnected) {
        sensorConnected = true;
        lv_obj_set_style_bg_color(ui_status_dot, lv_color_make(0, 255, 0), 0); // Vert pour connecté
        Serial.println("Capteur reconnecté !");
    }

      Serial.println("Data from: Salon");
      char tempStr[16]; 
      snprintf(tempStr, sizeof(tempStr), "%.1f", data.t);
      lv_label_set_text(ui_temp1, tempStr); 
      lv_obj_set_style_text_color(ui_temp1, getColorForTemperature(data.t), 0); // Appliquer la couleur de la température

      int humInt = (int) data.h;
      snprintf(tempStr, sizeof(tempStr), "%d", humInt);
      lv_label_set_text(ui_humidity1, tempStr);

      int pInt = (int) data.p;
      snprintf(tempStr, sizeof(tempStr), "%d", pInt);
      lv_label_set_text(ui_altitude1, tempStr);

    } else if (memcmp(mac, macAddresses[1], 6) == 0) {
          lastDataReceivedTime2 = millis();

    // Si le capteur était déconnecté, on le marque comme connecté et met à jour la couleur
    if (!sensorConnected2) {
        sensorConnected2 = true;
        lv_obj_set_style_bg_color(ui_status_dot2, lv_color_make(0, 255, 0), 0); // Vert pour connecté
        Serial.println("Capteur reconnecté !");
    }

      Serial.println("Data from: Chambre Maloë");
      char tempStr[16]; 
      snprintf(tempStr, sizeof(tempStr), "%.1f", data.t);
      lv_label_set_text(ui_temp, tempStr); 
      lv_obj_set_style_text_color(ui_temp, getColorForTemperature(data.t), 0); // Appliquer la couleur de la température

      int humInt = (int) data.h;
      snprintf(tempStr, sizeof(tempStr), "%d", humInt);
      lv_label_set_text(ui_humidity, tempStr);

      int pInt = (int) data.p;
      snprintf(tempStr, sizeof(tempStr), "%d", pInt);
      lv_label_set_text(ui_altitude, tempStr);

    } else if (memcmp(mac, macAddresses[2], 6) == 0) {
          lastDataReceivedTime3 = millis();

    // Si le capteur était déconnecté, on le marque comme connecté et met à jour la couleur
    if (!sensorConnected3) {
        sensorConnected3 = true;
        lv_obj_set_style_bg_color(ui_status_dot1, lv_color_make(0, 255, 0), 0); // Vert pour connecté
        Serial.println("Capteur reconnecté !");
    }

      Serial.println("Data from: Chambre Parentale");
      char tempStr[16]; 
      snprintf(tempStr, sizeof(tempStr), "%.1f", data.t);
      lv_label_set_text(ui_temp2, tempStr); 
      lv_obj_set_style_text_color(ui_temp2, getColorForTemperature(data.t), 0); // Appliquer la couleur de la température

      int humInt = (int) data.h;
      snprintf(tempStr, sizeof(tempStr), "%d", humInt);
      lv_label_set_text(ui_humidity2, tempStr);

      int pInt = (int) data.p;
      snprintf(tempStr, sizeof(tempStr), "%d", pInt);
      lv_label_set_text(ui_altitude2, tempStr);

    }
}

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

void setup()
{
  Serial.begin(9600);
  Serial.println("LVGL Widgets Demo");



  //GPIO init
#if defined (CrowPanel_50) || defined (CrowPanel_70)
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);

  //touch timing init
  Wire.begin(19, 20);
  Out.reset();
  Out.setMode(IO_OUTPUT);
  Out.setState(IO0, IO_LOW);
  Out.setState(IO1, IO_LOW);
  delay(20);
  Out.setState(IO0, IO_HIGH);
  delay(100);
  Out.setMode(IO1, IO_INPUT);

#elif defined (CrowPanel_43)
  pinMode(20, OUTPUT);
  digitalWrite(20, LOW);
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);
  pinMode(35, OUTPUT);
  digitalWrite(35, LOW);
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  pinMode(0, OUTPUT);//TOUCH-CS
#endif

  //Display Prepare
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  delay(200);

  lv_init();

  delay(100);

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight/10);
  /* Initialize the display */
  lv_disp_drv_init(&disp_drv);
  /* Change the following line to your display resolution */
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.full_refresh = 1;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /* Initialize the (dummy) input device driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  tft.fillScreen(TFT_BLACK);

  //please do not use LVGL Demo and UI export from Squareline Studio in the same time.
  // lv_demo_widgets();    // LVGL demo
  ui_init();

  int32_t channel = 6;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  connectToWiFi();

// Configuration ESP-NOW
    WiFi.mode(WIFI_AP_STA); // Mode station
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Ajouter tous les pairs
    /*for (int i = 0; i < numPeers; i++) {
        esp_now_peer_info_t peerInfo;
        memcpy(peerInfo.peer_addr, macAddresses[i], 6);
        peerInfo.channel = 6; // Canal 0 (par défaut)
        peerInfo.encrypt = false;

        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.print("Failed to add peer: ");
            for (int j = 0; j < 6; j++) {
                Serial.printf("%02X", macAddresses[i][j]);
                if (j < 5) Serial.print(":");
            }
            Serial.println();
        } else {
            Serial.print("Added peer: ");
            for (int j = 0; j < 6; j++) {
                Serial.printf("%02X", macAddresses[i][j]);
                if (j < 5) Serial.print(":");
            }
            Serial.println();
        }
    }*/
    
    // Enregistrer le callback pour recevoir des données
  esp_now_register_recv_cb(OnDataRecv);

  

  Serial.print("L'adresse MAC de l'ESP32 est : ");
  Serial.println(WiFi.macAddress());
  
  delay(1000);

  initTime();


  queryWeather();

  Serial.println(getWiFiChannel(ssid));

  Serial.println( "Setup done" );

  //WiFi.disconnect(); 
  //Serial.println("Wi-Fi déconnecté");

}

void loop()
{
    lv_timer_handler();
    delay(5);


    // Vérification du timeout
    if (millis() - lastDataReceivedTime > timeoutDuration) {
        if (sensorConnected) {
            // Le capteur semble déconnecté
            sensorConnected = false;
            Serial.println("Le capteur ne répond plus!");

            // Mettre à jour l'indicateur de statut
            lv_obj_set_style_bg_color(ui_status_dot, lv_color_make(255, 0, 0), 0); // Rouge pour déconnecté

            // Optionnel : afficher un message d'erreur
            //lv_label_set_text(ui_temp1, "Erreur: Pas de données");
            //lv_obj_set_style_text_color(ui_temp1, lv_color_make(255, 0, 0), 0); // Texte rouge
        }
    }

    if (millis() - lastDataReceivedTime2 > timeoutDuration2) {
        if (sensorConnected2) {
            // Le capteur semble déconnecté
            sensorConnected2 = false;
            Serial.println("Le capteur ne répond plus!");

            // Mettre à jour l'indicateur de statut
            lv_obj_set_style_bg_color(ui_status_dot2, lv_color_make(255, 0, 0), 0); // Rouge pour déconnecté

            // Optionnel : afficher un message d'erreur
            //lv_label_set_text(ui_temp1, "Erreur: Pas de données");
            //lv_obj_set_style_text_color(ui_temp1, lv_color_make(255, 0, 0), 0); // Texte rouge
        }
    }

        if (millis() - lastDataReceivedTime3 > timeoutDuration3) {
        if (sensorConnected3) {
            // Le capteur semble déconnecté
            sensorConnected3 = false;
            Serial.println("Le capteur ne répond plus!");

            // Mettre à jour l'indicateur de statut
            lv_obj_set_style_bg_color(ui_status_dot1, lv_color_make(255, 0, 0), 0); // Rouge pour déconnecté

            // Optionnel : afficher un message d'erreur
            //lv_label_set_text(ui_temp1, "Erreur: Pas de données");
            //lv_obj_set_style_text_color(ui_temp1, lv_color_make(255, 0, 0), 0); // Texte rouge
        }
    }

        // Vérification de la connexion WiFi
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            wifiConnected = false;
            Serial.println("Connexion WiFi perdue !");
        }
    } else {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.println("Connexion WiFi rétablie !");
        }
    }

    refreshTime();




}