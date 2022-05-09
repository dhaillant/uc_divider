#ifndef MYDIVISION_H
#define MYDIVISION_H

// A simple clock division utility by L.Geerinckx 6-12-2021

class MyDivision
{
private:
    int count;
    int current;
    int pin;
    int halfCount;

public:
    MyDivision(int, int);
    void next();
    void begin();
    void reset();
};

MyDivision::MyDivision(int _pin, int _count)
{
    count = _count;
    halfCount = count / 2;
    pin = _pin;
    current = -1;
}

void MyDivision::begin()
{
    pinMode(pin, OUTPUT);
    reset();
}

void MyDivision::reset()
{
    digitalWrite(pin, HIGH);
    delay(2);
    digitalWrite(pin, LOW);
    current = -1;
}

void MyDivision::next()
{
    current++;
    if (current == halfCount)
    {
        digitalWrite(pin, LOW);
    }
    else if (current == count)
    {
        current = 0;
        digitalWrite(pin, HIGH);
    }
}

#endif
