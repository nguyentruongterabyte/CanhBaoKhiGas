#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "secrets.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#define DHTPIN 5     // Định nghĩa chân kết nối cảm biến DHT11

#define DHTTYPE DHT11   // Loại cảm biến DHT11, bạn có thể thay thế bằng DHT22 hoặc DHT21

DHT dht(DHTPIN, DHTTYPE);
int LED = 32;            /*LED pin defined*/
int Sensor_input = A0; /*Digital pin 4 for sensor input*/
LiquidCrystal_I2C lcd(0x27,16,2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
byte celciusSymbol[] = {
  B11000,
  B11000,
  B00111,
  B01000,
  B01000,
  B01000,
  B00111,
  B00000
};

void setup() {
  Serial.begin(115200);  /*baud rate for serial communication*/
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.print(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key
  config.api_key = API_KEY;

  // Assign the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the callback function for the long-running token generation task
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  // Begin Firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // Reconnect to Wi-Fi if necessary
  Firebase.reconnectWiFi(true);
  
  pinMode(LED, OUTPUT);
  lcd.init();                      // Khởi tạo màn hình LCD
  lcd.backlight();                // Bật đèn nền màn hình LCD
  lcd.createChar(0, celciusSymbol);
  dht.begin();    
  pinMode(Sensor_input, INPUT); 
}

void loop() {
  int humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int sensor_Aout = analogRead(Sensor_input);  /*Analog value read function*/
  String label = "No Gas"; 
  if (sensor_Aout > 3000) label = "Very dangerous";
  else if(sensor_Aout >1300) label = "Dangerous";
  else if(sensor_Aout > 800) label = "slightly dangerous";
  else label = "No Dangerrous";
  
  lcd.clear(); 
  // Hiển thị giá trị nhiệt độ và độ ẩm lên màn hình LCD
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(temperature);
  lcd.write(0);

  lcd.setCursor(10, 0);
  lcd.print("H: ");
  lcd.print(humidity);
  lcd.print(" %");

  lcd.setCursor(0,1);
  lcd.print("Gas: ");
  lcd.print(sensor_Aout);

  lcd.setCursor(10,1);
  lcd.print(label);

  Serial.print("Nhiệt độ: ");
  Serial.print(temperature);
  
  Serial.print("°C - Độ ẩm: ");
  Serial.print(humidity);
  Serial.println("%");
  
  Serial.print("Gas Sensor: ");  
  Serial.print(sensor_Aout);   /*Read value printed*/
  Serial.print("\t");
  Serial.print("\t");
  if (sensor_Aout > 1800) {    /*if condition with threshold 1800*/
    Serial.println(label);  
    digitalWrite (LED, HIGH) ;/*LED set HIGH if Gas detected */
  }
  else {
    Serial.println(label);
    digitalWrite (LED, LOW) ;  /*LED set LOW if NO Gas detected */
    
  }
  
  String documentPath = "EspData/MQ2/data";


  // Create a FirebaseJson object for storing data
  FirebaseJson content;

  // Check if the values are valid (not NaN)
  if (!isnan(sensor_Aout) ) {
    content.set("fields/Tem/stringValue", String(temperature));
    content.set("fields/Hum/stringValue", String(humidity));
    content.set("fields/Gas/stringValue", String(sensor_Aout));
    content.set("fields/Label/stringValue", label); 
    Serial.print("Update/Add MQ2 Data... ");

    // Use the patchDocument method to update the Temperature and Humidity Firestore document
    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw())  ) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      } 
      else 
      {
         Serial.println(fbdo.errorReason());
      }
    }
    else
    {
       Serial.println("Failed to read MQ2 data.");
    }
   // Delay before the next reading
  delay(1000);
                
}
