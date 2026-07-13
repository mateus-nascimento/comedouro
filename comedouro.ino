#include <SoftwareSerial.h>
#include <Servo.h>
#include <stdint.h>
#include <string.h>

// =====================================================
// Configuração de Hardware
// =====================================================
constexpr byte RFID_RX_PIN = 2;
constexpr byte RFID_TX_PIN = 3;      // Não utilizado, exigido pelo SoftwareSerial
constexpr byte SERVO_PIN = 10;
constexpr byte STATUS_LED_PIN = LED_BUILTIN;

// =====================================================
// Configuração do Servo
// =====================================================
constexpr int SERVO_OPEN_ANGLE = 90;
constexpr int SERVO_CLOSED_ANGLE = 180;

constexpr int SERVO_STEP = 2;
constexpr unsigned long SERVO_STEP_INTERVAL_MS = 20;

// =====================================================
// Configuração RFID
// =====================================================
constexpr unsigned long DEBOUNCE_MS = 3000UL;

constexpr uint8_t START_BYTE = 0xAA;
constexpr uint8_t END_BYTE = 0xBB;

constexpr size_t MAX_PACKET_SIZE = 64;
constexpr unsigned long BYTE_TIMEOUT_MS = 200;

constexpr int NATIONAL_DEC_WIDTH = 12;

// =====================================================
// Tags cadastradas
// =====================================================
constexpr char TAG_LILITH[] = "900200000014611";
constexpr char TAG_SNOW[]   = "900200000014659";
constexpr char TAG_SOPHIE[] = "900200000014691";

// =====================================================
// Objetos
// =====================================================
SoftwareSerial rfidSerial(RFID_RX_PIN, RFID_TX_PIN);
Servo doorServo;

// =====================================================
// Estado da Porta
// =====================================================
bool doorOpen = false;

// =====================================================
// Estado do Servo
// =====================================================
int currentServoAngle = SERVO_CLOSED_ANGLE;
int targetServoAngle = SERVO_CLOSED_ANGLE;

unsigned long lastServoUpdateTime = 0;

// =====================================================
// Controle de Debounce RFID
// =====================================================
char lastProcessedTag[32] = "";
unsigned long lastProcessedTime = 0;

// =====================================================
// Leitura não-bloqueante do pacote RFID
// =====================================================
enum class PacketReadState
{
    Idle,
    Reading
};

struct RfidReaderState
{
    PacketReadState state = PacketReadState::Idle;
    uint8_t buffer[MAX_PACKET_SIZE];
    size_t length = 0;
    unsigned long lastByteTime = 0;
};

RfidReaderState rfidReaderState;

// =====================================================

void setup()
{
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    rfidSerial.begin(9600);

    doorServo.attach(SERVO_PIN);
    doorServo.write(currentServoAngle);
}

void loop()
{
    updateServoPosition();
    pollRfid();
}

// =====================================================
// RFID
// =====================================================

void pollRfid()
{
    uint8_t packet[MAX_PACKET_SIZE];
    size_t length = 0;

    if (!pollPacketReader(rfidReaderState, packet, length))
    {
        return;
    }

    char tag[32];

    if (!extractTag(packet, length, tag, sizeof(tag)))
    {
        return;
    }

    if (isDebounced(tag))
    {
        return;
    }

    rememberProcessedTag(tag);
    handleDetectedTag(tag);
}

void handleDetectedTag(const char *tag)
{
    digitalWrite(STATUS_LED_PIN, HIGH);

    if (strcmp(tag, TAG_SNOW) == 0)
    {
        if (!isDoorAtOpenPosition())
        {
            requestDoorOpen();
        }
        else
        {
            doorOpen = true;
        }
    }
    else if (!isDoorAtClosedPosition())
    {
        requestDoorClose();
    }
    else
    {
        doorOpen = false;
    }

    digitalWrite(STATUS_LED_PIN, LOW);
}

// =====================================================
// Controle da Porta
// =====================================================

bool isDoorAtOpenPosition()
{
    return currentServoAngle == SERVO_OPEN_ANGLE
        && targetServoAngle == SERVO_OPEN_ANGLE;
}

bool isDoorAtClosedPosition()
{
    return currentServoAngle == SERVO_CLOSED_ANGLE
        && targetServoAngle == SERVO_CLOSED_ANGLE;
}

void requestDoorOpen()
{
    if (isDoorAtOpenPosition())
    {
        doorOpen = true;
        return;
    }

    if (doorOpen && targetServoAngle == SERVO_OPEN_ANGLE)
    {
        return;
    }

    targetServoAngle = SERVO_OPEN_ANGLE;
    doorOpen = true;
}

void requestDoorClose()
{
    if (isDoorAtClosedPosition())
    {
        doorOpen = false;
        return;
    }

    if (!doorOpen && targetServoAngle == SERVO_CLOSED_ANGLE)
    {
        return;
    }

    targetServoAngle = SERVO_CLOSED_ANGLE;
    doorOpen = false;
}

// =====================================================
// Movimento Suave do Servo
// =====================================================

void updateServoPosition()
{
    if (currentServoAngle == targetServoAngle)
    {
        return;
    }

    if (millis() - lastServoUpdateTime < SERVO_STEP_INTERVAL_MS)
    {
        return;
    }

    lastServoUpdateTime = millis();

    moveServoOneStep();

    doorServo.write(currentServoAngle);
}

void moveServoOneStep()
{
    if (currentServoAngle < targetServoAngle)
    {
        currentServoAngle += SERVO_STEP;

        if (currentServoAngle > targetServoAngle)
        {
            currentServoAngle = targetServoAngle;
        }
    }
    else
    {
        currentServoAngle -= SERVO_STEP;

        if (currentServoAngle < targetServoAngle)
        {
            currentServoAngle = targetServoAngle;
        }
    }
}

// =====================================================
// Debounce
// =====================================================

bool isDebounced(const char *tag)
{
    if (strcmp(tag, lastProcessedTag) != 0)
    {
        return false;
    }

    return (millis() - lastProcessedTime) < DEBOUNCE_MS;
}

void rememberProcessedTag(const char *tag)
{
    strncpy(lastProcessedTag, tag, sizeof(lastProcessedTag));
    lastProcessedTag[sizeof(lastProcessedTag) - 1] = '\0';
    lastProcessedTime = millis();
}

// =====================================================
// Leitura do pacote RFID
// =====================================================

void resetPacketReader(RfidReaderState &reader)
{
    reader.state = PacketReadState::Idle;
    reader.length = 0;
}

bool pollPacketReader(RfidReaderState &reader, uint8_t *buffer, size_t &length)
{
    length = 0;

    if (reader.state == PacketReadState::Idle)
    {
        while (rfidSerial.available())
        {
            int value = rfidSerial.read();

            if (value == START_BYTE)
            {
                reader.buffer[0] = START_BYTE;
                reader.length = 1;
                reader.lastByteTime = millis();
                reader.state = PacketReadState::Reading;
                break;
            }
        }

        return false;
    }

    while (rfidSerial.available())
    {
        int value = rfidSerial.read();

        if (reader.length >= MAX_PACKET_SIZE)
        {
            resetPacketReader(reader);
            return false;
        }

        reader.buffer[reader.length++] = value;
        reader.lastByteTime = millis();

        if (value == END_BYTE)
        {
            length = reader.length;
            memcpy(buffer, reader.buffer, length);
            resetPacketReader(reader);
            return true;
        }
    }

    if (millis() - reader.lastByteTime > BYTE_TIMEOUT_MS)
    {
        resetPacketReader(reader);
    }

    return false;
}

// =====================================================
// Conversão do pacote para Tag
// =====================================================

bool extractTag(const uint8_t *packet, size_t length, char *tag, size_t tagSize)
{
    uint16_t country = 0;
    uint64_t national = 0;

    if (!parsePacket(packet, length, country, national))
    {
        return false;
    }

    buildTag(country, national, tag, tagSize);

    return true;
}

bool parsePacket(const uint8_t *packet, size_t length, uint16_t &country, uint64_t &national)
{
    if (length < 13)
    {
        return false;
    }

    country = (packet[4] << 8) | packet[5];

    national = 0;

    for (int i = 6; i <= 10; i++)
    {
        national <<= 8;
        national |= packet[i];
    }

    return true;
}

void buildTag(uint16_t country, uint64_t national, char *tag, size_t tagSize)
{
    char countryString[8];
    char nationalString[32];

    u16ToString(country, countryString, sizeof(countryString));
    nationalToString(national, nationalString, sizeof(nationalString));

    snprintf(tag, tagSize, "%s%s", countryString, nationalString);
}

// =====================================================
// Conversões
// =====================================================

void u16ToString(uint16_t value, char *output, size_t size)
{
    u64ToString(value, output, size);
}

void u64ToString(uint64_t value, char *output, size_t size)
{
    if (value == 0)
    {
        strncpy(output, "0", size);
        return;
    }

    char buffer[32];
    int index = 0;

    while (value > 0)
    {
        buffer[index++] = '0' + (value % 10);
        value /= 10;
    }

    for (int i = 0; i < index; i++)
    {
        output[i] = buffer[index - i - 1];
    }

    output[index] = '\0';
}

void nationalToString(uint64_t national, char *output, size_t size)
{
    char number[32];

    u64ToString(national, number, sizeof(number));

    int length = strlen(number);
    int padding = NATIONAL_DEC_WIDTH - length;

    if (padding < 0)
    {
        padding = 0;
    }

    memset(output, '0', padding);
    strcpy(output + padding, number);
}