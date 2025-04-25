//Codigo: Monitoreo de ruido(SPL) y co2(PPM) con ESP32, Thingspeak y Whatsapp
//Autor: Diego Alexander Fonseca
//Pais: Colombia

//........................................................

//MATERIALES 
//ESP32 -WROOM32 - DEVKITV1
//Pantalla : OLED 128x32 - SSD1306(I2C)
//Sensor sonido : INMP441 - MEMS(I2S)
//Sensor CO2: SCD41 (I2C)

//........................................................

#include <Wire.h> // Libreria i2c
#include <driver/i2s.h> // Libreria i2s
#include "ThingSpeak.h" // Libreria thingspeak
#include "WiFi.h" // Libreria wifi
#include <WiFiManager.h> //Libreria para gestion de wifi
#include <Adafruit_GFX.h>   //Libreria para gráficos
#include <Adafruit_SSD1306.h>  //Libreria para Oleds SSD1306
#include "SparkFun_SCD4x_Arduino_Library.h" // Libreria para SCD4X
#include "secrets.h" // claves secretas
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <math.h> // libreria para operaciones matematicas
#include "time.h" // libreria para obtener tiempo


//........................................................

//CONFIGURACION I2S PARA INMP441
//WS = GPIO25
//SD = GPIO32
//SCK = GPIO33
//L/R = GND
//VCC = VCC
//GND = GND
#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33
#define I2S_PORT I2S_NUM_0
#define BUFFER_LENGTH 64 // tamaño del buffer
#define MAX_DIGITAL_VALUE 32768 // 2^15 para señal de 16 bits
int16_t sampleBuffer[BUFFER_LENGTH]; // arreglo para almacenar datos

void setupI2S() {
  const i2s_config_t i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_LENGTH,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL);

  const i2s_pin_config_t pinConfig = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pinConfig);
}
//........................................................

//CONFIGURACION I2C PARA OLED Y SCD41 
//Pines I2C en ESP32
//SDA = GPIO21
//SCL = GPIO22

#define SCREEN_WIDTH 128 // ancho pantalla OLED , en pixeles
#define SCREEN_HEIGHT 32 // alto pantalla OLED , en pixeles
#define OLED_RESET -1 // NO usa pin dedicado para reiniciar pantalla, se comparte pin de reinicio del ESP32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // constructor de objeto
SCD4x sensor; //creamos la variable sensor

//........................................................

//CONFIGURACION THINKSPEAK Y TIME
WiFiClient cliente; // creamos cliente para poder enviar datos a Thingspeak
const char* ntpServer = "pool.ntp.org"; // servidor ntp par ala hora
const long  gmtOffset_sec = -18000; // COLOMBIA GMT-5 =(-5*36000) 
const int   daylightOffset_sec = 3600; // 3600s por hora
int lastDay = -1; // Para almacenar el ultimo día, -1 no define ningun dia del mes

//........................................................

//CONFIGURACION VARIABLES DE LECTURA
//const int led =2;// led interno de la placa esp32
float dBSPL = 0;
float dbcal = 0;
float co2 = 0;
float t = 0;
float h = 0;

float dosis = 0;  // Dosis de ruido acumulada (%)
int hmax = 8; // horas maximas de exposicion a db maximo - NIOSH(1998)
int dbmax = 85; // db maximo - NIOSH(1998)
int co2max = 1500; // nivel ppm maximo - Health and Safety Executive (2023)

int temperatura_local = 10; // offset °C - https://weather.com/
int altitud_local = 2516 ; // msnm
int presion_local = 101100; // presión en pascales(Pa) - 1mbar = 1HPa = 100 Pa
int ppm_local = 422; // co2 422 (ppm) del aire NASA - https://climate.nasa.gov/vital-signs/carbon-dioxide


unsigned long whatsPreviousMillis = 0; // contador para mensajes whatsapp
unsigned long thingPreviousMillis = 0; // contador para thingspeak
unsigned long sensor1PreviousMillis = 0; // contador para inmp441
unsigned long sensor2PreviousMillis = 0; // contador para scd41
unsigned long sensor1Interval = 1000; // 1 segundo para medir db
unsigned long sensor2Interval = 5000; // 5 segundos para medir co2, h, t


//........................................................

//FUNCIÓN PARA ENVIAR MENSAJES A WHATSAPP
void enviarWhatsapp(String message){

  // Data to send with HTTP POST
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);    
  HTTPClient http;
  http.begin(url);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200){
    Serial.println("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  // Free resources
  http.end();
}


//........................................................

//FUNCIÓN PROYECTAR_OLED: PROYECTAR VARIABLES: DEFINIR TAMAÑO, COLOR, POSICIÓN Y TEXTO EN OLED
void sensar_oled() { 
  display.clearDisplay(); //Borra el buffer
  display.setTextColor(WHITE); //Establece el color

  display.setTextSize(2); //Establece el tamaño de fuente, admite tamaños de 1 a 8
  display.setCursor(0,0); //Establecer las coordenadas y texto de dbspl
  display.print(dbcal,0);
  display.setTextSize(1);
  display.println("dBA");
  
  display.setTextSize(2);
  display.setCursor(0,16); //Establecer las coordenadas y texto de co2
  display.print(co2,0);
  display.setTextSize(1);
  display.print("ppm");
  
  display.setTextSize(2);
  display.setCursor(80,0); //Establecer las coordenadas y texto de temperatura
  display.print(t,0);
  display.setTextSize(1);
  display.print("C"); 
  
  display.setTextSize(2);
  display.setCursor(80,16); //Establecer las coordenadas y texto de humedad relativa
  display.print(h,0);
  display.setTextSize(1);
  display.print("%");

  // Indicar si está conectado a Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    display.setTextSize(1);
    display.setCursor(118, 0); // Coordenadas para el icono Wi-Fi
    display.print("=");
  }
  
  display.display(); //Muestra el texto 
}

//........................................................


//FUNCIÓN MENSAJE_OLED: DEFINIR TAMAÑO, COLOR, POSICIÓN Y MENSAJE EN OLED
void mensaje_oled( String mensaje) { 
  display.clearDisplay(); //Borra el buffer
  display.setTextColor(WHITE); //Establece el color
  display.setTextSize(1); //Establece el tamaño de fuente, admite tamaños de 1 a 8
  display.setCursor(0,16); //Establecer las coordenadas y texto de dbspl
  display.println(mensaje);
  display.display(); //Muestra el texto 
}

//........................................................


//FUNCIÓN PARA COMPENSAR TEMPERATURA, ALTITUD Y PRESION EN SENSOR SCD41
void signal_compensation_scd41(){

  Serial.println(F("SCD41 : Signal Compensation"));
  //sensor.enableDebugging(); // Uncomment this line to get helpful debug messages on Serial

  if (sensor.begin() == false)
  {
    Serial.println(F("Sensor not detected. Please check wiring. Freezing..."));
    while (1)
      ;
  }

  //We need to stop periodic measurements before we can change the sensor signal compensation settings
  if (sensor.stopPeriodicMeasurement() == true)
  {
    Serial.println(F("Periodic measurement is disabled!"));
  }  

  //Now we can change the sensor settings.
  //There are three signal compensation commands we can use: setTemperatureOffset; setSensorAltitude; and setAmbientPressure

  Serial.print(F("Temperature offset is currently: "));
  Serial.println(sensor.getTemperatureOffset(), 2); // Print the temperature offset with two decimal places
  sensor.setTemperatureOffset(temperatura_local); // Set the temperature offset to local C
  Serial.print(F("Temperature offset is now: "));
  Serial.println(sensor.getTemperatureOffset(), 2); // Print the temperature offset with two decimal places

  Serial.print(F("Sensor altitude is currently: "));
  Serial.println(sensor.getSensorAltitude()); // Print the sensor altitude
  sensor.setSensorAltitude(altitud_local); // Set the sensor altitude to local msnm
  Serial.print(F("Sensor altitude is now: "));
  Serial.println(sensor.getSensorAltitude()); // Print the sensor altitude

  //There is no getAmbientPressure command
  //bool success = sensor.setAmbientPressure(presion_local); // Set the ambient pressure to local Pascals
  //if (success)
  //{
  //  Serial.println(F("setAmbientPressure was successful"));
  //}

  //The signal compensation settings are stored in RAM by default and will reset if reInit is called
  //or if the power is cycled. To store the settings in EEPROM we can call:
  sensor.persistSettings(); // Uncomment this line to store the sensor settings in EEPROM

  //Just for giggles, while the periodic measurements are stopped, let's read the sensor serial number
  char serialNumber[13]; // The serial number is 48-bits. We need 12 bytes plus one extra to store it as ASCII Hex
  if (sensor.getSerialNumber(serialNumber) == true)
  {
    Serial.print(F("The sensor's serial number is: 0x"));
    Serial.println(serialNumber);
  }

  //Finally, we need to restart periodic measurements
  if (sensor.startPeriodicMeasurement() == true)
  {
    Serial.println(F("Periodic measurements restarted!"));
  }
}


//........................................................

//FUNCIÓN PARA FORZAR CALIBRACION DEL SENSOR SCD41 en ppm
void forzar_calibracion_scd41(){

  // Operar en modo de medición periódica por 3 minutos
  Serial.println("Iniciando medición periódica...");
  delay(180000);  // 3 minutos
  // Asegúrate de detener las mediciones periódicas antes de la calibración
  sensor.stopPeriodicMeasurement();
  delay(500);  // Esperar a que el sensor detenga las mediciones

  // Forzar la calibración a ppm_local
  float correction = sensor.performForcedRecalibration(ppm_local);
  if (correction == 0xFFFF) {
    Serial.println("Error: La calibración forzada ha fallado.");
  } else {
    Serial.print("Calibración forzada realizada. Corrección: ");
    Serial.println(correction);
  }

  // Iniciar medición periódica nuevamente
  sensor.startPeriodicMeasurement();

}


void reset_scd41(){
  if (sensor.begin() == false)
  {
    Serial.println(F("Sensor not detected. Please check wiring. Freezing..."));
    while (1)
      ;
  }

  //We need to stop periodic measurements before we can run the self test
  if (sensor.stopPeriodicMeasurement() == true)
  {
    Serial.println(F("Periodic measurement is disabled!"));
  }  

  //Now we can run the self test:
  Serial.println(F("Starting the self-test. This will take 10 seconds to complete..."));

  bool success = sensor.performSelfTest();

  Serial.print(F("The self test was "));
  if (success == false)
    Serial.print(F("not "));
  Serial.println(F("successful"));

  //We can do a factory reset if we want to completely reset the sensor
  Serial.println(F("Starting the factory reset. This will take 1200ms seconds to complete..."));

  success = sensor.performFactoryReset();

  Serial.print(F("The factory reset was "));
  if (success == false)
    Serial.print(F("not "));
  Serial.println(F("successful"));

  // Iniciar medición periódica nuevamente
  sensor.startPeriodicMeasurement();

}





//........................................................


void setup() {

  Serial.begin(115200); // iniciamos puerto serial
  Serial.println("Configurando I2C");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // iniciamos i2c para oled en la direccion 0x3C 
  sensor.begin(); // iniciamos scd41
  //reset_scd41(); // resetear a fabrica scd41 : comentar luego de ejecutar
  //signal_compensation_scd41(); // ajustar t,h : comentar luego de ejecutar 
  //forzar_calibracion_scd41(); // ajustar co2 : comentar luego de ejecutar 
  if(sensor.getAutomaticSelfCalibrationEnabled()==true){ // si autocalibracion activada
    Serial.println("SCD41 : SelfCalibration Mode"); // 
  }

  Serial.println("Connect to Wi-Fi");
  mensaje_oled("Connect to Wi-Fi");
  delay(50);
  //WiFi.begin(ssid,password); // iniciamos conexion wifi
  
  WiFi.mode(WIFI_STA); //modo STATION
  WiFiManager wm; // inicializamos libreria
  
  //wm.resetSettings() borra credenciales guardadas por libreria esp
  //dejar linea de codigo solo para pruebas
  //comentar la siguiente linea para produccion
  wm.resetSettings();  

  bool res;
  res = wm.autoConnect("AutoConnectAP","password"); // AP protegido con contraseña
  if(!res) {
    Serial.println("Connection error");
    mensaje_oled("Connection error");
    delay(50);
    //ESP.restart();
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); // si no hay conexion imprime .
  }

  Serial.println("Wi-Fi connected"); // si hay conexion imprime mensaje
  mensaje_oled("Wi-Fi connected");
  delay(50);


  ThingSpeak.begin(cliente); // iniciamos cliente thingspeak 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // Iniciar y obtener tiempo
 
  Serial.println("Configurando I2S"); // mensaje antes de iniciar i2s
  setupI2S(); // configuramos i2s
  i2s_start(I2S_PORT); // iniciamos i2s en el puerto seleccionado

  delay(1000); 
}



void loop() {
  unsigned long currentMillis = millis();
  
  // Obtiene el tiempo local y actualiza la información
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  int currentDay = timeinfo.tm_mday; // tm_mday contiene el día del mes

  if (currentDay != lastDay) {   // Verifica si ha cambiado el día
    dosis = 0; // Resetea la dosis a 0
    lastDay = currentDay; // Actualiza el día anterior
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.println("Dosis reseteada a 0.");
  }

  
  // Leer sensor1(INMP441) cada 1 segundo
  size_t bytesRead = 0;
  if (currentMillis - sensor1PreviousMillis >= sensor1Interval) {
    sensor1PreviousMillis = currentMillis;
    // Leer datos y obtener valor SPL

    //esp_err_t result = i2s_read()

    //Return
    //ESP_OK: Success
    //ESP_ERR_INVALID_ARG: Parameter error

    //Parameters
    //i2s_num: I2S_NUM_0, I2S_NUM_1
    //dest: Destination address to read into
    //size: Size of data in bytes
    //bytes_read: Number of bytes read, if timeout, bytes read will be less than the size passed in.
    //ticks_to_wait: RX buffer wait timeout in RTOS ticks. Pass portMAX_DELAY for no timeout.(1tick = 1ms@1khz)

    esp_err_t result = i2s_read(I2S_PORT, &sampleBuffer, BUFFER_LENGTH * sizeof(int16_t), &bytesRead, portMAX_DELAY);

    if (result == ESP_OK && bytesRead > 0) {
      int16_t samplesRead = bytesRead / sizeof(int16_t);
      if (samplesRead > 0) {
        float rms = 0;
        for (int16_t i = 0; i < samplesRead; ++i) {
          rms += sampleBuffer[i] * sampleBuffer[i];
        }
        rms = sqrt(rms / samplesRead); // Calcular valor RMS

        float dBFS = 20 * log10(rms / MAX_DIGITAL_VALUE);// Convertir RMS a dBFS
        dBSPL = dBFS + 120;// Convertir dBFS to dB SPL
        dbcal = (1.07*dBSPL)-7.19; // regresion lineal obtenida por calibracion
        
        Serial.print("dB: ");// Imprimir dB
        Serial.println(dbcal);

        float tiempoPermitido = hmax * pow(2, (dbmax - dbcal) / 3.0); // tiempo permitido en horas - NIOSH(1998)
        float dosisparcial = ( (1.0 / 3600) / tiempoPermitido ) * 100.0; // Dosis parcial para un tiempo de exposicion de 1 segundo
        dosis = dosis + dosisparcial; // Incrementar dosis total

        //Serial.print("Dosis Parcial(%): ");// Imprimir dosis
        //Serial.println(dosisparcial,5);


      }
    }
    sensar_oled(); //enviamos datos de sensores a pantalla OLED cada 1 segundo
  }
  

  // Leer sensor2 (scd41) cada 5 segundos
  if (currentMillis - sensor2PreviousMillis >= sensor2Interval) {
    sensor2PreviousMillis = currentMillis;
    // Leer datos y obtener valor de CO2, humedad relativa y temperatura
    if (sensor.readMeasurement()) // readMeasurement retornara verdadero cuando hay nuevos datos disponibles
    { 
      Serial.print("CO2(ppm):");
      co2=sensor.getCO2();
      Serial.println(co2);
      
      Serial.print("Temperatura(C):");
      t=sensor.getTemperature();
      Serial.println(t, 0);
      
      Serial.print("Humedad(%RH):");
      h=sensor.getHumidity();
      Serial.println(h, 0); 
      
    }
  }
  

  // Enviar datos a ThingSpeak cada 30 segundos
  if (currentMillis - thingPreviousMillis >= 30000) {
    thingPreviousMillis = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {
      // Ejecuta las tareas que requieren conexión a Internet
      // Enviar datos de sensor1 a internet
      // Enviar datos de sensor2 a internet
      ThingSpeak.setField (1,dbcal); // configuramos el campo 1 con el valor en dbcal
      ThingSpeak.setField (2,co2); // configuramos el campo 2 con el valor en co2
      ThingSpeak.setField (3,t); // configuramos el campo 3 con el valor en t
      ThingSpeak.setField (4,h); // configuramos el campo 4 con el valor en h
      ThingSpeak.setField (5,dosis); // configuramos el campo 5 con el valor en dosis
      ThingSpeak.writeFields(channelID,WriteAPIKey); // enviamos datos al canal en ThingSpeak
      Serial.println("Datos enviados a ThingSpeak"); 
    }


  }


  // Enviar datos a WhatsApp cada 300 segundos, si cumple condición
  if (currentMillis - whatsPreviousMillis >= 300000) {
    whatsPreviousMillis = currentMillis;

    Serial.print("Dosis(%): "); // Imprimir dosis diaria
    Serial.println(dosis);

    if( dosis>=100 && WiFi.status() == WL_CONNECTED){ // revisamos alerta cada 2 minutos, si dosis>=100%
      Serial.println("Enviando alerta de dosis de ruido..."); // Mensaje de depuración
      String messagedosis = "Alerta: La exposición a ruido ha superado el nivel recomendado ( 100% = 85dB-8H )"; 
      enviarWhatsapp(messagedosis);
      dosis = 0; // Resetea la dosis a 0
    }
          
    if( co2 >=co2max && WiFi.status() == WL_CONNECTED){ // revisamos alerta cada 2 minutos, si co2>=c02max
      Serial.println("Enviando alerta de CO2..."); // Mensaje de depuración
      String messageco2 = "Alerta: La exposición a CO2 ha superado el nivel recomendado ( 1500 ppm ). Su valor actual es " +  String(co2) + " ppm."; 
      enviarWhatsapp(messageco2);
    }

    
  }

}




