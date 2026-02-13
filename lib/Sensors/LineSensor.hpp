/*
* Implementation de la class lineSensor, detecteur de ligne planceh (contour du Tatami)
* Principe POO utilisee Erritage 
*/

#pragma once
#include "SensorsInterface.hpp"

class LineSensor: public SensorsInterface
{
private:
    bool state;
public:
    LineSensor(/* args */): state(false){}

    bool init() override{
        //// configure pin en entrée
        return true;
    }

    void update() override{

        state = true ; // <---readPin();;
    }

    float getValue () override{ return state ? 1.0f : 0.0f ;}
};

