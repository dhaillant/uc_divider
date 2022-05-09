
// Reading an array of buttons, by L.Geerinckx 6-12-2021 6-12-2021


#ifndef MYBUTTONS_H
#define MYBUTTONS_H

enum button
{
    PRESSED,
    RELEASED,
};


class MyButtons
{
  private:
    byte _num;
    byte *_btnPins;
    byte *lastButtonState;
    byte *buttonState;
    void (*_handler)(byte, button);
    unsigned long lastDebounceTime;
    unsigned long debounceDelay;

  public:
    MyButtons(byte arr[],byte, void handler(byte,button), byte); // constructor
    byte *reading;
    void begin();
    bool on();
};


MyButtons::MyButtons(byte btnPins[], byte num, void handler(byte, button), byte debounce=0)
{
    _num = num;
    _handler = handler;
    debounceDelay = debounce;
    _btnPins=new byte[_num];
    lastButtonState = new byte[_num];
    buttonState = new byte[_num];
    lastDebounceTime = 0;
    reading = new byte[_num];
  
    // populate arrays
    for (byte i = 0; i < _num; ++i)
    {
        _btnPins[i] = btnPins[i];
        lastButtonState[i] = 1;
        buttonState[i]=1;
    }
}

void MyButtons::begin()
{
    for (byte i = 0; i < _num; i++)
    {
        pinMode(_btnPins[i], INPUT_PULLUP);
    }
}

bool MyButtons::on()
{
    for (int i = 0; i < _num; i++)
    {
        reading[i] = !digitalRead(_btnPins[i]);  // inverted
        if (reading[i] != lastButtonState[i]) // if change
        {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay) // check again after debounce time
        {
            if (reading[i] != buttonState[i])
            {
                if (reading[i] == 1)
                {
                    _handler(i, PRESSED);
                }
                if (reading[i] == 0)
                {
                    _handler(i, RELEASED);
                }
                buttonState[i] = reading[i];
                return true;
            }
        }
        lastButtonState[i] = reading[i];
    }
    return false;
}

#endif
