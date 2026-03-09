#define BAT_PIN     34
#define R1          100000.0  // 100kΩ
#define R2          47000.0   // 47kΩ
#define ADC_REF     3.3
#define ADC_RES     4095.0
#include <Arduino.h>




float getBatteryVoltage() {
    int raw = analogRead(BAT_PIN);
    float vout = (raw / ADC_RES) * ADC_REF;
    float vbat = vout * ((R1 + R2) / R2);
    return vbat;
}

int getBatteryPercent(float voltage) {
    // Pour LiPo 2S
    if (voltage >= 8.4) return 100;
    if (voltage <= 6.0) return 0;
    return (int)((voltage - 6.0) / (8.4 - 6.0) * 100);
}