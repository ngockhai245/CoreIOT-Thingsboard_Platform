#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>
#include <Shared_Attribute_Update.h>
#include "DHT20.h"
#include "DHT.h"

#define LEDPIN 5
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// define the wifi ssid and password
const char WIFI_SSID[] = "Wifi11";
const char WIFI_PASSWORD[] = "123456789";

// define the thingsboard server and port
const char THINGSBOARD_SERVER[] = "app.coreiot.io";
const uint16_t THINGSBOARD_PORT = 1883;
const char TOKEN[] = "7l23s5snln10k419oc0n";

// define max size of the message
const uint32_t MAX_MESSAGE_SIZE = 1024;

// parameter to subscribe shared attribute
bool STATE = false;
void processSharedAttributeUpdate(const JsonObjectConst &data)
{
  for (auto it = data.begin(); it != data.end(); ++it)
  {
    Serial.println(it->key().c_str());
    if (strcmp(it->key().c_str(), "STATE") == 0)
    {
      STATE = it->value().as<bool>();
      digitalWrite(LEDPIN, STATE);

      const size_t jsonSize = Helper::Measure_Json(data);
      char buffer[jsonSize];
      serializeJson(data, buffer, jsonSize);
      Serial.println(buffer);

      if (STATE)
        Serial.println("Turn on the led");
      else
        Serial.println("Turn off the led");
    }
  }
}

constexpr uint8_t MAX_SHARED_ATTR = 5U;
Shared_Attribute_Update<1U, MAX_SHARED_ATTR> sharedAttributeUpdate;

const char ledState[] = "STATE";
constexpr std::array<const char *, 1U> SUBSCRIBED_SHARED_ATTRIBUTES = {ledState};
const Shared_Attribute_Callback<MAX_SHARED_ATTR> callback(&processSharedAttributeUpdate, SUBSCRIBED_SHARED_ATTRIBUTES);

// RPC callback
void processGetValidationVal(const JsonVariantConst &data, JsonDocument &response)
{
  // Process data
  Serial.println("Processing RPC data...");
  const bool isValid = data["isValid"];
  if (!isValid)
    Serial.println("DHT20 data is invalid");
  else
    Serial.println("DHT20 data is valid");

  response["isValid"] = isValid;
}

const char RPC_TRUE_METHOD[] = "rpcTrueCommand";
const char RPC_FALSE_METHOD[] = "rpcFalseCommand";

constexpr uint8_t MAX_RPC_RESPONSE = 5U;
Server_Side_RPC<2U, MAX_RPC_RESPONSE> rpc;

const std::array<RPC_Callback, 2U> RPCcallbacks = {
    // Requires additional memory in the JsonDocument for the JsonDocument that will be copied into the response
    RPC_Callback{RPC_TRUE_METHOD, processGetValidationVal},
    RPC_Callback{RPC_FALSE_METHOD, processGetValidationVal},
};

const std::array<IAPI_Implementation *, 2U> apis = {
    &sharedAttributeUpdate,
    &rpc,
};
// define object instantiation to communicate with thingsboar server
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard thingsBoard(mqttClient, MAX_MESSAGE_SIZE, MAX_MESSAGE_SIZE, Default_Max_Stack_Size, apis);

DHT20 dht20;
bool sharedAttributeSubscribed = false;
bool RPCSubscribed = false;

void initWifi()
{
  // establish a connection to wifi
  Serial.println("Start connecting to Access Point");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    // waiting until connecting to wifi successfully
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to Access Point");
}

void taskReconnectWifi(void *pvParameters)
{
  (void)pvParameters;

  while (true)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      initWifi();
    }
    else
      Serial.println("Wifi already connected");
    // check every 10s
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void taskConnectToCoreIoT(void *pvParameters)
{
  (void)pvParameters;
  while (true)
  {
    if (!thingsBoard.connected())
    {
      if (!thingsBoard.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT))
      {
        Serial.println("Failed to connect to CoreIoT");
      }
      else
        Serial.println("Connecting to CoreIoT successfully");
    }
    else
      Serial.println("CoreIoT already connected");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void taskSubscribeSharedAttribute(void *pvParameters)
{
  (void)pvParameters;
  while (true)
  {
    if (!sharedAttributeSubscribed)
    {
      if (!sharedAttributeUpdate.Shared_Attributes_Subscribe(callback))
      {
        Serial.println("Failed to subscribe for shared attribute updates");
        return;
      }
      Serial.println("Subscribe shared attribute done");
      sharedAttributeSubscribed = true;
    }
    vTaskDelay(9000 / portTICK_PERIOD_MS);
  }
}

void taskSubscribeRPCCallback(void *pvParameters)
{
  (void)pvParameters;
  while (true)
  {
    if (!RPCSubscribed)
    {
      Serial.println("Subscribing for RPC...");
      if (!rpc.RPC_Subscribe(RPCcallbacks.cbegin(), RPCcallbacks.cend()))
      {
        Serial.println("Failed to subscribe for RPC");
        return;
      }

      Serial.println("Subscribe RPC done");
      RPCSubscribed = true;
    }
    vTaskDelay(9000 / portTICK_PERIOD_MS);
  }
}

void taskReadDHT(void *pvParameters)
{
  while (1)
  {
    dht.read();
    // float temperature = 100;
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity))
    {
      Serial.println("Can not read data from DHT!");
    }
    else
    {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C, ");

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      StaticJsonDocument<256> jsonBuffer;
      jsonBuffer["temperature"] = temperature;
      jsonBuffer["humidity"] = humidity;

      bool sent = thingsBoard.sendTelemetryJson(jsonBuffer, measureJson(jsonBuffer));
      if (sent)
      {
        Serial.println("Send data successfully");
      }
    }
    Serial.println();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  // Wire.begin();
  // dht20.begin();
  dht.begin();
  initWifi();
  delay(2000);
  xTaskCreate(taskReconnectWifi, "reconnectWifi", 4096, NULL, 1, NULL);
  xTaskCreate(taskConnectToCoreIoT, "connectToCoreIoT", 4096, NULL, 1, NULL);
  xTaskCreate(taskSubscribeSharedAttribute, "subscribeSharedAttribute", 2048, NULL, 1, NULL);
  xTaskCreate(taskSubscribeRPCCallback, "taskSubscribeRPCCallback", 2048, NULL, 1, NULL);
  xTaskCreate(taskReadDHT, "readDHT", 2048, NULL, 1, NULL);
}

void loop()
{
  thingsBoard.loop();
  delay(1000);
}
