#include "Encoder.hpp"

// Définition des membres statiques
Encoder* Encoder::_instanceLeft  = nullptr;
Encoder* Encoder::_instanceRight = nullptr;

// ISR définies dans .cpp pour que le linker Xtensa place correctement
// le pool de littéraux AVANT les instructions IRAM_ATTR
void IRAM_ATTR Encoder::_isrLeft() {
    if (_instanceLeft) _instanceLeft->_handleISR();
}

void IRAM_ATTR Encoder::_isrRight() {
    if (_instanceRight) _instanceRight->_handleISR();
}
