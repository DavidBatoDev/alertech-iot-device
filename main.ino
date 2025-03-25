#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

/********** 1) Wi-Fi Credentials **********/
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

/********** 2) Firebase Credentials **********/
#define API_KEY         "YOUR_FIREBASE_API_KEY"
#define USER_EMAIL      "YOUR_USER_EMAIL"
#define USER_PASSWORD   "YOUR_USER_PASSWORD"
#define FIREBASE_PROJECT_ID "YOUR_FIREBASE_PROJECT_ID"

/********** 3) Firestore Paths **********/
#define FIRESTORE_COLLECTION "YOUR_COLLECTION"               // e.g. "user"
#define FIRESTORE_DOCUMENT   "YOUR_DOCUMENT"                 // e.g. "project-demo"

/********** 4) Sensor Setup (Example: DHT22) **********/
#define DHTPIN   14
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

/********** 5) Other Sensor Pins **********/
#define MQ2_PIN            34
#define TOUCH_PIN          4
#define TOUCH_THRESHOLD    40
#define SMOKE_THRESHOLD    1500

// Additional warning thresholds
#define MQ2_WARNING_THRESHOLD   800   // Example value
#define TEMP_WARNING_THRESHOLD  35.0  // Example value in °C

/********** 6) Global Variables **********/
String g_idToken;       // Stores the current ID token
String g_refreshToken;  // Stores the refresh token from sign-in response
bool   tokenReceived = false;
unsigned long lastNotificationTime = 0;  // for simple debounce

// Base URL for notifications API
const char* BASE_API_URL = "https://your-notification-api.example.com/";

// Device topic for notifications
const char* DEVICE_TOPIC = "YOUR_DEVICE_TOPIC";

/********** Function Prototypes **********/
bool signInToFirebase(String email, String password, String apiKey);
bool refreshFirebaseToken(String apiKey);
bool patchFirestoreData(float temperature, float humidity, int mq2Value, String status);
void sendTopicNotification();
void sendDeviceNotification();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Initialize DHT
  dht.begin();

  // Sign in to Firebase using email/password
  if (signInToFirebase(USER_EMAIL, USER_PASSWORD, API_KEY)) {
    Serial.println("Sign-in successful!");
    tokenReceived = true;
  } else {
    Serial.println("Sign-in failed!");
  }
}

void loop() {
  // Refresh token if necessary (this example simply tries to refresh if not tokenReceived)
  if (!tokenReceived) {
    if (refreshFirebaseToken(API_KEY)) {
      Serial.println("Token refreshed successfully!");
      tokenReceived = true;
    }
  }

  // Read sensors if we have a valid token
  if (tokenReceived) {
    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();
    int mq2Value      = analogRead(MQ2_PIN);
    int touchValue    = touchRead(TOUCH_PIN);

    // Basic sensor check
    if (!isnan(temperature) && !isnan(humidity)) {
      // Determine status based on both MQ2 and touch sensor
      String status;
      
      // 1) CRITICAL if touch sensor is triggered OR MQ2 exceeds SMOKE_THRESHOLD
      if (touchValue < TOUCH_THRESHOLD || mq2Value > SMOKE_THRESHOLD) {
        status = "critical";
      }
      // 2) WARNING if MQ2 or temperature crosses certain warning thresholds
      else if (mq2Value > MQ2_WARNING_THRESHOLD || temperature > TEMP_WARNING_THRESHOLD) {
        status = "warning";
      }
      // 3) Otherwise, SAFE
      else {
        status = "safe";
      }

      // Update Firestore document with sensor values + new status
      if (patchFirestoreData(temperature, humidity, mq2Value, status)) {
        Serial.println("Firestore updated successfully!");
      } else {
        Serial.println("Failed to update Firestore!");
      }
    }

    // Check for alarm triggers (smoke detection or touch sensor activation)
    // Use a simple debounce (here 10 seconds) to avoid flooding notifications.
    unsigned long currentMillis = millis();
    if (currentMillis - lastNotificationTime > 10000) { // 10-second interval
      if (mq2Value > SMOKE_THRESHOLD || touchValue < TOUCH_THRESHOLD) {
        sendTopicNotification();
        sendDeviceNotification();
        lastNotificationTime = currentMillis;
      }
    }
    delay(3000);
  }
}

/**
 * signInToFirebase:
 *   Uses the Firebase Auth REST API (signInWithPassword)
 *   to exchange email/password for an ID token and refresh token.
 */
bool signInToFirebase(String email, String password, String apiKey) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    return false;
  }
  HTTPClient http;
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + apiKey;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["email"] = email;
  doc["password"] = password;
  doc["returnSecureToken"] = true;
  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  if (httpResponseCode > 0) {
    String response = http.getString();
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error) {
      if (responseDoc.containsKey("idToken") && responseDoc.containsKey("refreshToken")) {
        g_idToken = responseDoc["idToken"].as<String>();
        g_refreshToken = responseDoc["refreshToken"].as<String>();
        Serial.println("Received ID Token: " + g_idToken.substring(0, 10) + "...");
        http.end();
        return true;
      } else {
        Serial.println("Error: No idToken/refreshToken in response!");
      }
    } else {
      Serial.println("Error parsing sign-in response JSON");
    }
  } else {
    Serial.print("Sign-in request failed. HTTP code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return false;
}

/**
 * refreshFirebaseToken:
 *   Uses the Firebase Auth REST API to exchange a refresh token for a new ID token.
 */
bool refreshFirebaseToken(String apiKey) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    return false;
  }
  HTTPClient http;
  String url = "https://securetoken.googleapis.com/v1/token?key=" + apiKey;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "grant_type=refresh_token&refresh_token=" + g_refreshToken;
  int httpResponseCode = http.POST(body);
  if (httpResponseCode > 0) {
    String response = http.getString();
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (!error) {
      if (responseDoc.containsKey("id_token") && responseDoc.containsKey("refresh_token")) {
        g_idToken = responseDoc["id_token"].as<String>();
        g_refreshToken = responseDoc["refresh_token"].as<String>();
        Serial.println("New ID Token: " + g_idToken.substring(0, 10) + "...");
        http.end();
        return true;
      } else {
        Serial.println("Error: No id_token/refresh_token in refresh response!");
      }
    } else {
      Serial.println("Error parsing refresh response JSON");
    }
  } else {
    Serial.print("Refresh token request failed. HTTP code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return false;
}

/**
 * patchFirestoreData:
 *   Sends a PATCH request to Firestore’s REST API to update fields in a document.
 */
bool patchFirestoreData(float temperature, float humidity, int mq2Value, String status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    return false;
  }
  String url = "https://firestore.googleapis.com/v1/projects/";
  url += FIREBASE_PROJECT_ID;
  url += "/databases/(default)/documents/";
  url += FIRESTORE_COLLECTION;
  url += "/";
  url += FIRESTORE_DOCUMENT;
  url += "?updateMask.fieldPaths=temperature";
  url += "&updateMask.fieldPaths=humidity";
  url += "&updateMask.fieldPaths=mq2";
  url += "&updateMask.fieldPaths=status";

  HTTPClient http;
  http.begin(url);
  String bearer = "Bearer " + g_idToken;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", bearer);

  StaticJsonDocument<256> doc;
  JsonObject fields = doc.createNestedObject("fields");

  JsonObject tempObj = fields.createNestedObject("temperature");
  tempObj["doubleValue"] = temperature;
  
  JsonObject humObj = fields.createNestedObject("humidity");
  humObj["doubleValue"] = humidity;
  
  JsonObject mq2Obj = fields.createNestedObject("mq2");
  mq2Obj["integerValue"] = mq2Value;
  
  JsonObject statusObj = fields.createNestedObject("status");
  statusObj["stringValue"] = status;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.sendRequest("PATCH", requestBody);
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Firestore response: ");
    Serial.println(response);
    http.end();
    return (httpResponseCode == 200);
  } else {
    Serial.print("PATCH request failed. HTTP code: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}

/**
 * sendTopicNotification:
 *   Sends a notification to all subscribers of the topic.
 */
void sendTopicNotification() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    return;
  }
  HTTPClient http;
  String url = String(BASE_API_URL) + "sendTopicNotification";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  StaticJsonDocument<256> doc;
  doc["title"] = "Fire Detected!";
  doc["body"] = "Fire Detected in Room 101";
  doc["sound_type"] = "alarm_sound";

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  if (httpResponseCode > 0) {
    Serial.println("Topic notification sent successfully.");
  } else {
    Serial.print("Error sending topic notification. HTTP code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

/**
 * sendDeviceNotification:
 *   Sends a notification to a specific device/topic.
 */
void sendDeviceNotification() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection!");
    return;
  }
  HTTPClient http;
  String url = String(BASE_API_URL) + "sendDeviceNotification";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload. The device topic here is set to the Firestore document name.
  StaticJsonDocument<256> doc;
  doc["topic"] = DEVICE_TOPIC;
  doc["title"] = "Fire Detected!";
  doc["body"] = "Fire Detected in Room 101";
  doc["sound_type"] = "alarm_sound";

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  if (httpResponseCode > 0) {
    Serial.println("Device notification sent successfully.");
  } else {
    Serial.print("Error sending device notification. HTTP code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}
