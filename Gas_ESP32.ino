#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "Model_data_gas.h" // Tệp header chứa mô hình TFLite

#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

#define DHTPIN 5   
#define DHTTYPE DHT11   
#define BLYNK_TEMPLATE_ID "TMPL6fHefmlSI"
#define BLYNK_TEMPLATE_NAME "Gas Warning"
#define BLYNK_AUTH_TOKEN "oIIBRmMKT8s7EQlsSIckVAnyGt0k0SPs"
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <BlynkSimpleEsp32.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Yamaha";
char pass[] = "123456789";
DHT dht(DHTPIN, DHTTYPE);
int LED = 32;            /*LED pin defined*/
int GasSensorInput = A0;
const char* output_classes[] = {"Dangerous", "No Dangerous", "Slightly dangerous", "Very dangerous"};

LiquidCrystal_I2C lcd(0x27, 16, 2);
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

const int kInputSize = 3; // Số lượng đầu vào của mô hình: gas, nhiệt độ, độ ẩm
const int kOutputSize = 4; // Số lượng lớp đầu ra của mô hình
constexpr int kTensorArenaSize = 32 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
float input_buffer[kInputSize];
float output_buffer[kOutputSize];

tflite::MicroInterpreter* interpreter;
bool isConnected = false;

tflite::MicroInterpreter* setupModel() {
    static tflite::MicroErrorReporter micro_error_reporter;
    static tflite::ErrorReporter* error_reporter = &micro_error_reporter;

    static const tflite::Model* model = tflite::GetModel(model_data_gas);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("Model provided is schema version %d not equal to supported version %d.\n", model->version(), TFLITE_SCHEMA_VERSION);
        return nullptr;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter.AllocateTensors();

    return &interpreter;
}

// Hàm đọc dữ liệu từ cảm biến
void readSensorData(float* input_data) {
    // Đọc dữ liệu từ các cảm biến gas, nhiệt độ, độ ẩm
    input_data[0] = dht.readHumidity();
    input_data[1] = analogRead(GasSensorInput); 
    input_data[2] = dht.readTemperature(); 
}

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    delay(1000);
    lcd.init();     // Khởi tạo màn hình LCD
    lcd.createChar(0, celciusSymbol);
    lcd.backlight();                 // Bật đèn nền màn hình LCD
    dht.begin();
    pinMode(GasSensorInput, INPUT); 
    Blynk.begin(auth, ssid, pass);

    interpreter = setupModel();
    if (interpreter == nullptr) {
        Serial.println("Failed to setup the model");

        while (true);
    }

    Serial.println("Model is ready");
}

void loop() {
    // Blynk
    int humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int sensor_Aout = analogRead(GasSensorInput);  /*Analog value read function*/
    String label = "No Gas"; 
    Blynk.run();
    if (Blynk.connected() && !isConnected) {
        isConnected = true;
        Blynk.virtualWrite(V0, HIGH); // Bật LED ảo trên Blynk
    } else if (!Blynk.connected() && isConnected) {
        isConnected = false;
        Blynk.virtualWrite(V0, LOW);
    }
    if (sensor_Aout > 1800) {
        label = "Gas";
    }
    String sensorValue ="Nhiệt độ: "+  String(temperature) +"°C - Độ ẩm: " +String(humidity) + "%"; // Đọc giá trị từ cảm biến analog
    
    Blynk.virtualWrite(V1, sensorValue);
    Blynk.virtualWrite(V2, sensor_Aout);

    // Model
    readSensorData(input_buffer);
    float* model_input = interpreter->input(0)->data.f;
    for (int i = 0; i < kInputSize; i++) {
        model_input[i] = input_buffer[i];
    }
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Failed to invoke TFLite interpreter");
        return;
    }

    float* model_output = interpreter->output(0)->data.f;
    int predicted_label = -1;
    float max_score = -FLT_MAX;
    for (int i = 0; i < kOutputSize; i++) {
        if (model_output[i] > max_score) {
            max_score = model_output[i];
            predicted_label = i;
        }
    }

    Serial.print("Predicted gas type: ");
    Serial.println(output_classes[predicted_label]);
    Serial.print("Gas: ");
    Serial.println(input_buffer[1]);
    lcd.clear(); 
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(input_buffer[2]);
    lcd.write(byte(0));
  
    lcd.setCursor(10, 0);
    lcd.print("H: ");
    lcd.print(input_buffer[0]);
    lcd.print("%");
  
    lcd.setCursor(0, 1);
    lcd.print("Gas: ");
    lcd.print(input_buffer[1]);
  
    lcd.setCursor(10, 1);
    lcd.print("L: ");
    lcd.print(predicted_label);

    delay(2000); // Đợi một khoảng thời gian trước khi đọc dữ liệu lại
}
    
