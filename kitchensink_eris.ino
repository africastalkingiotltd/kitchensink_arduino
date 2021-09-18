#define TINY_GSM_MODEM_SIM800

#include "./include/config.h"

#include <Countdown.h>
#include <IPStack.h>
#include <MQTTClient.h>
#include <MQTTPacket.h>
#include <TinyGsmClient.h>
#include <DHT.h>
#include <Servo.h>

// BEGIN GSM CONFIG

const char apn[] = "iot.safaricom.com";
const char user[] = "none";
const char pass[] = "none";

// This is for STM32Duino Core or PlatformIO, comment this out when using a different Core
HardwareSerial GSMSerial(PA3, PA2);

/**
 * @brief Setting up the UART and initializing the GSM Client
 * 
 *  You need to be careful here. 
 *  Depending on the STM32 Core you're using you must be aware of the Serial Configuration.
 *  Consult the relevant docs if you can not see anything on the Serial.
 * 
 *  The GSM module power is at PB15 and needs to be set HIGH for 3 seconds then LOW once
 */
#define SerialMon Serial
#define SerialAT GSMSerial

#define GSM_POWER_KEY PB15

// DEBUGGING: (un)Comment out this line if you do not want to see the AT Command stream from Serial 2
//#define DUMP_AT_COMMANDS

#define TINY_GSM_DEBUG SerialMon

// Uses an extra library
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// END DEBUGGING CONF
TinyGsmClient tinyGSMClient(modem);

// END GSM CONFIG

// BEGIN MQTT CONFIG

const char mqttUsername[] = MQTT_CREDENTIALS;
const char mqttPassword[] = DEVICE_GROUP_PASSWORD;
const char mqttDeviceID[] = DEVICE_ID;

/**
 * @brief Setup Topics
 * 
 *  Notes on Topics: 
 * 
 *  birth topic - This is a topic that the device publishes to
 *  when it makes the first MQTT Connection or any other subsequent
 *  reconnections. It is not enforced but encouraged as a good practice.
 * 
 *  will topic - This is a topic that the device should publish to when 
 *  it disconnects. This can be used to detect your device going offline.
 *  It is sent together with the CONNECT payload.
 *  Also not enforced but considered a good practice.
 */
const char *birthTopic = TOPIC_PREFIX "birth";
const char *willTopic = TOPIC_PREFIX "will";
const char *servoTopic = TOPIC_PREFIX "servo";
const char *humidityTopic = TOPIC_PREFIX "humidity";
const char *soilMoistureTopic = TOPIC_PREFIX "moisture";
const char *temperatureTopic = TOPIC_PREFIX "temperature";
const char *ledTopic = TOPIC_PREFIX "led";
const char *lightIntensityTopic = TOPIC_PREFIX "light";
const char *ultraSonicDataTopic = TOPIC_PREFIX "distance";

const char birthMessage[] = "CONNECTED";
const char willMessage[] = "DISCONNECTED";

char brokerAddress[] = "broker.africastalking.com";
int brokerPort = 1883;

// END MQTT CONFIG

bool brokerConnect(void);
bool gsmConnect(void);
void getModemData(void);
void incomingMessageHandlerServo(MQTT::MessageData &messageData);
void incomingMessageHandlerLED(MQTT::MessageData &messageData);

void sendTemperature(void);
void sendHumidity(void);
void sendLightIntensity(void);
void sendUltraSonicData(void);

constexpr unsigned int str2int(const char *str, int h)
{
    return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

IPStack ipstack(tinyGSMClient);
MQTT::Client<IPStack, Countdown, 128, 3> mqttClient = MQTT::Client<IPStack, Countdown, 128, 3>(ipstack);

char buffer[100];
int returnCode = 0;

int modemConnAttemptsCount = 0;

// Update these based on you preferred wiring
#define TRIGGER_PIN PB10
#define ECHO_PIN PB11
#define DHTPIN PB1
#define LDR_PIN PA6
#define SOIL_MOISTURE_PIN PB0
#define LED_PIN PC13
#define SERVO_PIN PA7

#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
Servo servo;

void setup()
{
    SerialMon.setRx(PA10);
    SerialMon.setTx(PA9);
    SerialMon.begin(115200);
    delay(100);
    SerialAT.begin(115200);
    delay(100);
    while (!SerialMon || !SerialAT)
    {
        ; // Serial Not working
    }

    pinMode(GSM_POWER_KEY, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LDR_PIN, INPUT_ANALOG);
    pinMode(SOIL_MOISTURE_PIN, INPUT_ANALOG);
    // pinMode(DHT_PIN, INPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(TRIGGER_PIN, OUTPUT);

    // GSM ON
    digitalWrite(GSM_POWER_KEY, 1);
    delay(3000);
    digitalWrite(GSM_POWER_KEY, 0);

    delay(100);
    dht.begin();
    servo.attach(SERVO_PIN);
}

void loop()
{
    SerialMon.println("Starting up");
    getModemData();
    sprintf(buffer, "Is network connected(0 False / 1 True)? : %i ", modem.isNetworkConnected());
    SerialMon.println(buffer);
    if (!modem.isGprsConnected() || !mqttClient.isConnected())
    {
        if (gsmConnect()== false)
        {
            SerialMon.println("[ERROR] GPRS Reconnection failed. Trying again.");
        }

        // Let's reconnect to the broker

        // Clean up connection???
        mqttClient.disconnect();
        if (brokerConnect() == true)
        {
            SerialMon.println("[ERROR] Failed to reconnnect to the broker. Trying again.");
        }
    }
    mqttClient.yield(1000);

    sendTemperature();
    sendHumidity();
    sendUltraSonicData();
    sendLightIntensity();

    delay(1500);
}

void getModemData(void)
{
    String name = modem.getModemInfo();
    SerialMon.print("Modem Info: ");
    SerialMon.println(name);
    String info = modem.getModemInfo();
    SerialMon.print("Modem Name: ");
    SerialMon.println(info);
    String ccid = modem.getSimCCID();
    SerialMon.print("CCID: ");
    SerialMon.println(ccid);
    String imei = modem.getIMEI();
    SerialMon.print("IMEI: ");
    SerialMon.println(imei);
    String imsi = modem.getIMSI();
    SerialMon.print("IMSI: ");
    SerialMon.println(imsi);
    String cop = modem.getOperator();
    SerialMon.print("Operator: ");
    SerialMon.println(cop);
}

bool gsmConnect(void)
{
    if (modemConnAttemptsCount > 0)
    {
        SerialMon.println("Modem reconnnection had been attempted earlier. Restarting");
        // FIXME: We're doing this in another piece of C code, but I fully do not understand the implications of regularly switching on the power-key
        digitalWrite(GSM_POWER_KEY, 1);
        delay(3000); // Should we do this here?????
        digitalWrite(GSM_POWER_KEY, 0);
        modem.restart();
    }
    sprintf(buffer, "Getting the modem ready \r\n");
    SerialMon.print(buffer);
    modem.init();
    sprintf(buffer, "Initializing GSM network registration...\r\n");
    SerialMon.print(buffer);
    if (!modem.waitForNetwork())
    {
        sprintf(buffer, "\r\n Unable to initialize registration. Reset and try again.\r\n");
        SerialMon.print(buffer);
        modemConnAttemptsCount++;
        return false; // Exit
    }
    sprintf(buffer, "GSM OK\r\n");
    SerialMon.print(buffer);

    sprintf(buffer, "Attempting to establish GPRS connection \r\n");
    SerialMon.print(buffer);
    if (!modem.gprsConnect(apn, user, pass))
    {
        sprintf(buffer, "Unable to connect to APN. Reset and try again \r\n");
        SerialMon.print(buffer);
        modemConnAttemptsCount++;
        return false; // Exit
    }

    sprintf(buffer, "GSM is Okay \r\n");
    SerialMon.print(buffer);
    modemConnAttemptsCount = 0;
    return true;
}

bool brokerConnect(void)
{
    MQTT::Message mqttMessage;
    snprintf(buffer, sizeof(buffer), "Connecting to %s on port %i \r\n", brokerAddress, brokerPort);
    returnCode = ipstack.connect(brokerAddress, brokerPort);
    SerialMon.println(buffer);
    if (returnCode != 1)
    {
        snprintf(buffer, sizeof(buffer), "Unable to connect to Broker TCP Port. \r\n");
        SerialMon.println(buffer);
        return false; // Exit immediately
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Broker TCP port open \r\n");
        SerialMon.println(buffer);
    }
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4;
    data.clientID.cstring = (char *)mqttDeviceID;
    data.username.cstring = (char *)mqttUsername;
    data.password.cstring = (char *)mqttPassword;
    data.keepAliveInterval = 60;
    data.cleansession = 1;
    data.will.message.cstring = (char *)willMessage;
    data.will.qos = MQTT::QOS1;
    data.will.retained = 0;
    data.will.topicName.cstring = (char *)willTopic;
    returnCode = mqttClient.connect(data);
    if (returnCode != 0)
    {
        snprintf(buffer, sizeof(buffer), "Error establishing connection session with  broker. Error Code %i. \r\n", returnCode);
        SerialMon.print(buffer);
        return false; // Exit immediately
    }
    mqttMessage.qos = MQTT::QOS1;
    mqttMessage.retained = false;
    mqttMessage.dup = false;
    mqttMessage.payload = (void *)birthMessage;
    mqttMessage.payloadlen = strlen(birthMessage) + 1;
    returnCode = mqttClient.publish(birthTopic, mqttMessage);
    snprintf(buffer, sizeof(buffer), "Birth topic publish return code %i \n", returnCode);
    SerialMon.println(buffer);
    returnCode = mqttClient.subscribe(servoTopic, MQTT::QOS1, incomingMessageHandlerServo);
    if (returnCode != 0)
    {
        snprintf(buffer, sizeof(buffer), "Unable to subscribe to servo topic. Hanginng the process\r\n");
        SerialMon.print(buffer);
        return false; // Exit immediately
    }
    returnCode = mqttClient.subscribe(ledTopic, MQTT::QOS1, incomingMessageHandlerLED);
    if (returnCode != 0)
    {
        snprintf(buffer, sizeof(buffer), "Unable to subscribe to LED topic. Hanging the process\n");
        SerialMon.print(buffer);
        return false; // Exit immediately
    }
    snprintf(buffer, sizeof(buffer), "Successfully connected to the broker\n");
    SerialMon.println(buffer);
    return true;
}

void publishMessage(char *payload, const char *topic)
{
    MQTT::Message message;
    message.qos = MQTT::QOS1;
    message.payload = (void *)payload;
    message.retained = 0;
    message.payloadlen = strlen(payload) + 1;
    returnCode = mqttClient.publish(topic, message);
    snprintf(buffer, sizeof(buffer), "%s topic publish return code %i \n", topic, returnCode);
    SerialMon.println(buffer);
}

void sendHumidity(void)
{
    float humidity = dht.readHumidity();
    dtostrf(humidity, 3, 2, buffer);
    SerialMon.print("Humidity: ");
    SerialMon.println(buffer);
    publishMessage(buffer, humidityTopic);
}

void sendTemperature(void)
{
    float temperature = dht.readTemperature();
    dtostrf(temperature, 3, 2, buffer);
    SerialMon.print("Temperature: ");
    SerialMon.println(buffer);
    publishMessage(buffer, temperatureTopic);
}

void sendLightIntensity(void)
{
    int intensity = analogRead(LDR_PIN);
    snprintf(buffer, sizeof(buffer), "%i", intensity);
    SerialMon.print("Light: ");
    SerialMon.println(buffer);
    publishMessage(buffer, lightIntensityTopic);
}

void sendUltraSonicData(void)
{
    long duration, cm;
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH);
    cm = (duration / 2) / 29.1;
    snprintf(buffer, sizeof(buffer), "%lu", cm);
    SerialMon.print("Proximity: ");
    SerialMon.println(buffer);
    publishMessage(buffer, ultraSonicDataTopic);
}

void incomingMessageHandlerServo(MQTT::MessageData &messageData)
{
    char cmd[10];
    MQTT::Message &message = messageData.message;
    snprintf(cmd, sizeof(cmd), "%s", messageData.message.payload);
    SerialMon.print(F("Incoming Servo Message: "));
    SerialMon.println(cmd);
    switch (str2int(cmd, 0))
    {
    case str2int("close", 0):
        servo.write(0);
        break;
    case str2int("open", 0):
        servo.write(180);
        break;
    default:
        SerialMon.print("Unknown Servo Command: ");
        SerialMon.println(cmd);
        break;
    }
    memset((char *)message.payload, NULL, sizeof(cmd));
}

void incomingMessageHandlerLED(MQTT::MessageData &messageData)
{
    char cmd[10];
    MQTT::Message &message = messageData.message;
    snprintf(cmd, sizeof(cmd), "%s", messageData.message.payload);
    SerialMon.print(F("Incoming LED Message: "));
    SerialMon.println(cmd);
    switch (str2int(cmd, 0))
    {
    case str2int("on", 0):
        digitalWrite(LED_PIN, 1);
        break;
    case str2int("off", 0):
        digitalWrite(LED_PIN, 0);
        break;
    default:
        SerialMon.print("Unknown LED Command: ");
        SerialMon.println(cmd);
        break;
    }
    memset((char *)message.payload, NULL, sizeof(cmd));
}
