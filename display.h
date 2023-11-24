#define STORED_IMAGES_LENGTH 250000L // Max size of image storage

class Display
{
public:
    int csPin;            // Chip Select pin
    uint8_t *storedImage; // Pointer to the stored image
    size_t size;        // Size of image, length = 0 indicate no image


    // Constructor
    Display(int pin)
        : csPin(pin), storedImage(NULL), size(0)
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
            size = 0;
        }
        else
        {
            memcpy(storedImage, image, length);
            size = length;
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

    size_t imageSize() const {
        return size;
    }

    uint8_t* storedImage() const {
        return storedImage;
    }
};