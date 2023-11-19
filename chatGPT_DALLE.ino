#include <PNGdec.h>
#include <TFT_eSPI.h>
#include "WebServer.h"
#include "arduino_base64.hpp"

#include "mandalaBase64Png.h"

#define MAX_IMAGE_WIDTH 240 // Adjust for your images

int16_t xpos = 0;
int16_t ypos = 0;

PNG png; // PNG decoder instance
TFT_eSPI tft = TFT_eSPI();

uint8_t output[50000L];
// uint8_t* output;

void setup()
{
    Serial.begin(115200);
    initWebServer();
    createTaskCore();

    tft.begin();
    // tft.setRotation(2);
    tft.fillScreen(TFT_WHITE);

    // output = (uint8_t*) ps_malloc(50000L);

    // if (output == NULL) {
    //     Serial.println("Failed to allocate memory in PSRAM");
    //     return;
    // }
    
    size_t length = base64::decodeLength(mandalaBase64Png);
    base64::decode(mandalaBase64Png, output);

    Serial.printf("base64 decoded length = %ld\n", length);

    // displayPngFromRam(panda,sizeof(panda));
    displayPngFromRam(output, length);
    // Serial.println((const char *)output);
}

void loop()
{
    delay(1);
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
