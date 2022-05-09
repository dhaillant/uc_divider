#include "MyDivision.h"
#include "MyButtons.h"

// Code By L.Geerinckx 6-12-2021

#define outNum 8

MyDivision out_1(4, 6);    // pin, division
MyDivision out_2(5, 12);    
MyDivision out_3(6, 24);   
MyDivision out_4(7, 48);   
MyDivision out_5(8, 96);   
MyDivision out_6(9, 192);  
MyDivision out_7(10, 384); 
MyDivision out_8(11, 768); 

MyDivision *outPuts[outNum] = {&out_1, &out_2, &out_3, &out_4, &out_5, &out_6, &out_7, &out_8};

void triggerHandler(byte index, button type)
{
    if (type == PRESSED && index == 0)// clock
    {
        for (byte i = 0; i < outNum; i++)
        {
            outPuts[i]->next();
        }
    }
    else if (type == PRESSED && index == 1) // reset
    {
        for (byte i = 0; i < outNum; i++)
        {
            outPuts[i]->reset();
        }
    }
}

byte triggerPins[2] = {2, 3}; // clock, reset
MyButtons triggerInputs(triggerPins, 2, triggerHandler);  // pin number array, number of buttons, handler function

void setup()
{
    triggerInputs.begin();
    for (byte i = 0; i < outNum; i++)
    {
        outPuts[i]->begin();
    }
}

void loop()
{
    triggerInputs.on();
}
