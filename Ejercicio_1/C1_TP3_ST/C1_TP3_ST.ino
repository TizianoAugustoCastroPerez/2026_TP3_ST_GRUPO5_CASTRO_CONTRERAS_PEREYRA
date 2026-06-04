// grupo 5: Tiziano Castro, Tomás Contreras y Tomas Pereyra

#define pin_LED_1 25
#define pin_LED_2 26

TaskHandle_t tarea1; // Va en el core 0
TaskHandle_t tarea2; // Va en el core 1

void setup() { // se usa para definir pines de leds, el serial begin y se crea las tareas
  pinMode(pin_LED_1, OUTPUT);
  pinMode(pin_LED_2, OUTPUT);
  Serial.begin(115200);

  xTaskCreatePinnedToCore(
    Loop1,          // función que ejecuta la tarea
    "Tarea1",       // nombre interno
    1000,           // tamaño de stack (Es suficiente)
    NULL,           // parámetros (no tiene)
    1,              // prioridad (es la misma en ambas tareas ya que ambas tienen la misma importancia)
    &tarea1,        // handle/referencia (nombre definido arriba)
    0               // core donde correrá
  );                // la lógica es la misma en la otra tarea

  xTaskCreatePinnedToCore(
    Loop2,
    "Tarea2",
    1000,
    NULL,
    1,
    &tarea2,
    1
  );
}

void Loop1(void * pvParameters) {
  for(;;){
    digitalWrite(pin_LED_1, HIGH);
    delay (10000);
    digitalWrite(pin_LED_1, LOW);
    delay (10000);
  }
}

void Loop2(void * pvParameters) {
  for(;;){
    digitalWrite(pin_LED_2, HIGH);
    delay (500);
    digitalWrite(pin_LED_2, LOW);
    delay (500);
  }
}

/* los delays no frenan todo el código, ya que los delays solo funcionan dentro de su respectivo loop o tarea,
por lo que el delay de una tarea no afecta a la otra */

void loop() {} // va vacío, no ejecuta nada