class SwitchReader {
public:
    SwitchReader(int pin, unsigned long debounceDelay = 50) 
    : _pin(pin), _debounceDelay(debounceDelay), _lastState(HIGH), _lastDebounceTime(0) {
        pinMode(_pin, INPUT_PULLUP);
    }

    bool read() {
        int currentState = digitalRead(_pin);
        if (currentState != _lastState) {
            _lastDebounceTime = millis();
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay) {
            if (currentState != _state) {
                _state = currentState;
                _lastState = currentState;
                return _state == LOW; // Returns true if the button is pressed
            }
        }

        _lastState = currentState;
        return false; // No change in state
    }

private:
    int _pin;
    unsigned long _debounceDelay;
    int _state;
    int _lastState;
    unsigned long _lastDebounceTime;
};
