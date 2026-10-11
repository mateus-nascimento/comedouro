#include <Servo.h>

// Arduino Nano: sinal do servo no pino D10.
// Envie um numero inteiro pelo Monitor Serial (uma linha por comando).
const byte SERVO_PIN = 10;
const byte STATUS_LED_PIN = LED_BUILTIN;
const int ANGULO_INICIAL = 90;
const unsigned long BAUD_RATE = 9600;
const unsigned long LED_INTERVAL_MS = 2000UL;

Servo servo;
char entrada[8];
byte tamanhoEntrada = 0;
unsigned long ultimaAlternanciaLed = 0;
bool ledLigado = false;

void setup()
{
    Serial.begin(BAUD_RATE);

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    servo.attach(SERVO_PIN);
    servo.write(ANGULO_INICIAL);

    Serial.println(F("Teste de servo iniciado. Envie um angulo inteiro."));
    Serial.println(ANGULO_INICIAL);
}

void loop()
{
    atualizarLed();

    while (Serial.available() > 0)
    {
        const char caractere = Serial.read();

        if (caractere == '\n' || caractere == '\r')
        {
            if (tamanhoEntrada > 0)
            {
                processarEntrada();
            }
        }
        else if (caractere >= '0' && caractere <= '9')
        {
            if (tamanhoEntrada < sizeof(entrada) - 1)
            {
                entrada[tamanhoEntrada++] = caractere;
            }
            else
            {
                tamanhoEntrada = 0;
                Serial.println(F("Entrada muito longa."));
            }
        }
        else if (caractere == '-' && tamanhoEntrada == 0)
        {
            entrada[tamanhoEntrada++] = caractere;
        }
        else if (caractere != ' ' && caractere != '\t')
        {
            tamanhoEntrada = 0;
            Serial.println(F("Entrada invalida. Envie um numero inteiro."));
        }
    }
}

void atualizarLed()
{
    const unsigned long agora = millis();
    if (agora - ultimaAlternanciaLed >= LED_INTERVAL_MS)
    {
        ultimaAlternanciaLed = agora;
        ledLigado = !ledLigado;
        digitalWrite(STATUS_LED_PIN, ledLigado ? HIGH : LOW);
    }
}

void processarEntrada()
{
    entrada[tamanhoEntrada] = '\0';
    const int angulo = atoi(entrada);
    tamanhoEntrada = 0;

    servo.write(angulo);
    Serial.print(F("Servo ajustado para "));
    Serial.print(angulo);
    Serial.println(F(" graus."));
}
