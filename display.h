#define STORED_IMAGES_LENGTH 250000L // Max size of image storage

class Display
{
public:
    int csPin;            // Chip Select pin
    uint8_t *storedImage; // Pointer to the stored image
    bool hasImage;        // Flag indicating if the display has an image

    // Constructor
    Display(int pin)
        : csPin(pin), storedImage(NULL), hasImage(false)
    {
    }

    bool reserveMemoryForStorage(void)
    {
        storedImage = (uint8_t *)ps_malloc(STORED_IMAGES_LENGTH);
        return storedImage != NULL;
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
    void activate(void)
    {
        digitalWrite(csPin, LOW);
    }
    void deActivate(void)
    {
        digitalWrite(csPin, HIGH);
    }

    void setStoredImage(uint8_t* image) {
        storedImage = image;
    }

    uint8_t* getStoredImage() const {
        return storedImage;
    }
};