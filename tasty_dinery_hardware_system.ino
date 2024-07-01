//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 01 ESP32 Cam QR Code Scanner
/*
 * Reference :
 * - ESP32-CAM QR Code Reader (off-line) : https://www.youtube.com/watch?v=ULZL37YqJc8
 * - ESP32QRCodeReader_Page : https://github.com/fustyles/Arduino/tree/master/ESP32-CAM_QRCode_Recognition/ESP32QRCodeReader_Page
 * 
 * The source of the "quirc" library I shared on this project: https://github.com/fustyles/Arduino/tree/master/ESP32-CAM_QRCode_Recognition/ESP32QRCodeReader_Page
 */

// _____________________________________________________________________ include libraries

#include <WiFi.h>
#include <LCD_I2C.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "quirc.h"
#include "time.h"
#include "camera_init.h"

// _____________________________________________________________________ firebase credentials

// firebase API and project id
#define API_KEY "AIzaSyB3gDM-H-mKIpMBELE7Tpw82-lXH98eUSY"
#define FIREBASE_PROJECT_ID "tasty-dinery"

// firebase dmin auth credentials
#define USER_EMAIL "mwesiga1601@gmail.com"
#define USER_PASSWORD "1111111111"

// _____________________________________________________________________ wifi credentials

const String ssid = "ment";
const String password = "ment11111";

// _____________________________________________________________________ lcd display

LCD_I2C lcd(0x27, 20, 4);

// _____________________________________________________________________ button pin instantiation

// led
#define onBoardLed 4
#define greenLedPin 2
#define whiteLedPin 16

// buttons
#define cancelButton 12
#define verifyOrderButton 13

// _____________________________________________________________________ firebase data object and ntp

// define firebase data object, firebase authentication and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// time var and ntp offset
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 3600;

// _____________________________________________________________________ statuses

bool dataAvailableStatus = false;

// _____________________________________________________________________ Variables declaration

struct QRCodeData {
  bool valid;
  int dataType;
  uint8_t payload[1024];
  int payloadLen;
};

struct quirc *q = NULL;
uint8_t *image = NULL;
camera_fb_t *fb = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;
struct QRCodeData qrCodeData;
String QRCodeResult = "";

// ________________________________________________________________________________

// ________________________________________________________________________________

// ________________________________________________________________________________ SETUP

void setup() {

  //-------------------------------------------------------------------- disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  //-------------------------------------------------------------------- Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //-------------------------------------------------------------------- initialize serial communication speed
  Serial.begin(38400);
  Serial.println();
  delay(2000);

  Serial.println("----------tasty dinery----------");
  Serial.println();
  delay(2000);

  //-------------------------------------------------------------------- initialize lcd display
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("----tasty dinery----");

  //-------------------------------------------------------------------- Camera configuration.
  Serial.println("Start configuring and initializing the camera...");
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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println();
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  Serial.println("Configuration and Initialization of Camera Completed Successfully!!!");

  //-------------------------------------------------------------------- initialize WiFi
  WiFiInit();

  //-------------------------------------------------------------------- initialize firebase
  Serial.println("Initializing firebase");

  firebaseInit();

  delay(2000);
  Serial.println("Firebase Initialization Completed Successfully!!!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("----tasty dinery----");
  lcd.setCursor(0, 1);
  lcd.print("system configuration");
  lcd.setCursor(5, 2);
  lcd.print("completed!");

  delay(2000);

  //-------------------------------------------------------------------- initialize gpio pins
  pinMode(onBoardLed, OUTPUT);
  pinMode(cancelButton, INPUT);
  pinMode(verifyOrderButton, INPUT);
}

// ________________________________________________________________________________

// ________________________________________________________________________________

// ________________________________________________________________________________

void loop() {

  // To instruct the camera to take or capture a QR Code image, then it is processed and translated into text.
  Serial.println("QRCodeReader is ready.");
  Serial.print("QRCodeReader running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("qrcodereader running");

  // an ongoing loop
  while (1) {
    q = quirc_new();
    if (q == NULL) {
      Serial.print("can't create quirc object\r\n");
      continue;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("qrcodereader running");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");

      lcd.setCursor(0, 1);
      lcd.print("qrcode_reader failed");
      delay(500);
      continue;
    }

    quirc_resize(q, fb->width, fb->height);
    image = quirc_begin(q, NULL, NULL);
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);

    int count = quirc_count(q);
    if (count > 0) {
      quirc_extract(q, 0, &code);
      err = quirc_decode(&code, &data);

      if (err) {
        Serial.println("Decoding FAILED");
        QRCodeResult = "Decoding FAILED";

        lcd.setCursor(2, 1);
        lcd.print("decoding failed!");

      } else {
        Serial.printf("Decoding successful:\n");

        lcd.setCursor(0, 1);
        lcd.print("decoding completed!!");

        // -------------------------------------------------------------------------- start firebase implementation
        dumpData(&data);

        lcd.clear();
        lcd.setCursor(2, 1);
        lcd.print("fetching order!!");
        delay(3000);
        Serial.println();
        Serial.print("Payload Length: ");
        Serial.println(QRCodeResult.length());
        String userId = QRCodeResult.substring(0, 28);    // 28 char in length
        String orderId = QRCodeResult.substring(28, 48);  // 20 char in length

        Serial.println();
        Serial.print("userId: ");
        Serial.println(userId);
        Serial.print("orderId: ");
        Serial.println(orderId);
        delay(1000);

        // String documentPath = "Users/userID/Orders/orderID";
        String documentPath = "/Users/" + userId + "/Orders/" + orderId + "/";
        Serial.println();
        Serial.print("document path: ");
        Serial.println(documentPath);
        delay(2000);

        // get a document
        Serial.println();
        Serial.println("Get a document... ");
        if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "")) {
          Serial.println();
          Serial.println("Document Fetching Completed Successfully!!!");
          delay(2000);
          Serial.printf("%s", fbdo.payload().c_str());

          // Working Alternative: Create a FirebaseJson object and set content with received payload

          Serial.println();
          Serial.println("Creating FirebaseJson Document to receive payload");

          // alternative one commented in camera_init.h file

          DynamicJsonDocument doc(2048);
          Serial.println();
          Serial.println("Initiating Deserialization");

          DeserializationError error = deserializeJson(doc, fbdo.payload().c_str());
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return;
          }

          // extract the required fields
          const char *title = doc["fields"]["items"]["arrayValue"]["values"][0]["mapValue"]["fields"]["title"]["stringValue"];
          const char *quantity = doc["fields"]["items"]["arrayValue"]["values"][0]["mapValue"]["fields"]["quantity"]["integerValue"];
          const char *status = doc["fields"]["status"]["stringValue"];
          const char *orderID = doc["fields"]["orderID"]["stringValue"];
          double totalAmount = doc["fields"]["totalAmount"]["doubleValue"];

          String len = String(status);
          String stats = len.substring(12, 20);          

          Serial.println();
          Serial.println("Deserialization Completed Successfully");
          Serial.print("Title: ");
          Serial.println(title);
          Serial.print("Quantity: ");
          Serial.println(quantity);
          Serial.print("Status: ");
          Serial.println(stats);
          Serial.print("Order ID: ");
          Serial.println(orderID);
          Serial.print("Total Amount: ");
          Serial.println(totalAmount);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(orderID);
          lcd.setCursor(0, 1);
          lcd.print(title);
          lcd.setCursor(0, 2);
          lcd.print("quantity: ");
          lcd.print(quantity);
          lcd.setCursor(0, 3);
          lcd.print("status: ");
          lcd.print(stats);

          delay(3000);

          dataAvailableStatus = true;

          // ------------------------------------------------------------------------ firebase update order or cancel order serving
          if (stats != "served") {

            while (dataAvailableStatus) { 

              if (digitalRead(cancelButton) == HIGH) {

                dataAvailableStatus = false;

                lcd.clear();
                lcd.setCursor(0, 1);
                lcd.print("exiting order review");
                lcd.setCursor(3, 2);
                lcd.print("please wait!!!");

                delay(3000);

              }

              else if (digitalRead(verifyOrderButton) == HIGH) {

                // digitalWrite(onBoardLed, HIGH);

                // create firebase json object for storing data
                FirebaseJson content;

                struct tm timeinfo;
                if (!getLocalTime(&timeinfo)) {
                  Serial.println("Failed to obtain time");
                  return;
                }
                char servedDate[100];
                strftime(servedDate, sizeof(servedDate), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);

                //  set the values for order status and served time on database
                content.set("fields/status/stringValue", "OrderStatus.served");
                content.set("fields/servedDate/timestampValue", String(servedDate));

                Serial.println();
                Serial.println("Update data using Esp32...");

                // use patch document method to update the data
                if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "status") && Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "servedDate")) {
                  Serial.printf("\data uploaded successfully\n%s\n\n", fbdo.payload().c_str());
                } else {
                  Serial.println(fbdo.errorReason());
                }

                Serial.println();
                Serial.println("order has been served");

                lcd.clear();
                lcd.setCursor(0, 1);
                lcd.print("order already marked");
                lcd.setCursor(5, 2);
                lcd.print("as served!");

                delay(2000);

                dataAvailableStatus = false;

                // digitalWrite(onBoardLed, LOW);

                // add a small delay
              }

              //  keep on waiting and keep on displaying information
            }
          } else {

            lcd.clear();
            lcd.print("the order was served");

            Serial.println();
            Serial.println("order has been served!!!");

            Serial.println("exiting!!!");

            delay(10000);
          }
        } else {
          Serial.println(fbdo.errorReason());

          lcd.clear();
          lcd.print(fbdo.errorReason());
        }
      }
      Serial.println();
    }

    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;
    quirc_destroy(q);
  }
}

// ________________________________________________________________________________

// ________________________________________________________________________________

/* ________________________________________________________________________________ Function to display the results of reading the QR Code on the serial monitor. */
void dumpData(const struct quirc_data *data) {
  Serial.printf("Version: %d\n", data->version);
  Serial.printf("ECC level: %c\n", "MLHQ"[data->ecc_level]);
  Serial.printf("Mask: %d\n", data->mask);
  Serial.printf("Length: %d\n", data->payload_len);
  Serial.printf("Payload: %s\n", data->payload);

  QRCodeResult = (const char *)data->payload;
}

// ________________________________________________________________________________ function for initializing wifi in esp32

void WiFiInit() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("\nConnecting");

  lcd.setCursor(2, 1);
  lcd.print("connecting wi-fi");
  lcd.setCursor(0, 2);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);

    lcd.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!!!");
  Serial.println("");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("----tasty dinery----");
  lcd.setCursor(3, 1);
  lcd.print("wifi connected");

  delay(2000);
}

// ________________________________________________________________________________ function for initializing firebase

void firebaseInit() {

  // print firebase client version
  Serial.printf("Firebase Client version: v%s\n\n", FIREBASE_CLIENT_VERSION);

  // assign API key
  config.api_key = API_KEY;

  // assign user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  //  assign the callback function for the long0running token generation task
  // config.token_status_callback = tokenStatusCallBack; // see addons/TokenHelper.h

  // begin firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // reconnect to Wi-Fi if necessary
  // Firebase.reconnectWiFi(true);
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<