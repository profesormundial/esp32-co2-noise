# esp32-co2-noise
Sistema de bajo costo basado en ESP2 para el monitoreo de CO2 y ruido en un aula

Este proyecto presenta el diseño y prototipado de un sistema de bajo costo basado en el microcontrolador ESP32 para el monitoreo en tiempo real de los niveles de dióxido de carbono (CO2) y ruido en un aula escolar en Mosquera, Colombia. El sistema utiliza los sensores SCD41 y INMP441, comunicación mediante Wi-Fi, visualización local en una pantalla OLED y almacenamiento en la nube a través de la plataforma ThingSpeak. Además, incorpora un mecanismo de notificación automática mediante WhatsApp utilizando la API de CallMeBot.
Las pruebas de campo se realizaron en condiciones reales durante 7 jornadas escolares. Los resultados mostraron que los niveles de CO2 alcanzaron valores superiores a 1500 ppm en sesiones con alta ocupación y poca ventilación. También se registraron niveles de ruido superiores a los 50 dB SPL, alcanzando en algunos casos más de 100 dB SPL. La dosimetría sonora y la visualización en la nube permitieron interpretar el comportamiento ambiental del aula y facilitar la toma de decisiones por parte del docente investigador. El sistema cumplió su objetivo con precisión, ofreciendo una herramienta accesible, replicable, con un costo de fabricación de aproximadamente US$29.23, y enmarcada en la línea de investigación en automatización y herramientas lógicas. 

*Palabras clave: ESP32, CO2, Ruido, Aula, ThingSpeak, WhatsApp*

>[!IMPORTANT]
>- Calibration Data
>- https://docs.google.com/spreadsheets/d/1ewTRGTV6pyz7rSBkRCYbqBNqwdV0pVQ3TlwAcdi6qx4/edit?usp=sharing
>- ThingSpeak Data
>- https://docs.google.com/spreadsheets/d/1fV2sr9jItipNWUdPTNSpUzUsvMA_-qq-z9VmyYO-v4k/edit?usp=sharing 





**Diego Alexander Fonseca**
Todos los derechos reservados
