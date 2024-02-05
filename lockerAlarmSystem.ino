#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

/*const char* ssid = "FTTX-e69600";
const char* password = "00037249";*/

const char* ssid = "vivo1901";
const char* password = "awit1234";

String chatId = "5413661467";
String BOTtoken = "6264862240:AAHbPH3EvyX0HI8JFlf1VmU-vD-qgnZEPdU";

#define API_KEY "AIzaSyBc3c3D0YdwAwjPtp7fZo9mL2h1qUHSPjc"
#define DATABASE_URL "https://atlas-iot-51e63-default-rtdb.asia-southeast1.firebasedatabase.app/"

WiFiServer server(80);

String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool sendPhoto = false;

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String formattedDate;
String dayStamp;
String timeStamp;

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const byte buzzerPin = 12;
bool buzzerState = false;

bool cameraState = true;
bool stats;

const byte motionSensor = 13;
bool motionDetected = false;
bool motionDetectEnable = false;

const byte resetPin = 15;

int botRequestDelay = 1000;  
long lastTimeBotRan;    

String sendPhotoTelegram();

static void IRAM_ATTR detectsMovement(void * arg) {
  Serial.println("MOTION DETECTED!");
  motionDetected = true;
  buzzerState = true;
}

void IRAM_ATTR resetHandler() {
  ESP.restart();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, buzzerState);

  pinMode(resetPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(resetPin), resetHandler, FALLING);

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  server.begin();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign up ok!");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(28800);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  err = gpio_install_isr_service(0);
  err = gpio_isr_handler_add(GPIO_NUM_13, &detectsMovement, (void *) 13);
  if (err != ESP_OK) {
    Serial.printf("handler add failed with error 0x%x \r\n", err);
  }
  err = gpio_set_intr_type(GPIO_NUM_13, GPIO_INTR_POSEDGE);
  if (err != ESP_OK) {
    Serial.printf("set intr type failed with error 0x%x \r\n", err);
  }

  delay(5000);
  Serial.printf("PIR Sensor Initilized!! \n");
}

void loop() {
  WiFiClient client = server.available();   

  if (client) {                            
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");         
    String currentLine = "";               
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  
      currentTime = millis();
      if (client.available()) {             
        char c = client.read();            
        Serial.write(c);                    
        header += c;
        if (c == '\n') {                    
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            if (header.indexOf("GET /Alarm/on") >= 0) {
              Serial.println("Alarm Enabled");
              motionDetectEnable = true;
              motionDetected = false;
            } else if (header.indexOf("GET /Alarm/off") >= 0) {
              Serial.println("Alarm Disabled");
              motionDetectEnable = false;
              motionDetected = false;
            } else if (header.indexOf("GET /Camera") >= 0) {
              Serial.println("Capturing Photo..");
              sendPhoto = true;
            } else if (header.indexOf("GET /Status") >= 0) {
              Serial.println("Showing Status..");
              stats = true;
            } else if (header.indexOf("GET /Reset") >= 0) {
              Serial.println("System Resetting..");
              resetHandler();
            } 
            
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; border-radius: 50px; width: 300px; }");
            client.println(".button2 { background-color: #555555; }</style></head>");
            client.println("<body><h1>Anti-Theft Locker Alarm System</h1>");
            client.println("<p>Use the following buttons to interact with the system</p>");
            if (motionDetectEnable == false) {
              client.println("<p><a href=\"/Alarm/on\"><button class=\"button\">Enable Alarm</button></a></p>");
            } else {
              client.println("<p><a href=\"/Alarm/off\"><button class=\"button button2\">Disable Alarm</button></a></p>");
            }
            client.println("<p><a href=\"/Camera\"><button class=\"button\">Capture Photo</button></a></p>");
            client.println("<p><a href=\"/Status\"><button class=\"button\">Show Status</button></a></p>");
            client.println("<p><a href=\"/Reset\"><button class=\"button\">Reset System</button></a></p>");
            client.println("</body></html>");
            client.println();
            break;
            
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;      
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  delay(1000);
  
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    Firebase.RTDB.setBool(&fbdo, "/Alarm/Status", motionDetectEnable);

    if (motionDetected && motionDetectEnable) {
      bot.sendMessage(chatId, "Alert! Motion Detected!", "");
      Serial.println("Motion Detected!");
      sendPhotoTelegram();

      Firebase.RTDB.setBool(&fbdo, "/Camera/Status", cameraState);
      delay(1000);

      cameraState = false;
      Firebase.RTDB.setBool(&fbdo, "/Camera/Status", cameraState);
      delay(1000);

      Firebase.RTDB.setBool(&fbdo, "/PIR Sensor/Status", motionDetected);
      delay(1000);

      motionDetected = false;
      Firebase.RTDB.setBool(&fbdo, "/PIR Sensor/Status", motionDetected);
      delay(1000);

      Firebase.RTDB.setBool(&fbdo, "/Piezo Buzzer/Status", buzzerState);
      if (buzzerState) {
        tone(buzzerPin, 1047, 10000);
      }
      delay(1000);

      buzzerState = false;
      Firebase.RTDB.setBool(&fbdo, "/Piezo Buzzer/Status", buzzerState);
      if (!buzzerState) {
        noTone(buzzerPin);
      }
      delay(1000);
      
      Firebase.RTDB.setString(&fbdo, "/History/Day", dayStamp);
      delay(1000);
      
      Firebase.RTDB.setString(&fbdo, "/History/Time", timeStamp);
      delay(1000);
    }

    if (sendPhoto) {
      Serial.println("Preparing photo");
      sendPhotoTelegram();

      Firebase.RTDB.setBool(&fbdo, "/Camera/Status", sendPhoto);
      delay(1000);

      sendPhoto = false;
      Firebase.RTDB.setBool(&fbdo, "/Camera/Status", sendPhoto);
      delay(1000);
    }

    if (stats) {
      String reply = "Security Information\n";
      Firebase.RTDB.getBool(&fbdo, "/Alarm/Status");
      reply += "Alarm: " + String(fbdo.boolData() ? "On" : "Off") + "\n";
      Firebase.RTDB.getString(&fbdo, "/History/Day");
      reply += "History: " + String(fbdo.stringData()) + " at ";
      Firebase.RTDB.getString(&fbdo, "/History/Time");
      reply += String(fbdo.stringData()) + "\n\n";
      
      reply += "Components Status\n";
      Firebase.RTDB.getBool(&fbdo, "/Camera/Status");
      reply += "Camera: " + String(fbdo.boolData() ? "On" : "Off") + "\n";
      Firebase.RTDB.getBool(&fbdo, "/PIR Sensor/Status");
      reply += "PIR Sensor: " + String(fbdo.boolData() ? "On" : "Off") + "\n";
      Firebase.RTDB.getBool(&fbdo, "/Piezo Buzzer/Status");
      reply += "Piezo Buzzer: " + String(fbdo.boolData() ? "On" : "Off") + "\n";

      bot.sendMessage(chatId, reply, "");
      stats = false;
    }
    
  }
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }

  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");

    String head = "--make2explore\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--make2explore\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--make2explore--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;

    clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=make2explore");
    clientTCP.println();
    clientTCP.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        clientTCP.write(fbBuf, remainder);
      }
    }

    clientTCP.print(tail);

    esp_camera_fb_return(fb);

    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state == true) getBody += String(c);
        if (c == '\n') {
          if (getAll.length() == 0) state = true;
          getAll = "";
        }
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }

  return getBody;
}
