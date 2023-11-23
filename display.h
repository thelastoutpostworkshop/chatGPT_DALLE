#define STORED_IMAGES_LENGTH 250000L // Max size of image storage

class Display
{
public:
    int csPin;            // Chip Select pin
    uint8_t *storedImage; // Pointer to the stored image
    bool hasImage;        // Flag indicating if the display has an image
    bool hasImageStorage; // Flag indicating if memory has been allocated to store an image

    // Constructor
    Display(int pin)
        : csPin(pin), storedImage(nullptr), hasImage(false), hasImageStorage(false)
    {
        storedImage = (uint8_t *)ps_malloc(STORED_IMAGES_LENGTH);
        if (storedImage != NULL)
        {
            hasImageStorage = true;
        }
    }

    void storeImage(uint8_t *image, size_t length)
    {
        if (length > STORED_IMAGES_LENGTH)
        {
            Serial.printf("!!! Cannot store image, too large=%u\n", length);
        }
        else
        {
            memcpy(storedImage, image, length);
        }
    }
};