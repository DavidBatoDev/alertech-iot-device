# 🚨 IoT Fire Safety and Intrusion Detection System

This project is an IoT-based Fire Safety and Intrusion Detection System that utilizes ESP32, Firebase, and real-time notifications to enhance safety and security.

## 📋 Features
- **Fire Detection:** Detects smoke levels using the MQ2 sensor.
- **Intrusion Detection:** Uses a touch sensor to detect unauthorized access.
- **Temperature and Humidity Monitoring:** Monitors environmental conditions with a DHT22 sensor.
- **Real-Time Notifications:** Sends alerts to subscribed devices via Firebase Cloud Messaging (FCM) when smoke or intrusion is detected.
- **Firestore Integration:** Updates sensor readings and system status in real-time on Firestore.

## ⚙️ Prerequisites
- **Hardware:**
  - ESP32 Development Board
  - MQ2 Gas Sensor
  - DHT22 Temperature and Humidity Sensor
  - Touch Sensor (or use ESP32's built-in touch pins)
- **Software:**
  - Arduino IDE
  - ESP32 Board Support Package for Arduino IDE
  - Libraries:
    - WiFi
    - HTTPClient
    - ArduinoJson
    - DHT Sensor Library
- **Firebase Setup:**
  - Firebase Project with Firestore Database
  - Firebase Authentication (Email/Password sign-in enabled)
  - API Key and Project ID from Firebase Console

## 🛠️ Installation Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/iot-fire-safety.git
   cd iot-fire-safety
    ```

2. Install required libraries in Arduino IDE:
   - Go to "Sketch" > "Include Library" > "Manage Libraries".
   - Search for and install:
     - WiFi
     - HTTPClient
     - ArduinoJson
     - DHT Sensor Library
3. Configure the `secrets.h` file with your credentials:
   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define API_KEY "YOUR_FIREBASE_API_KEY"
   #define USER_EMAIL "YOUR_FIREBASE_USER_EMAIL"
   #define USER_PASSWORD "YOUR_FIREBASE_USER_PASSWORD"
   #define FIREBASE_PROJECT_ID "YOUR_FIREBASE_PROJECT_ID"
    ```

4. Upload the code to your ESP32.

5. Monitor the Serial Monitor (115200 baud) for status and sensor readings.

## 🚦 How It Works

- The ESP32 reads data from the DHT22 and MQ2 sensors.
- The touch sensor acts as an intrusion detection mechanism.
- Sensor readings and system status are sent to Firestore.
- Notifications are triggered when:
  - Smoke levels exceed the defined threshold.
  - The touch sensor is activated.
- Real-time updates and alerts are accessible through the Firestore dashboard or any connected client.

## 📌 Future Improvements

- Add more sensors for enhanced detection.
- Implement a mobile app for remote monitoring.