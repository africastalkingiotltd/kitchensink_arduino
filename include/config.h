/**
 * @brief Credentials configuration header (Mandatory!!)
 * 
 * @APPLICATION_USERNAME - This is your application username that is associated with your IoT Appplication. Refer to https://help.africastalking.com/en/articles/2249244-what-is-my-username-and-api-key
 * @DEVICE_GROUP_NAME - This is the device group that the IoT device is to connect to 
 * @DEVICE_ID - This is the device identifier that is sent to the Africa's Talking Broker 
 * @DEVICE_GROUP_PASSWORD - This is the devive group password that the IoT device is to use to establish a connection 
 * @MQTT_CREDENTIALS - This will resolve to <username>:<device-group>
 * @TOPIC_PREFIX - This will always start with <username>/<device-group>/
 */
#define APPLICATION_USERNAME "" 
#define DEVICE_GROUP_NAME ""
#define DEVICE_ID ""
#define DEVICE_GROUP_PASSWORD ""
#define MQTT_CREDENTIALS APPLICATION_USERNAME ":" DEVICE_GROUP_NAME
#define TOPIC_PREFIX APPLICATION_USERNAME "/" DEVICE_GROUP_NAME "/"