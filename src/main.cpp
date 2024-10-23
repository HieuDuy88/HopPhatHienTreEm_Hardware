
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include "ScioSense_ENS160.h" // ENS160 library
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <WiFiManager.h>
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SH110X.h>

#define IR_PIN 1
#define COI_PIN 10
#define TRIG_PIN 2
#define ECHO_PIN 3

#define TIME_INTERVAL 2000
#define TOPIC "message"
#define SERVER "nguyenduyhieu.xyz"
#define PORT 80

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)

// Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;
// ScioSense_ENS160 ens160(ENS160_I2CADDR_0); //0x52
ScioSense_ENS160 ens160(ENS160_I2CADDR_1); // 0x53..ENS160+AHT21
SocketIOclient socketIO;

float lastTempC = 0, lasteco2 = 0;
float hTemp, hSpO2, hRate;
float tempC, tempF, humidity, aqi, eco2, tvoc;
bool isChecking = false;
int totalCheck; // to see how many IR check done (to count the people)
unsigned long lastCheck = 0, lastRead = 0;
long duration;
float distance; // cm

//--------------------------------------------------------------------------
void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case sIOtype_DISCONNECT:
    Serial.printf("[IOc] Disconnected!\n");
    break;
  case sIOtype_CONNECT:
    Serial.printf("[IOc] Connected to url: %s\n", payload);
    // join default namespace (no auto join in Socket.IO V3)
    socketIO.send(sIOtype_CONNECT, "/");
    break;
  case sIOtype_EVENT:
    Serial.printf("[IOc] get event: %s\n", payload);
    break;
  case sIOtype_ACK:
    Serial.printf("[IOc] get ack: %u\n", length);
    break;
  case sIOtype_ERROR:
    Serial.printf("[IOc] get error: %u\n", length);
    break;
  case sIOtype_BINARY_EVENT:
    Serial.printf("[IOc] get binary: %u\n", length);
    break;
  case sIOtype_BINARY_ACK:
    Serial.printf("[IOc] get binary ack: %u\n", length);
    break;
  }
}
//-----------------------------------------------------------------------------
void sendDataToServer()
{
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add(TOPIC);
  JsonObject data = array.createNestedObject();
  data["tempC"] = tempC;
  data["humidity"] = humidity;
  data["aqi"] = aqi;
  data["eco2"] = eco2;
  data["tvoc"] = tvoc;
  data["hTemp"] = hTemp;
  data["hSpO2"] = hSpO2;
  data["hRate"] = hRate;
  data["ir"] = isChecking;
  String output;
  serializeJson(doc, output);
  Serial.println(output);
  socketIO.sendEVENT(output);
}
/*--------------------------------------------------------------------------
SETUP function
initiate sensor
--------------------------------------------------------------------------*/
void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
  }
  pinMode(IR_PIN, INPUT);
  pinMode(COI_PIN, OUTPUT);
  digitalWrite(COI_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // if (!display.begin(SCREEN_ADDRESS, true)) {
  //   Serial.println(F("SSD1306 allocation failed"));
  // }
  // display.cp437(true);  // Use full 256 char 'Code Page 437' font

  // display.clearDisplay();
  // display.setCursor(0, 24);
  // display.setTextSize(3);
  // display.print(" HELLO  ");
  // display.display();
  // delay(5000);
  // display.clearDisplay();

  // display.setCursor(0, 0);
  // display.setTextSize(1);
  // delay(1000);
  Serial.print("ENS160...");
  // display.print("ENS160...");
  // display.display();
  ens160.begin();
  Serial.println(ens160.available() ? "done." : "failed!");
  // display.println(ens160.available() ? "done." : "failed!");
  // display.display();
  // AHT20 start
  Serial.println("Adafruit AHT20!");
  if (!aht.begin())
  {
    Serial.println("Could not find AHT? Check wiring");
  }
  Serial.println("AHT20 found");
  // display.print("AHT20 found");
  // display.display();
  readAht();
  lastTempC = tempC;
  lasteco2 = eco2;
  // AHT20 end
  //---------------------------------------------------------------------------
  //  init wifi
  Serial.println("WiFi...");
  WiFiManager wifiManager;
  wifiManager.setTimeout(180); // wait 3 mins for config, or else use saved
  if (!wifiManager.autoConnect("DuyHieu"))
  {
    Serial.println("failed to connect and hit timeout");
  }
  else
  {
    Serial.println("Connected.");
  }
  // server address, port and URL
  Serial.println("Socket...");
  socketIO.begin(SERVER, PORT, "/socket.io/?EIO=4"); // /socket.io/?EIO=4
  socketIO.onEvent(socketIOEvent);
} // end void setup
/*--------------------------------------------------------------------------
MAIN LOOP FUNCTION
Cylce every 1000ms and perform measurement
--------------------------------------------------------------------------*/
void loop()
{

  socketIO.loop();
  if (millis() - lastRead > TIME_INTERVAL)
  {
    Serial.print("reading\n");
    readAht();
    readEns160();
    readDistance();
    // updateDisplay();
    if (readIR())
    {
      totalCheck++;
      readMLX();
      readMax();
      delay(50); // debounce
      lastCheck = millis();
      Serial.print("IR read");
    }
    lastRead = millis();
    sendDataToServer();
  }
  if (totalCheck % 2 != 0)
  {
    if (millis() - lastCheck >= 100000 && distance < 40.0)
    {
      lastCheck = millis();
      digitalWrite(COI_PIN, HIGH);
      delay(1000);
      digitalWrite(COI_PIN, LOW);
      delay(1000);
      digitalWrite(COI_PIN, HIGH);
      delay(1000);
      digitalWrite(COI_PIN, LOW);
      delay(1000);
      digitalWrite(COI_PIN, HIGH);
      delay(1000);
      digitalWrite(COI_PIN, LOW);
      delay(1000);
      digitalWrite(COI_PIN, HIGH);
      delay(1000);
      digitalWrite(COI_PIN, LOW);
      delay(1000);
    }
  }
  // display.display();
  delay(1);
}

// void updateDisplay() {
//   display.setTextColor(SH110X_WHITE, SH110X_BLACK);  // Draw white text
//   display.clearDisplay();
//   display.setCursor(0, 0);
//   display.print(F("temp: "));
//   display.println(tempC);
//   display.print(F("humidity: "));
//   display.println(humidity);
//   display.print(F("eco2: "));
//   display.println(eco2);
//   display.print(F("tvoc: "));
//   display.println(tvoc);
//   display.print(F("aqi: "));
//   display.println(aqi);
//   display.print(F("distance: "));
//   display.println(distance);
//   // int16_t i;
//   // for (i = 0; i < display.width(); i += 4) {
//   //   display.drawLine(0, 0, i, display.height() - 1, SH110X_WHITE);
//   //   display.display();  // Update screen with each newly-drawn line
//   //   delay(1);
//   // }
//   display.display();
// }

void readMLX()
{
  hTemp = random(365, 375) / 10;
}

void readMax()
{
  hRate = random(70, 110);
  hSpO2 = random(95, 99);
}

bool readIR()
{
  isChecking = digitalRead(IR_PIN);
  return isChecking;
}

void readAht()
{
  sensors_event_t humidity1, temp; // Tim had to change to humidity1
  aht.getEvent(&humidity1, &temp); // populate temp and humidity objects with fresh data
  tempC = (temp.temperature);
  tempF = (temp.temperature) * 1.8 + 32;
  humidity = (humidity1.relative_humidity);
}

void readEns160()
{
  if (ens160.available())
  {
    ens160.set_envdata(tempC, humidity);
    ens160.measure(true);
    ens160.measureRaw(true);
    aqi = ens160.getAQI();
    tvoc = ens160.getTVOC();
    eco2 = ens160.geteCO2();
    Serial.print(aqi);
    Serial.print('\t');
    Serial.print(tvoc);
    Serial.print('\t');
    Serial.print(eco2);
    Serial.print('\n');
  }
}

void readDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);

  distance = duration * 0.034 / 2;
}