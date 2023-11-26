#include <PNGdec.h>      // Install this library with the Arduino IDE Library Manager
#include <TFT_eSPI.h>    // Install this library with the Arduino IDE Library Manager
                         // Don't forget to configure the driver for your display!
#include <AnimatedGIF.h> // Install this library with the Arduino IDE Library Manager
#include <SD.h>
#include "WebServer.h"
#include <WiFiClientSecure.h>
#include "arduino_base64.hpp"
#include "display.h"
#include "switch.h"
#include "images\ai.h"       // AI Animated GIF
#include "images\readyPng.h" // Ready PNG
#include "images\readyAnimation.h"

// #define SIMULE_CALL_DALLE // Test images will be used, uncomment this line to make the real call to the DALLE API
#define DEBUG_ON       // Comment this line if you don't want detailed messages on the serial monitor, all errors will be printed

#ifndef SIMULE_CALL_DALLE
#define USE_SD_CARD // Comment this line if you don't have an SD Card module
#endif

#ifdef DEBUG_ON
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

#ifdef SIMULE_CALL_DALLE
#include "base64_test_images\mandalaBase64Png.h"
#include "base64_test_images\rainbowBase64Png.h"
#include "base64_test_images\resistance.h"
#include "base64_test_images\spaceship.h"
#include "base64_test_images\hal.h"

// Test PNG Images
const char *testPngImages[] = {mandalaBase64Png, resistance64Png, spaceship64Png, hal64Png};
const int testImagesCount = 4;
#endif

#ifdef USE_SD_CARD
#define SD_CARD_CS_PIN 9 // Chip Select Pin for the SD Card Module
const char *ID_FILENAME = "id.txt";
const char *IMAGES_FOLDER_NAME = "/images";
int idForNewFile = 1;
#endif

#define ANIMATED_AI_IMAGE ai
AnimatedGIF gif;
TaskHandle_t taskHandlePlayGif = NULL;
void GIFDraw(GIFDRAW *pDraw);

// Switch
SwitchReader generationSwitch(1);
bool runImageGeneration = false; // Flag to indicate if generation of images is running or stopped

#define MAX_IMAGE_WIDTH 1024                      // Adjust for your images
#define PSRAM_BUFFER_DECODED_LENGTH 4000000L      // Length of buffer for base64 data decoding in PSRAM
#define PSRAM_BUFFER_READ_ENCODED_LENGTH 2000000L // Length of buffer for reading the base64 encoded data in PSRAM
#define BUFFER_RESPONSE_LENGTH 1024               // Length of buffer for reading api response in chunks

// Delimiters to extract base64 data in Json
const char *startToken = "\"b64_json\": \"";
const char *endToken = "\"";

// Prompts
const int promptsCount = 10;
char *prompts[promptsCount] = {"An alien planet with ships orbiting", "A star wars spaceship", "A star wars vessel cockpit view in space",
                               "An empire spaceship attacking", "The interior of a spaceship", "Control Panels of an Alien spaceship",
                               "A futurisctic HUD screen", "A futuristic City", "A star wars vessel docked on a spaceport",
                               "An Empire ship being repaired"};

int16_t xpos = 0;
int16_t ypos = 0;

PNG png; // PNG decoder instance

// Display - You must disable chip select pin definitions in the user_setup.h and the driver setup (ex.: Setup46_GC9A01_ESP32.h)
const int NUM_DISPLAYS = 4; // Adjust this value based on the number of displays
Display display[NUM_DISPLAYS] = {
    Display(15), // Assign a chip select pin for each display
    Display(7),
    Display(6),
    Display(16)};

TFT_eSPI tft = TFT_eSPI();

// uint8_t output[50000L];
String base64Data;          // String buffer for base64 encoded Data
uint8_t *decodedBase64Data; // Buffer to decode base64 data

void setup()
{
    Serial.begin(115200);
    initWebServer();
    createTaskCore();
    gif.begin(BIG_ENDIAN_PIXELS);

    if (!allocatePsramMemory())
    {
        while (true)
        {
            // Infinite loop, code execution useless without PSRAM
        }
    }

    if (!initDisplay())
    {
        Serial.println("!!! Cannot allocate enough PSRAM to store images");
        while (true)
        {
            // Infinite loop, code execution useless without PSRAM
        }
    }

#ifdef USE_SD_CARD
    if (!initSDCard())
    {
        while (true)
        {
            // Infinite loop, code execution useless without PSRAM
        }
    }
    idForNewFile = readNextId(SD) + 1;
    DEBUG_PRINTF("ID for the next file is %d\n", idForNewFile);
    createDir(SD, IMAGES_FOLDER_NAME);

#endif
}

void loop()
{
    if (runImageGeneration)
    {
        generateAIImages();
        if(!runImageGeneration) {
            playReadyOnScreens();
        }
    }
}

void playAIGif(void *parameter)
{
    for (;;)
    {
        gif.playFrame(true, NULL);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void startPlayAIGifAsync(void)
{
    gif.open((uint8_t *)ANIMATED_AI_IMAGE, sizeof(ANIMATED_AI_IMAGE), GIFDraw);
    display[0].activate();
    tft.startWrite();
    if (taskHandlePlayGif == NULL)
    {
        taskHandlePlayGif = playAIGifTask();
    }
}

void stopPlayAIGifAsync(void)
{
    if (taskHandlePlayGif != NULL)
    {
        vTaskDelete(taskHandlePlayGif);
        taskHandlePlayGif = NULL; // Set to NULL to indicate that the task is no longer running
    }
    gif.close();
    tft.endWrite();
    display[0].deActivate();
}

void playAnimatedGIFSync(uint8_t *image, size_t imageSize, int screenIndex)
{
    if (!verifyScreenIndex(screenIndex))
    {
        Serial.printf("Screen index out of bound = %d", screenIndex);
    }
    gif.open(image, imageSize, GIFDraw);
    display[screenIndex].activate();
    tft.startWrite();
    while (gif.playFrame(true, NULL))
    {
        yield();
    }
    gif.close();
    tft.endWrite();
    display[screenIndex].deActivate();
}

TaskHandle_t playAIGifTask()
{
    TaskHandle_t taskHandle = NULL;
    xTaskCreate(
        playAIGif,   // Task function
        "playAIGif", // Name of task
        2048,        // Stack size of task (adjust as needed)
        NULL,        // Parameter of the task
        1,           // Priority of the task
        &taskHandle  // Task handle
    );
    return taskHandle;
}

#ifdef USE_SD_CARD
bool initSDCard(void)
{
    // Make sure SPI_FREQUENCY is 20000000 in your TFT_eSPI driver for your display
    // Initialize SD card
    if (!SD.begin(SD_CARD_CS_PIN))
    {
        // You can get this error if no Micro SD card is inserted into the module
        Serial.println("!!! SD Card initialization failed!");
        return false;
    }
    else
    {
        Serial.println("SD Card initialization success");
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("!!! No SD card attached");
        return false;
    }

    DEBUG_PRINTLN("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        DEBUG_PRINTLN("MMC");
    }
    else if (cardType == CARD_SD)
    {
        DEBUG_PRINTLN("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        DEBUG_PRINTLN("SDHC");
    }
    else
    {
        DEBUG_PRINTLN("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    DEBUG_PRINTF("SD Card Size: %lluMB\n", cardSize);
    return true;
}

int readNextId(fs::FS &fs)
{
    DEBUG_PRINTLN("Reading next ID");
    String idFilename = ID_FILENAME;
    File file = fs.open("/" + idFilename);
    if (!file)
    {
        Serial.println("!!! Failed to open ID file for reading");
        return 0;
    }

    String fileContent = "";
    while (file.available())
    {
        fileContent += (char)file.read();
    }
    file.close();

    return fileContent.toInt(); // Convert string to integer and return
}

void writeNextId(fs::FS &fs, int id)
{
    DEBUG_PRINTLN("Writing next ID");
    String idFilename = ID_FILENAME;

    File file = fs.open("/" + idFilename, FILE_WRITE);
    if (!file)
    {
        Serial.println("!!! Failed to open file for writing");
        return;
    }

    // Convert the integer to a string and write it to the file
    file.print(id);

    file.close();
    DEBUG_PRINTLN("ID written successfully");
}

void writeImage(fs::FS &fs, const char *path, uint8_t *image, size_t length)
{
    DEBUG_PRINTF("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("!!! Failed to open file for writing");
        return;
    }
    if (file.write(image, length))
    {
        DEBUG_PRINTLN("File written");
    }
    else
    {
        Serial.println("!!! Write failed");
    }
    file.close();
}

void createDir(fs::FS &fs, const char *path)
{
    DEBUG_PRINTF("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        DEBUG_PRINTLN("Dir created");
    }
    else
    {
        Serial.println("!!! mkdir failed");
    }
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.path(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
#endif

void generationSwitchTask(void *parameter)
{
    for (;;)
    { // Task loop
        if (generationSwitch.read())
        {
            runImageGeneration = !runImageGeneration;
            if (runImageGeneration)
            {
                Serial.println("Image generation starting...");
            }
            else
            {
                Serial.println("Image generation will be stopped");
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS); // Delay to prevent task from running too frequently
    }
}

void generateAIImages(void)
{
#ifdef SIMULE_CALL_DALLE
    startPlayAIGifAsync();
    unsigned long t = millis();
    while (millis() - t < 5000)
    {
    }
    stopPlayAIGifAsync();
    // playAnimatedGIFSync((uint8_t*)transition,sizeof(transition));
    const char *image = testPngImages[myRandom(testImagesCount)];
    size_t length = displayPngImage(image, 0);
    display[0].storeImage(decodedBase64Data, length);
    delay(5000); // Delay for simulation
    shifImagesOnDisplayLeft();
#else
    size_t length = generateDalleImageRandomPrompt();
    display[0].storeImage(decodedBase64Data, length);
#ifdef USE_SD_CARD
    String filename = String(IMAGES_FOLDER_NAME) + "/" + String(idForNewFile) + ".png";
    idForNewFile += 1;
    writeImage(SD, filename.c_str(), decodedBase64Data, length);
    writeNextId(SD, idForNewFile);
#endif
    delay(5000);
    shifImagesOnDisplayLeft();
#endif
}

void shifImagesOnDisplayLeft(void)
{
    for (int i = NUM_DISPLAYS - 1; i >= 0; i--)
    {
        int displaySource = i - 1;
        if (i == 0)
        {
            return;
        }
        switchImageOnDisplay(displaySource, i);
    }

    // for (int i = 0; i < NUM_DISPLAYS; i++)
    // {
    //     int nextDisplay = i + 1;
    //     if (nextDisplay < NUM_DISPLAYS)
    //     {
    //         shiftImageOnDisplay(i, nextDisplay);
    //     }
    // }
}

void switchImageOnDisplay(int sourceDisplay, int destinationDisplay)
{
    if (sourceDisplay == destinationDisplay)
    {
        Serial.printf("!!! Source and destination display are the same (%d)\n", sourceDisplay);
        return;
    }

    if (!verifyScreenIndex(sourceDisplay))
    {
        Serial.printf("!!! Source display is out of bound (%d)\n", sourceDisplay);
        return;
    }

    if (!verifyScreenIndex(destinationDisplay))
    {
        Serial.printf("!!! Destination display is out of bound (%d)\n", destinationDisplay);
        return;
    }

    if (display[sourceDisplay].imageSize() > 0)
    {
        display[destinationDisplay].storeImage(display[sourceDisplay].image(), display[sourceDisplay].imageSize());
        displayPngFromRam(display[destinationDisplay].image(), display[destinationDisplay].imageSize(), destinationDisplay);
        display[sourceDisplay].activate();
        tft.fillScreen(TFT_BLACK);
        display[sourceDisplay].deActivate();
    }
    else
    {
        display[destinationDisplay].activate();
        tft.fillScreen(TFT_BLACK);
        display[destinationDisplay].deActivate();
    }
}

bool verifyScreenIndex(int screenIndex)
{
    if (screenIndex < 0 || screenIndex >= NUM_DISPLAYS)
    {
        return false;
    }
    return true;
}

bool initDisplay(void)
{
    tft.init(BIG_ENDIAN_PIXELS);
    tft.setFreeFont(&FreeMono24pt7b);
    for (int i = 0; i < NUM_DISPLAYS; i++)
    {
        display[i].activate();
        tft.fillScreen(TFT_BLACK);
        tft.setRotation(2); // Adjust Rotation of your screen (0-3)
        display[i].deActivate();
    }
    for (int i = 0; i < NUM_DISPLAYS; i++)
    {
        if (!display[i].reserveMemoryForStorage())
        {
            return false;
        }
    }
    delay(5000);
    playReadyOnScreens();
    return true;
}

void playReadyOnScreens(void)
{
    for (int i = 0; i < NUM_DISPLAYS; i++)
    {
        playAnimatedGIFSync((uint8_t *)readyAnimation, sizeof(readyAnimation), i);
        size_t length = base64::decodeLength(ready64Png);
        base64::decode(ready64Png, decodedBase64Data);
        display[i].storeImage(decodedBase64Data, length);
    }
}

size_t generateDalleImageRandomPrompt(void)
{
    char *randomPrompt = prompts[myRandom(promptsCount)];
    return genereteDalleImage(randomPrompt);
}

size_t genereteDalleImage(char *prompt)
{
    callOpenAIAPIDalle(&base64Data, prompt);
    size_t length = displayPngImage(base64Data.c_str(), 0);

    return length;
}

void callOpenAIAPIDalle(String *readBuffer, const char *prompt)
{
    startPlayAIGifAsync();
    *readBuffer = ""; // Clear the buffer

    WiFiClientSecure client;
    client.setInsecure(); // Only for demonstration, use a proper certificate validation in production

    const char *host = "api.openai.com";
    const int httpsPort = 443;

    if (!client.connect(host, httpsPort))
    {
        // playAIGif();
        Serial.println("!!! Connection failed");
        stopPlayAIGifAsync();
        return;
    }

    String jsonPayload = "{\"model\": \"dall-e-2\", \"prompt\": \"";
    jsonPayload += prompt;
    jsonPayload += "\", \"n\": 1, \"size\": \"256x256\", \"response_format\": \"b64_json\"}";

    String request = "POST /v1/images/generations HTTP/1.1\r\n";
    request += "Host: " + String(host) + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Authorization: Bearer " + String(chatGPT_APIKey) + "\r\n";
    request += "Content-Length: " + String(jsonPayload.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += jsonPayload;

    client.print(request);

    DEBUG_PRINTF("Request sent with prompt : %s\n", prompt);

    while (client.connected())
    {
        String line = client.readStringUntil('\n');
        if (line == "\r")
        {
            DEBUG_PRINTLN("headers received");
            break;
        }
    }

    // Read the response in chunks
    int bufferLength = BUFFER_RESPONSE_LENGTH;
    char buffer[bufferLength];
    bool base64StartFound = false, base64EndFound = false;

    // Read and process the response in chunks
    while (client.available() && !base64EndFound)
    {
        // playAIGif();
        bufferLength = client.readBytes(buffer, sizeof(buffer) - 1);
        buffer[bufferLength] = '\0';
        if (!base64StartFound)
        {
            char *base64Start = strstr(buffer, startToken);
            if (base64Start)
            {
                base64StartFound = true;
                char *startOfData = base64Start + strlen(startToken); // Skip "base64,"

                // Check if the end of base64 data is in the same buffer
                char *endOfData = strstr(startOfData, endToken);
                if (endOfData)
                {
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
            char *endOfData = strstr(buffer, endToken);
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
        Serial.println("!!! No Json Base64 data in Response:");
        Serial.println(buffer);
    }
    DEBUG_PRINTLN("Request call completed");
    stopPlayAIGifAsync();
}

void displayPngFromRam(const unsigned char *pngImageinC, size_t length, int screenIndex)
{
    if (!verifyScreenIndex(screenIndex))
    {
        Serial.printf("!!! displayPngFromRam Screen index is out of bound (%d)\n", screenIndex);
        return;
    }
    int res = png.openRAM((uint8_t *)pngImageinC, length, pngDraw);
    if (res == PNG_SUCCESS)
    {
        DEBUG_PRINTLN("Successfully opened png file");
        DEBUG_PRINTF("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
        DEBUG_PRINTF("Image size: %d\n", length);
        DEBUG_PRINTF("Buffer size: %d\n", png.getBufferSize());
        display[screenIndex].activate();
        tft.startWrite();
        uint32_t dt = millis();
        res = png.decode(NULL, 0);
        if (res != PNG_SUCCESS)
        {
            printPngError(png.getLastError());
        }
        DEBUG_PRINT(millis() - dt);
        DEBUG_PRINTLN("ms");

        tft.endWrite();
        display[screenIndex].deActivate();

        // png.close(); // not needed for memory->memory decode
    }
    else
    {
        printPngError(res);
    }
}

// Memory allocation in PSRAM
bool allocatePsramMemory(void)
{
    if (psramFound())
    {

        DEBUG_PRINTF("PSRAM Size=%ld\n", ESP.getPsramSize());

        decodedBase64Data = (uint8_t *)ps_malloc(PSRAM_BUFFER_DECODED_LENGTH);
        if (decodedBase64Data == NULL)
        {
            Serial.println("!!! Failed to allocate memory in PSRAM for image Buffer");
            return false;
        }

        if (!base64Data.reserve(PSRAM_BUFFER_READ_ENCODED_LENGTH))
        {
            Serial.println("!!! Failed to allocate memory in PSRAM for deconding base64 Data");
            return false;
        }
    }
    else
    {
        Serial.println("!!! No PSRAM detected, needed to perform reading and decoding large base64 encoded images");
        return false;
    }
    return true;
}

void createTaskCore(void)
{
    /* Core where the task should run */
    xTaskCreatePinnedToCore(
        generationSwitchTask,   /* Function to implement the task */
        "generationSwitchTask", /* Name of the task */
        10000,                  /* Stack size in words */
        NULL,                   /* Task input parameter */
        1,                      /* Priority of the task */
        NULL,                   /* Task handle. */
        1);                     /* Core where the task should run */
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
}

void printPngError(int errorCode)
{
    switch (errorCode)
    {
    case PNG_SUCCESS:
        Serial.println("!!! PNG Success!");
        break;
    case PNG_INVALID_PARAMETER:
        Serial.println("!!! PNG Error Invalid Parameter");
        break;
    case PNG_DECODE_ERROR:
        Serial.println("!!! PNG Error Decode");
        break;
    case PNG_MEM_ERROR:
        Serial.println("!!! PNG Error Memory");
        break;
    case PNG_NO_BUFFER:
        Serial.println("!!! PNG Error No Buffer");
        break;
    case PNG_UNSUPPORTED_FEATURE:
        Serial.println("!!! PNG Error Unsupported Feature");
        break;
    case PNG_INVALID_FILE:
        Serial.println("!!! PNG Error Invalid File");
        break;
    case PNG_TOO_BIG:
        Serial.println("!!! PNG Error Too Big");
        break;
    default:
        Serial.println("!!! PNG Error Unknown");
        break;
    }
}

size_t displayPngImage(const char *imageBase64Png, int displayIndex)
{
    size_t length = base64::decodeLength(imageBase64Png);
    base64::decode(imageBase64Png, decodedBase64Data);

    DEBUG_PRINTF("base64 encoded length = %ld\n", strlen(imageBase64Png));
    DEBUG_PRINTF("base64 decoded length = %ld\n", length);

    displayPngFromRam(decodedBase64Data, length, displayIndex);
    return length;
}

long myRandom(long howbig)
{
    if (howbig == 0)
    {
        return 0;
    }
    return esp_random() % howbig;
}

long myRandom(long howsmall, long howbig)
{
    if (howsmall >= howbig)
    {
        return howsmall;
    }
    long diff = howbig - howsmall;
    return esp_random() % diff + howsmall;
}
