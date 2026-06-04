// Grupo 5: Tiziano Castro, Tomás Contreras y Tomas Pereyra

// Bibiliotecas
#include <DHT.h> //Sensor Temperatura
#include <Wire.h> // Pantalla
#include <Adafruit_GFX.h> // Pantalla
#include <Adafruit_SSD1306.h> // Pantalla
#include <Ticker.h> // Timer
#include <WiFi.h> // WiFi
#include <WiFiClientSecure.h> // WiFi
#include <UniversalTelegramBot.h> // Telegram
#include <ArduinoJson.h> // Telegram

// Tareas
TaskHandle_t tarea1; // Va en el core 0
TaskHandle_t tarea2; // Va en el core 1

typedef enum {
  RST,
  P1,
  P2,
  Bo1,
  Bo2,
  Espera1,
  Espera2,
  Espera3,
  Espera4,
  Espera5,
  Espera6,
} estadoPrograma;
estadoPrograma estadoActual = RST;

// Defines OLED
#define ANCHO 128
#define ALTO 64
Adafruit_SSD1306 display(ANCHO, ALTO, &Wire, -1);

// Defines DHT
#define DHTPIN 23
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Defines botones y led
#define B1 35
#define B2 34
#define ESPERA_PULSO 30
#define PARAMETRO_MILLIS 5000
#define pin_LED 26

// Defines Bot de Telegram, WiFi y Timer
WiFiClientSecure client;
#define BOTtoken "8624134445:AAHLQa7cK7h399IGNZjAqLxnIZvuHk7qqvs"
#define CHAT_ID "8691812814"
UniversalTelegramBot bot(BOTtoken, client);
Ticker timer;

// Variables Telegram y temperatura
int cantidadMensajes;
bool mensajeRepetido = false;
bool primerUmbral = true;
float verificarTemperaturaNumerica;
volatile float temperatura;  // es la temperatura actual
volatile int umbral; // es el valor umbral
volatile bool leerTemperatura = false;
volatile bool umbralSuperaTemperatura = false;

// Variables botones y tiempo
unsigned long tiempoPulso;
bool primerPulso;
unsigned long desfasaje;
int tiempoMillis;

// Variables WiFi
const char* ssid = "MECA-IoT-V2";
const char* password = "IoT$2026";

void setup() { // se usa para definir pines de leds, el serial begin y se crea las tareas
  Serial.begin(115200);

  // LED Y BOTONES
  pinMode(pin_LED, OUTPUT);
  pinMode(B1, INPUT);
  pinMode(B2, INPUT);

  // TIMER
  timer.attach(5, funcionTemperatura);

  // DHT
  dht.begin();

  // OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // WIFI Y BOT
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Conectando al WiFi...");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando...");
  }
  Serial.println("Conexión WiFi exitosa");
  if (bot.sendMessage(CHAT_ID, "El bot fue iniciado", "")) {
    Serial.println("Mensaje enviado");
  } else {
    Serial.println("Error enviando mensaje");
  }
  // TAREA 1
  xTaskCreatePinnedToCore(
    Loop1,          // función que ejecuta la tarea
    "Telegram",     // nombre interno
    8192,           // tamaño de stack (es suficiente)
    NULL,           // parámetros (no tiene)
    1,              // prioridad (es la misma en ambas tareas ya que ambas tienen la misma importancia)
    &tarea1,        // handle/referencia (nombre definido arriba)
    0               // core donde correrá
  );                // la lógica es la misma en la otra tarea

  // TAREA 2
  xTaskCreatePinnedToCore(
    Loop2,
    "MaquinaDeEstado",
    8192,
    NULL,
    1,
    &tarea2,
    1
  );
}

void Loop1(void * pvParameters) {
  for(;;) {

    if (umbralSuperaTemperatura == true) {
      if(mensajeRepetido == false) {
        bot.sendMessage(CHAT_ID, "La temperatura (" + String(temperatura) + " °C) superó el valor umbral (" + String(umbral) + " °C)", "");
        mensajeRepetido = true;
      } 
    } else {
        mensajeRepetido = false;
      }

    cantidadMensajes = bot.getUpdates(bot.last_message_received + 1);
    while (cantidadMensajes) {
      for (int i = 0; i < cantidadMensajes; i++)
      {
        String chat_id = String(bot.messages[i].chat_id);
        String mensaje = bot.messages[i].text;

        if (chat_id == CHAT_ID) {
          if (mensaje == "/t" || mensaje == "/T") {
            bot.sendMessage(chat_id, "Temperatura actual = " + String(temperatura) + " *C", "");
          }
          else if (mensaje == "/start") {
            bot.sendMessage(chat_id, "Para ver la temperatura actual, mandar /t o /T", "");
          }
        }
      }
      cantidadMensajes = bot.getUpdates(bot.last_message_received + 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  // Poner task delay al final
}

void Loop2(void * pvParameters) {
  for(;;){

    if (leerTemperatura == true) {
      leerTemperatura = false;
      verificarTemperaturaNumerica = dht.readTemperature();
      if (!isnan(verificarTemperaturaNumerica)) {
        temperatura = verificarTemperaturaNumerica;
        if (primerUmbral == true) {
          umbral = temperatura - 5;
          primerUmbral = false;
        }
      }
    }

    switch (estadoActual) {

    case (RST):
      tiempoPulso = 0;
      primerPulso = false;
      temperatura = 0;
      umbral = 25;
      desfasaje = millis();
      estadoActual = P1;
      break;


    case (P1):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("temperatura: " + String(temperatura) + " *C");
      display.print("valor umbral: " + String(umbral) + " *C");
      display.display();
      if (digitalRead(B1) == LOW && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso < ESPERA_PULSO) {
          if (digitalRead(B1) == HIGH) {
            primerPulso = false;
          }
        } else {
          primerPulso = false;
          tiempoMillis = 0;
          desfasaje = millis();
          estadoActual = Espera1;
        }
      }
      break;


    case (P2):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("valor umbral: " + String(umbral) + " *C");
      display.display();
      if (primerPulso == false) {
        if (digitalRead(B1) == LOW && digitalRead(B2) == HIGH) {
          tiempoPulso = millis();
          primerPulso = true;
        } else if (digitalRead(B1) == HIGH && digitalRead(B2) == LOW) {
          tiempoPulso = millis();
          primerPulso = true;
        } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
          tiempoPulso = millis();
          primerPulso = true;
        }
      } else {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == LOW && digitalRead(B2) == HIGH) {
            primerPulso = false;
            estadoActual = Bo1;
          } else if (digitalRead(B1) == HIGH && digitalRead(B2) == LOW) {
            primerPulso = false;
            estadoActual = Bo2;
          } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
            primerPulso = false;
            estadoActual = Espera6;
          }
        } else {
          if (digitalRead(B1) == HIGH && digitalRead(B2) == HIGH) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Bo1):
      if (primerPulso == false) {
        if (digitalRead(B1) == HIGH) {
          tiempoPulso = millis();
          primerPulso = true;
        } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
          tiempoPulso = millis();
          primerPulso = true;
        }
      } else {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == HIGH) {
            primerPulso = false;
            estadoActual = P2;
            umbral = umbral + 1;
          } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
            primerPulso = false;
            estadoActual = Espera6;
          }
        } else {
          if (digitalRead(B1) == LOW && digitalRead(B2) == HIGH) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Bo2):
      if (primerPulso == false) {
        if (digitalRead(B2) == HIGH) {
          tiempoPulso = millis();
          primerPulso = true;
        } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
          tiempoPulso = millis();
          primerPulso = true;
        }
      } else {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B2) == HIGH) {
            primerPulso = false;
            estadoActual = P2;
            umbral = umbral - 1;
          } else if (digitalRead(B1) == LOW && digitalRead(B2) == LOW) {
            primerPulso = false;
            estadoActual = Espera6;
          }
        } else {
          if (digitalRead(B1) == HIGH && digitalRead(B2) == LOW) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera1):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando a que se");
      display.print("suelte el botón 1...");
      display.display();
      tiempoMillis = millis() - desfasaje;
      if (tiempoMillis >= PARAMETRO_MILLIS) {
        estadoActual = P1;
      }
      if (digitalRead(B1) == HIGH && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == HIGH) {
            primerPulso = false;
            estadoActual = Espera2;
          }
        } else {
          if (digitalRead(B1) == LOW) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera2):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando a que se");
      display.print("presione el botón 2...");
      display.display();
      tiempoMillis = millis() - desfasaje;
      if (tiempoMillis >= PARAMETRO_MILLIS) {
        estadoActual = P1;
      }
      if (digitalRead(B2) == LOW && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B2) == LOW) {
            primerPulso = false;
            estadoActual = Espera3;
          }
        } else {
          if (digitalRead(B2) == HIGH) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera3):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando a que se");
      display.print("suelte el botón 2...");
      display.display();
      tiempoMillis = millis() - desfasaje;
      if (tiempoMillis >= PARAMETRO_MILLIS) {
        estadoActual = P1;
      }
      if (digitalRead(B2) == HIGH && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B2) == HIGH) {
            primerPulso = false;
            estadoActual = Espera4;
          }
        } else {
          if (digitalRead(B2) == LOW) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera4):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando a que se");
      display.print("presione el botón 1...");
      display.display();
      tiempoMillis = millis() - desfasaje;
      if (tiempoMillis >= PARAMETRO_MILLIS) {
        estadoActual = P1;
      }
      if (digitalRead(B1) == LOW && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == LOW) {
            primerPulso = false;
            estadoActual = Espera5;
          }
        } else {
          if (digitalRead(B1) == HIGH) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera5):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando a que se");
      display.print("suelte el botón 1...");
      display.display();
      tiempoMillis = millis() - desfasaje;
      if (tiempoMillis >= PARAMETRO_MILLIS) {
        estadoActual = P1;
      }
      if (digitalRead(B1) == HIGH && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == HIGH) {
            primerPulso = false;
            estadoActual = P2;
          }
        } else {
          if (digitalRead(B1) == LOW) {
            primerPulso = false;
          }
        }
      }
      break;


    case (Espera6):
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("esperando...");
      display.display();
      if (digitalRead(B1) == HIGH && digitalRead(B2) == HIGH && primerPulso == false) {
        tiempoPulso = millis();
        primerPulso = true;
      }
      if (primerPulso == true) {
        if (millis() - tiempoPulso >= ESPERA_PULSO) {
          if (digitalRead(B1) == HIGH && digitalRead(B2) == HIGH) {
            primerPulso = false;
            estadoActual = P1;
          }
        } else {
          if (digitalRead(B1) == LOW || digitalRead(B2) == LOW) {
            primerPulso = false;
          }
        }
      }
      break; 
    }
  if (umbral < temperatura) {
    digitalWrite(pin_LED, HIGH);
    umbralSuperaTemperatura = true;
  } else {
    digitalWrite(pin_LED, LOW);
    umbralSuperaTemperatura = false;
  }
  vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void loop() {}

void funcionTemperatura() {
  leerTemperatura = true;
}
