#include "FS.h"
#include <Arduino.h>
#include <FastLED.h>
#include <Ticker.h>
#include <string.h>
// #include <ESP8266WiFi.h> // https://github.com/esp8266/Arduino

//needed for library
// #include <DNSServer.h>
// #include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <I2Cdev.h>

#include <ArduinoZMQ.h>

#include <pb_decode.h>
#include <pb_encode.h>
// #include <pb_common.h>
#include <proto/vridge-api.pb.h>

#define VRIDGE_HOST_CONFIG "/vridge-host"

#define DEVICE_TITLE "Controller"
char deviceName[32];

uint32_t controllerPacketCount = 0;

#define CONTROLLER_BUTTON_SYSTEM 0
#define CONTROLLER_BUTTON_MENU 1
#define CONTROLLER_BUTTON_GRIP 2
#define CONTROLLER_BUTTON_DPAD_LEFT 3
#define CONTROLLER_BUTTON_DPAD_UP 4
#define CONTROLLER_BUTTON_DPAD_RIGHT 5
#define CONTROLLER_BUTTON_DPAD_DOWN 6
#define CONTROLLER_BUTTON_A 7
#define CONTROLLER_BUTTON_AXIS0 32
#define CONTROLLER_BUTTON_AXIS1 33
#define CONTROLLER_BUTTON_AXIS2 34
#define CONTROLLER_BUTTON_AXIS3 35
#define CONTROLLER_BUTTON_AXIS4 36
#define CONTROLLER_BUTTON_TOUCHPAD CONTROLLER_BUTTON_AXIS0
#define CONTROLLER_BUTTON_TRIGGER CONTROLLER_BUTTON_AXIS1

#define UPDATE_INTERVAL_MS 16

#define ERROR_RESTART_INTERVAL 2000

enum ControllerState
{
    READY,
    BOOTING,
    ERROR,
    CONNECTING,
};
ControllerState currentState = BOOTING;

char hostname[32] = "";

ZMQSocket* setupSocket = NULL;
ZMQSocket* controllerSocket = NULL;
int controllerTimeout = 0;

unsigned long errorRestartTimestamp;

auto statusLED = new CRGB(CRGB::Black);

/**
 * Reset WiFi configuration and any other
 * persistent settings (like host address).
 */
void resetSettings(boolean reboot = true)
{
    if (SPIFFS.exists(VRIDGE_HOST_CONFIG))
    {
        SPIFFS.remove(VRIDGE_HOST_CONFIG);
    }

    WiFiManager wifiManager;
    wifiManager.resetSettings();

    if (reboot)
    {
        statusLED->setColorCode(CRGB::Black);
        FastLED.show();
        // ESP.restart();
        ESP.reset();
    }
}

#define LED_MAX_BRIGHTNESS UINT8_MAX / 5

boolean connectingAnimationProgressDirection = false;
uint8_t connectingAnimationProgress = 0;

#define CONNECTED_FADEOUT_INTERVAL 2500
#define CONNECTED_FADEOUT_FINAL_BRIGTNESS UINT8_MAX / 100
int connectedFadeoutAnimationProgress = 0;
unsigned long connectedFadeoutTimestamp;

/**
 * Set current controller state.
 */
void setState(ControllerState state)
{
    switch (state)
    {
        case ControllerState::CONNECTING:
        {
            connectingAnimationProgress = 0;
            connectingAnimationProgressDirection = false;
            // setupSocket->flush();
            break;
        }
        case ControllerState::READY:
        {
            connectedFadeoutAnimationProgress = 0;
            connectedFadeoutTimestamp = millis();
            controllerPacketCount = 0;
            break;
        }
        case ControllerState::ERROR:
        {
            errorRestartTimestamp = millis();

            if (currentState == ControllerState::BOOTING)
            {
                resetSettings();
            }

            break;
        }
        default:
        {
            //
        }
    }

    currentState = state;
}

// void connectVRidge(ZMQSocket* socket)
// {
//     Serial.println("Trying to connect...");
//     socket->send("{\"ProtocolVersion\":1,\"Code\":2}");

//     auto reply = socket->recv();

//     DynamicJsonDocument doc(2048);

//     deserializeJson(doc, reply->c_str());

//     boolean hasControllerEndpoint = false;

//     if (doc["Code"] == 0) // OK
//     {
//         for (JsonObject o : doc["Endpoints"].as<JsonArray>())
//         {
//             if (strcmp(o["Name"].as<const char*>(), "Controller") == 0)
//             {
//                 hasControllerEndpoint = true;
//             }
//         }
//     }
//     else if (doc["Code"] == 2) // InUse
//     {
//         delay(1000);
//         connectVRidge(socket);
//     }

//     if (hasControllerEndpoint)
//     {
//         JsonObject object = doc.to<JsonObject>();
//         object["RequestingAppName"] = deviceName;
//         object["RequestedEndpointName"] = "Controller";
//         object["ProtocolVersion"] = 3;
//         object["Code"] = 1;

//         char buffer[1024];

//         serializeJson(object, buffer);

//         socket->send(buffer);

//         auto reply = socket->recv();

//         Serial.println(reply->c_str());

//         deserializeJson(doc, reply->c_str());

//         if (doc["Code"] == 0) // OK
//         {
//             int port = doc["Port"].as<int>();
//             controllerTimeout = doc["TimeoutSec"].as<int>();

//             if (port > 0)
//             {
//                 Serial.printf("Controller service port: %d\n", port);
//                 controllerSocket = ZMQSocket::connect(REQ, hostname, port);
//             }
//         }
//         else if (doc["Code"] == 2) // InUse
//         {
//             delay(1000);
//             connectVRidge(socket);
//         }
//     }
// }

#define MUX_CLK D5
#define MUX_IN D6
#define MUX_RST D7

#define MAX_BUTTONS 3
#define MENU_BUTTON 0
#define SYSTEM_BUTTON 1
#define GRIP_BUTTON 2
// #define TOUCHPAD_BUTTON 3
// #define TRIGGER_BUTTON 4

boolean buttonPressed[MAX_BUTTONS] = {false};

/**
 * Read hardware buttons and update `buttonPressed[]` array.
 */
void readButtons()
{
    // Reset hardware counter
    digitalWrite(MUX_RST, HIGH);
    digitalWrite(MUX_RST, LOW);

    for (size_t i = 0; i < MAX_BUTTONS; i++)
    {
        // Active low logic
        buttonPressed[i] = digitalRead(MUX_IN) == LOW;

        // Advance hardware counter
        digitalWrite(MUX_CLK, HIGH);
        digitalWrite(MUX_CLK, LOW);
    }
}

void updateStatusLED()
{
    switch (currentState)
    {
        case ControllerState::BOOTING:
        case ControllerState::CONNECTING:
        {
            if (currentState == ControllerState::CONNECTING)
            {
                statusLED->setColorCode(0x0090ff);
            }
            else
            {
                statusLED->setColorCode(0xffffff);
            }

            FastLED.setBrightness(connectingAnimationProgress);
            FastLED.show();

            if (connectingAnimationProgressDirection == false)
            {
                connectingAnimationProgress++;
            }
            else
            {
                connectingAnimationProgress--;
            }

            if (connectingAnimationProgress == 0 ||
                connectingAnimationProgress == LED_MAX_BRIGHTNESS)
            {
                connectingAnimationProgressDirection =
                    !connectingAnimationProgressDirection;
            }

            break;
        }
        case ControllerState::READY:
        {
            statusLED->setColorCode(0x00ff00);

            if (millis() - connectedFadeoutTimestamp >= CONNECTED_FADEOUT_INTERVAL)
            {
                if (connectedFadeoutAnimationProgress < UINT8_MAX)
                {
                    auto brightness = LED_MAX_BRIGHTNESS -
                                      LED_MAX_BRIGHTNESS * ((float)connectedFadeoutAnimationProgress / UINT8_MAX);

                    if (brightness >= CONNECTED_FADEOUT_FINAL_BRIGTNESS)
                    {
                        FastLED.setBrightness(brightness);
                    }

                    connectedFadeoutAnimationProgress++;
                }
            }
            else
            {
                FastLED.setBrightness(LED_MAX_BRIGHTNESS);
            }

            FastLED.show();

            break;
        }
        default:
        {
            connectingAnimationProgress = 0;
            statusLED->setColorCode(CRGB::Black);
            FastLED.show();
        }
    }
}

#define SYSTEM_RESET_INTERVAL 5000
unsigned long systemResetTimestamp;

void scanButtons()
{
    boolean previousButtonPressed[MAX_BUTTONS] = {false};
    for (size_t i = 0; i < MAX_BUTTONS; i++)
    {
        previousButtonPressed[i] = buttonPressed[i];
    }

    readButtons();

    // If combination is not pressed
    if (previousButtonPressed[SYSTEM_BUTTON] == false ||
        previousButtonPressed[GRIP_BUTTON] == false)
    {
        systemResetTimestamp = millis();
    }

    // If combination is pressed
    if (buttonPressed[SYSTEM_BUTTON] == true &&
        buttonPressed[GRIP_BUTTON] == true)
    {
        if (millis() - systemResetTimestamp >= SYSTEM_RESET_INTERVAL)
        {
            resetSettings();
        }
    }
}

Ticker statusLEDTicker;
Ticker buttonScannerTicker;

void setup()
{
    pinMode(MUX_CLK, OUTPUT); // MUX_CLK
    digitalWrite(MUX_CLK, LOW);
    pinMode(MUX_IN, INPUT);   // MUX_IN
    pinMode(MUX_RST, OUTPUT); // MUX_RST
    pinMode(D8, OUTPUT);      // LED_DATA

    /**
     * Setup FastLED
     */
    FastLED.addLeds<WS2812B, 8, EOrder::GRB>(statusLED, 1);
    statusLED->setColorCode(CRGB::Black);
    FastLED.setBrightness(LED_MAX_BRIGHTNESS);
    FastLED.show();

    /**
     * Attach ticker functions
     */
    statusLEDTicker.attach_ms(24, updateStatusLED);
    buttonScannerTicker.attach_ms(64, scanButtons);

    Serial.begin(115200);

    if (!SPIFFS.begin())
    {
        Serial.println("ERROR: Unable to mount SPIFFS.");
        setState(ERROR);
        return;
    }

    sprintf(deviceName, "%s %02X", DEVICE_TITLE, ESP.getChipId());

    WiFiManager wifiManager;

    auto parameter = new WiFiManagerParameter(
        "server", "VRidge server address", "", 32);

    wifiManager.addParameter(parameter);

    wifiManager.setAPStaticIPConfig(
        IPAddress(10, 0, 1, 1),
        IPAddress(10, 0, 1, 1),
        IPAddress(255, 255, 255, 0));

    wifiManager.autoConnect(deviceName);

    // Serial.println(p->getValueLength());

    if (strlen(parameter->getValue()) > 0)
    {
        auto file = SPIFFS.open(VRIDGE_HOST_CONFIG, "w");
        // file.write(p->getValue());
        file.print(parameter->getValue());
        file.close();
    }

    delete parameter;

    if (SPIFFS.exists(VRIDGE_HOST_CONFIG))
    {
        auto file = SPIFFS.open(VRIDGE_HOST_CONFIG, "r");

        file.readBytes(
            hostname,
            file.size() > sizeof(hostname)
                ? sizeof(hostname)
                : file.size());
        file.close();
    }

    SPIFFS.end();

    if (strlen(hostname) == 0)
    {
        Serial.println("ERROR: Unable to read vridge host address.");
        setState(ERROR);
        return;
    }
    else
    {
        setupSocket = ZMQSocket::connect(REQ, hostname, 38219);
        setState(ControllerState::CONNECTING);

        // connectVRidge(setupSocket);
    }
}

unsigned long controllerUpdateTimestamp = millis();
unsigned long testMillis = millis();

bool encodeVelocity(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    // XYZ vector
    double velocity[] = {0, 0, 0};

    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed64(stream, &velocity[0]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed64(stream, &velocity[1]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed64(stream, &velocity[2]);
};

uint32_t positionProgress = 0;

bool encodePosition(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    // XYZ vector (x - right, y - up, z - )
    // float position[] = {0, 1.5f, -0.5f};
    float position[] = {0, 1.5f, -0.5f};

    // position[1] = positionProgress++ / 100.0f;

    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &position[0]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &position[1]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &position[2]);
};

bool encodeOrientation(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    // XYZW quaternion
    float orientation[] = {0.5f, 0, 0, 0};

    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &orientation[0]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &orientation[1]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &orientation[2]) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_fixed32(stream, &orientation[3]);
};

bool toggle = false;

void updateState()
{
    ControllerStateRequest request = ControllerStateRequest_init_default;

    request.has_Version = true;
    request.Version = 3;
    request.has_TaskType = true;
    request.TaskType = 0x01; // SendFullState

    VRController controllerState = VRController_init_default;

    // VRControllerState_t buttonState;

    controllerState.has_ControllerId = true;
    controllerState.ControllerId = ESP.getChipId();
    controllerState.has_Status = true;
    controllerState.Status = 0; // active
    controllerState.has_SuggestedHand = true;
    controllerState.SuggestedHand = HandType_Right;
    controllerState.Velocity.funcs.encode = encodeVelocity;
    controllerState.Position.funcs.encode = encodePosition;
    controllerState.Orientation.funcs.encode = encodeOrientation;

    VRControllerState_t buttonState = VRControllerState_t_init_default;

    buttonState.has_ulButtonPressed = true;
    buttonState.ulButtonPressed = 0;
    buttonState.ulButtonPressed |= (uint64_t)(buttonPressed[SYSTEM_BUTTON] ? 1 : 0)
                                   << CONTROLLER_BUTTON_SYSTEM;
    buttonState.ulButtonPressed |= (uint64_t)(buttonPressed[MENU_BUTTON] ? 1 : 0)
                                   << CONTROLLER_BUTTON_MENU;
    buttonState.ulButtonPressed |= (uint64_t)(buttonPressed[GRIP_BUTTON] ? 1 : 0)
                                   << CONTROLLER_BUTTON_GRIP;
    // buttonState.ulButtonPressed |= (uint64_t)1 << CONTROLLER_BUTTON_TOUCHPAD;
    // buttonState.ulButtonPressed |= (uint64_t)1 << CONTROLLER_BUTTON_TRIGGER;

    buttonState.has_unPacketNum = true;
    buttonState.unPacketNum = ++controllerPacketCount;

    VRControllerAxis_t touchpadAxis = VRControllerAxis_t_init_default;
    touchpadAxis.has_x = true;
    touchpadAxis.x = 0;
    touchpadAxis.has_y = true;
    touchpadAxis.y = 0;

    buttonState.has_rAxis0 = true;
    buttonState.rAxis0 = touchpadAxis;

    VRControllerAxis_t triggerAxis = VRControllerAxis_t_init_default;
    triggerAxis.has_x = true;
    triggerAxis.x = 0;

    buttonState.has_rAxis1 = true;
    buttonState.rAxis1 = triggerAxis;

    controllerState.has_ButtonState = true;
    controllerState.ButtonState = buttonState;

    request.has_ControllerState = true;
    request.ControllerState = controllerState;

    uint8_t buffer[1024] = {};
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    pb_encode(&stream, ControllerStateRequest_fields, &request);

    // Serial.println("--------------------");
    // for (size_t i = 0; i < stream.bytes_written; i++)
    // {
    //     Serial.printf("%02X", buffer[i]);
    // }
    // Serial.println();

    auto status = controllerSocket->send(
        new ZMQMessage(buffer, stream.bytes_written));

    toggle = !toggle;

    if (status != ZMQ_STATUS_OK)
    {
        setState(ControllerState::CONNECTING);
    }
}

void connectVRidge()
{
    if (controllerSocket != NULL)
    {
        delete controllerSocket;
        controllerSocket = NULL;
    }

    if (setupSocket == NULL)
    {
        setState(ERROR);
        return;
    }

    auto status = setupSocket->send("{\"ProtocolVersion\":1,\"Code\":2}");

    if (status != ZMQ_STATUS_OK)
    {
        return;
    }

    auto reply = setupSocket->recv();

    if (reply == NULL)
    {
        return;
    }

    DynamicJsonDocument doc(2048);

    deserializeJson(doc, reply->c_str());

    delete reply;

    boolean hasControllerEndpoint = false;

    if (doc["Code"] == 0) // OK
    {
        for (JsonObject o : doc["Endpoints"].as<JsonArray>())
        {
            if (strcmp(o["Name"].as<const char*>(), "Controller") == 0)
            {
                hasControllerEndpoint = true;
            }
        }
    }
    else if (doc["Code"] == 2) // InUse
    {
        return;
    }

    if (hasControllerEndpoint)
    {
        JsonObject object = doc.to<JsonObject>();
        object["RequestingAppName"] = deviceName;
        object["RequestedEndpointName"] = "Controller";
        object["ProtocolVersion"] = 3;
        object["Code"] = 1;

        char buffer[1024];

        serializeJson(object, buffer);

        setupSocket->send(buffer);

        auto reply = setupSocket->recv();

        if (reply == NULL)
        {
            return;
        }

        deserializeJson(doc, reply->c_str());

        delete reply;

        if (doc["Code"] == 0) // OK
        {
            int port = doc["Port"].as<int>();
            controllerTimeout = doc["TimeoutSec"].as<int>();

            if (port > 0)
            {
                Serial.printf("Controller service port: %d\n", port);

                controllerSocket = ZMQSocket::connect(REQ, hostname, port);

                setState(ControllerState::READY);
            }
        }
        else if (doc["Code"] == 2) // InUse
        {
            return;
        }
    }
}

void loop()
{
    // Serial.printf("FREE HEAP: %d FREE STACK: %d\n",
    //               ESP.getFreeHeap(),
    //               ESP.getFreeContStack());

    switch (currentState)
    {
        case ControllerState::READY:
        {
            // ZMQSocket::work();

            if (controllerSocket != NULL)
            {
                // if (millis() - controllerUpdateTimestamp >= UPDATE_INTERVAL_MS)
                // {
                controllerUpdateTimestamp = millis();
                updateState();
                // }
                // if (millis() - controllerUpdateTimestamp >= 10000)
                // {
                //     delete controllerSocket;
                //     controllerSocket = NULL;
                //     setState(CONNECTING);
                // }
            }
            break;
        }
        case ControllerState::CONNECTING:
        {
            delay(200);
            connectVRidge();
            break;
        }
        case ControllerState::ERROR:
        {
            if (millis() - errorRestartTimestamp >= ERROR_RESTART_INTERVAL)
            {
                resetSettings();
            }
            break;
        }
        default:
        {
            //
        }
    }
}
