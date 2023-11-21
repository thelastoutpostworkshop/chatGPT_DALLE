#include <PNGdec.h>
#include <TFT_eSPI.h>
#include "WebServer.h"
#include <WiFiClientSecure.h>
#include "arduino_base64.hpp"

#include "mandalaBase64Png.h"
#include "rainbowBase64Png.h"

#define MAX_IMAGE_WIDTH 1024                      // Adjust for your images
#define PSRAM_BUFFER_DECODED_LENGTH 4000000L      // Length of buffer for base64 data decoding in PSRAM
#define PSRAM_BUFFER_READ_ENCODED_LENGTH 2000000L // Length of buffer for reading the base64 encoded data in PSRAM
#define BUFFER_RESPONSE_LENGTH 1024               // Length of buffer for reading api response in chunks

int16_t xpos = 0;
int16_t ypos = 0;

PNG png; // PNG decoder instance
TFT_eSPI tft = TFT_eSPI();

// uint8_t output[50000L];
String base64Data;          // String buffer for base64 encoded Data
uint8_t *decodedBase64Data; // Buffer to decode base64 data

void setup()
{
    Serial.begin(115200);
    initWebServer();
    createTaskCore();

    tft.begin();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);

    if (psramFound())
    {

        Serial.printf("PSRAM Size=%ld\n", ESP.getPsramSize());

        decodedBase64Data = (uint8_t *)ps_malloc(PSRAM_BUFFER_DECODED_LENGTH);
        if (decodedBase64Data == NULL)
        {
            Serial.println("Failed to allocate memory in PSRAM for image Buffer");
            return;
        }

        if (!base64Data.reserve(PSRAM_BUFFER_READ_ENCODED_LENGTH))
        {
            Serial.println("Reserve memory failed");
            return;
        }
    }
    else
    {
        Serial.println("No PSRAM detected, needed to perform reading and decoding large base64 encoded images");
    }

    // fetchBase64Image("192.168.1.90", 80, &base64Data);
    // Serial.printf("Size of encoded data = %u\n", strlen(base64Data.c_str()));

    // size_t length = base64::decodeLength(base64Data.c_str());
    // base64::decode(base64Data.c_str(), decodedBase64Data);

    // Serial.printf("base64 decoded length = %ld\n", length);

    // displayPngFromRam(decodedBase64Data, length);

    callOpenAIAPIDalle(&base64Data);
    Serial.println(base64Data);
}

void loop()
{
    delay(1);
}

void callOpenAIAPIDalle(String *readBuffer)
{
    *readBuffer = ""; // Clear the buffer

    WiFiClientSecure client;
    client.setInsecure(); // Only for demonstration, use a proper certificate validation in production

    const char *host = "api.openai.com";
    const int httpsPort = 443;

    if (!client.connect(host, httpsPort))
    {
        Serial.println("Connection failed");
        return;
    }

    String jsonPayload = "{\"model\": \"dall-e-2\", \"prompt\": \"a star wars ship flying through stars\", \"n\": 1, \"size\": \"256x256\", \"response_format\": \"b64_json\"}";

    String request = "POST /v1/images/generations HTTP/1.1\r\n";
    request += "Host: " + String(host) + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Authorization: Bearer " + String(chatGPT_APIKey) + "\r\n";
    request += "Content-Length: " + String(jsonPayload.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += jsonPayload;

    client.print(request);

    Serial.println("Request sent");

    while (client.connected())
    {
        String line = client.readStringUntil('\n');
        // Serial.println(line);
        if (line == "\r")
        {
            Serial.println("headers received");
            break;
        }
    }

    // Read the response in chunks
    int bufferLength = BUFFER_RESPONSE_LENGTH;
    char buffer[bufferLength];
    char *startToken = "\"b64_json\": \"";
    bool base64StartFound = false, base64EndFound = false;

    // Read and process the response in chunks
    while (client.available() && !base64EndFound)
    {
        bufferLength = client.readBytes(buffer, sizeof(buffer) - 1);
        buffer[bufferLength] = '\0'; // Null-terminate the buffer
        // Serial.println(buffer);
        if (!base64StartFound)
        {
            char *base64Start = strstr(buffer, startToken);
            if (base64Start)
            {
                Serial.println("base64StartFound");
                base64StartFound = true;
                char *startOfData = base64Start + strlen(startToken); // Skip "base64,"

                // Check if the end of base64 data is in the same buffer
                char *endOfData = strchr(startOfData, '\"');
                if (endOfData)
                {
                    Serial.println("endOfData");
                    *endOfData = '\0'; // Replace the quote with a null terminator
                    *readBuffer += startOfData;
                    base64EndFound = true;
                }
                else
                {
                    *readBuffer += startOfData;
                }
            }
        }
        else
        {
            char *endOfData = strstr(buffer, "\"");
            if (endOfData)
            {
                *endOfData = '\0'; // Replace the quote with a null terminator
                *readBuffer += buffer;
                base64EndFound = true;
            }
            else
            {
                *readBuffer += buffer;
            }
        }
    }

    client.stop();
    if (!base64StartFound)
    {
        Serial.println("Error received:");
        Serial.println(buffer);
    }
    Serial.println("Request call completed");
}

void displayPngFromRam(const unsigned char *pngImageinC, size_t length)
{
    int res = png.openRAM((uint8_t *)pngImageinC, length, pngDraw);
    if (res == PNG_SUCCESS)
    {
        Serial.println("Successfully opened png file");
        Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
        Serial.printf("Image size: %d\n", length);
        Serial.printf("Buffer size: %d\n", png.getBufferSize());
        tft.startWrite();
        uint32_t dt = millis();
        res = png.decode(NULL, 0);
        if (res != PNG_SUCCESS)
        {
            printPngError(png.getLastError());
        }
        Serial.print(millis() - dt);
        Serial.println("ms");
        tft.endWrite();
        // png.close(); // not needed for memory->memory decode
    }
    else
    {
        printPngError(res);
    }
}

void createTaskCore(void)
{
    xTaskCreatePinnedToCore(
        handleBrowserCalls,   /* Function to implement the task */
        "handleBrowserCalls", /* Name of the task */
        10000,                /* Stack size in words */
        NULL,                 /* Task input parameter */
        1,                    /* Priority of the task */
        NULL,                 /* Task handle. */
        1);                   /* Core where the task should run */
}

// Task for the web browser
//
void handleBrowserCalls(void *parameter)
{
    for (;;)
    {
        server.handleClient();
        delay(1); // allow other tasks to run
    }
}

//=========================================v==========================================
//                                      pngDraw
//====================================================================================
// This next function will be called during decoding of the png file to
// render each image line to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
// Callback function to draw pixels to the display
void pngDraw(PNGDRAW *pDraw)
{
    uint16_t lineBuffer[MAX_IMAGE_WIDTH];
    png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    // png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, -1);
    tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);

    // // Print the buffer contents
    // for (int i = 0; i < MAX_IMAGE_WIDTH; i++)
    // {
    //     Serial.printf("%d", lineBuffer[i]);
    // }
    // Serial.println();
}

void printPngError(int errorCode)
{
    switch (errorCode)
    {
    case PNG_SUCCESS:
        Serial.println("PNG Success!");
        break;
    case PNG_INVALID_PARAMETER:
        Serial.println("PNG Error Invalid Parameter");
        break;
    case PNG_DECODE_ERROR:
        Serial.println("PNG Error Decode");
        break;
    case PNG_MEM_ERROR:
        Serial.println("PNG Error Memory");
        break;
    case PNG_NO_BUFFER:
        Serial.println("PNG Error No Buffer");
        break;
    case PNG_UNSUPPORTED_FEATURE:
        Serial.println("PNG Error Unsupported Feature");
        break;
    case PNG_INVALID_FILE:
        Serial.println("PNG Error Invalid File");
        break;
    case PNG_TOO_BIG:
        Serial.println("PNG Error Too Big");
        break;
    default:
        Serial.println("PNG Error Unknown");
        break;
    }
}
