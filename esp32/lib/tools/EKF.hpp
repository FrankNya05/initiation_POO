#include <Arduino.h>
#include <ArduinoEigen.h>

// Utilisation du namespace Eigen pour simplifier l'écriture
using namespace Eigen;

class SumoEKF {
public:
    // Définition des matrices avec tailles fixes (plus rapide sur ESP32)
    Matrix<double, 1, 1> x; // État (Position)
    Matrix<double, 1, 1> P; // Incertitude
    Matrix<double, 1, 1> Q; // Bruit de processus
    Matrix<double, 2, 2> R; // Bruit de mesure (Encodeur + IR)

    SumoEKF() {
        x.setZero();
        P << 0.1;
        Q << 0.01;
        // R : [0,0]=Var Encodeur, [1,1]=Var IR
        R << 0.5, 0.0,
             0.0, 2.0;
    }

    void predict() {
        // Modèle f(x) = x -> F = 1
        // P = F*P*F' + Q
        P = P + Q;
    }

    void update(double z_enc, double z_ir) {
        // h(x) = [x, x]^T -> H = [1, 1]^T
        Matrix<double, 2, 1> z;
        z << z_enc, z_ir;

        Matrix<double, 2, 1> H;
        H << 1.0, 1.0;

        // Innovation
        Matrix<double, 2, 1> y = z - (H * x);
        
        // S = H*P*H' + R
        Matrix<double, 2, 2> S = H * P * H.transpose() + R;
        
        // Gain de Kalman K = P*H'*S^-1
        Matrix<double, 1, 2> K = P * H.transpose() * S.inverse();

        // Mise à jour
        x = x + K * y;
        P = (Matrix<double, 1, 1>::Identity() - K * H) * P;
    }
};

SumoEKF ekf;

void setup() {
    Serial.begin(115200);
    while(!Serial); // Attend l'ouverture du port série
    Serial.println("--- Robot Sumo EKF Prêt ---");
}

void loop() {
    static unsigned long lastUpdate = 0;
    const unsigned long interval = 50; // 50ms (20Hz)

    if (millis() - lastUpdate >= interval) {
        lastUpdate = millis();

        // --- SECTION À REMPLACER PAR TES VRAIES LECTURES ---
        // Exemple fictif :
        double lecture_encodeur = 10.0 + (random(-5, 5) / 10.0); 
        double lecture_ir = 10.0 + (random(-10, 10) / 10.0);
        // --------------------------------------------------

        // 1. Prédiction
        ekf.predict();

        // 2. Mise à jour avec les capteurs
        ekf.update(lecture_encodeur, lecture_ir);

        // 3. Affichage pour le Serial Plotter (VS Code ou Arduino)
        // Format : >Nom:Valeur
        Serial.print(">Encodeur:"); Serial.println(lecture_encodeur);
        Serial.print(">IR:"); Serial.println(lecture_ir);
        Serial.print(">Kalman_Fusé:"); Serial.println(ekf.x(0));
    }
}