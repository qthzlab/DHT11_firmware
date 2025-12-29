/*
 * DHT11 SCPI-like Firmware for Arduino Mega
 * ==========================================
 * 
 * Professional-grade firmware for temperature/humidity monitoring
 * with QCoDeS-compatible serial command interface.
 * 
 * Author: QTHz Lab @ GSU
 * Version: 1.0.0
 * Date: 2025
 * 
 * Hardware: Arduino Mega 2560 + DHT11 sensor
 * 
 * Command Reference:
 * ------------------
 * *IDN?                    - Query instrument identification
 * *RST                     - Reset to default settings
 * *OPC?                    - Operation complete query (always returns 1)
 * 
 * SYST:ERR?                - Query last error (then clears it)
 * SYST:MODE <STREAM|QUERY> - Set operating mode
 * SYST:MODE?               - Query operating mode
 * SYST:INTV <ms>           - Set streaming interval (minimum 2000ms)
 * SYST:INTV?               - Query streaming interval
 * 
 * MEAS:TEMP?               - Query temperature (Celsius)
 * MEAS:HUM?                - Query humidity (%)
 * MEAS:ALL?                - Query both: returns "TEMP:<val>,HUM:<val>"
 * 
 * CONF:UNIT <C|F|K>        - Set temperature unit
 * CONF:UNIT?               - Query temperature unit
 * CONF:AVG <1-16>          - Set averaging count
 * CONF:AVG?                - Query averaging count
 * 
 * DATA:STREAM:START        - Start continuous streaming
 * DATA:STREAM:STOP         - Stop continuous streaming
 * DATA:STREAM?             - Query streaming status (ON/OFF)
 * 
 * Response Format:
 * ----------------
 * Successful queries return the value directly (e.g., "23.5")
 * Successful commands return "OK"
 * Errors return "ERR:<code>:<message>"
 * Stream data format: "DATA:TEMP:<val>,HUM:<val>,TIME:<ms>"
 */

#include <dht_nonblocking.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

#define FIRMWARE_VERSION    "1.0.0"
#define FIRMWARE_NAME       "DHT11-SCPI"
#define MANUFACTURER        "QTHz-Lab"
#define SERIAL_NUMBER       "001"

#define DHT_SENSOR_PIN      2
#define DHT_SENSOR_TYPE     DHT_TYPE_11

#define SERIAL_BAUD_RATE    115200      // Higher baud for responsiveness
#define CMD_BUFFER_SIZE     64          // Max command length
#define MIN_SAMPLE_INTERVAL 2000        // DHT11 minimum (ms)
#define DEFAULT_INTERVAL    3000        // Default streaming interval (ms)

// ============================================================================
// ENUMERATIONS AND STRUCTURES
// ============================================================================

enum OperatingMode {
    MODE_QUERY,     // Only respond to queries
    MODE_STREAM     // Continuous output + queries
};

enum TempUnit {
    UNIT_CELSIUS,
    UNIT_FAHRENHEIT,
    UNIT_KELVIN
};

enum ErrorCode {
    ERR_NONE = 0,
    ERR_UNKNOWN_CMD = 100,
    ERR_INVALID_PARAM = 101,
    ERR_PARAM_RANGE = 102,
    ERR_SENSOR_FAIL = 200,
    ERR_SENSOR_TIMEOUT = 201,
    ERR_NOT_READY = 202
};

struct InstrumentState {
    OperatingMode mode;
    TempUnit unit;
    unsigned long streamInterval;
    uint8_t averagingCount;
    bool streamingActive;
    ErrorCode lastError;
    char errorMessage[32];
};

struct SensorData {
    float temperature;      // Always stored in Celsius internally
    float humidity;
    bool valid;
    unsigned long timestamp;
    unsigned long lastReadTime;
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

DHT_nonblocking dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

InstrumentState state;
SensorData sensorData;

char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdIndex = 0;

// Averaging buffer
float tempBuffer[16];
float humBuffer[16];
uint8_t bufferIndex = 0;
uint8_t samplesCollected = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void initializeState();
void processSerialInput();
void parseAndExecuteCommand(char* cmd);
void handleStreaming();
bool updateSensorReading();
float convertTemperature(float celsius);
const char* getUnitString();
void setError(ErrorCode code, const char* msg);
void clearError();

// Command handlers
void cmdIDN();
void cmdRST();
void cmdOPC();
void cmdSystErr();
void cmdSystMode(char* param);
void cmdSystModeQ();
void cmdSystIntv(char* param);
void cmdSystIntvQ();
void cmdMeasTemp();
void cmdMeasHum();
void cmdMeasAll();
void cmdConfUnit(char* param);
void cmdConfUnitQ();
void cmdConfAvg(char* param);
void cmdConfAvgQ();
void cmdDataStreamStart();
void cmdDataStreamStop();
void cmdDataStreamQ();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    
    while (!Serial) {
        ; 
    }
    
    // Wait for USB serial to stabilize (prevents garbage)
    delay(500);
    
    // Clear input buffer
    while (Serial.available() > 0) {
        Serial.read();
    }
    
    initializeState();
    
    // DHT11 warm-up
    delay(2000);
    
    float dummy_t, dummy_h;
    dht_sensor.measure(&dummy_t, &dummy_h);
    
    // Signal ready to Python
    Serial.println("READY");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Always process incoming commands
    processSerialInput();
    
    // Update sensor reading (non-blocking)
    updateSensorReading();
    
    // Handle streaming if active
    if (state.mode == MODE_STREAM && state.streamingActive) {
        handleStreaming();
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void initializeState() {
    state.mode = MODE_QUERY;
    state.unit = UNIT_CELSIUS;
    state.streamInterval = DEFAULT_INTERVAL;
    state.averagingCount = 1;
    state.streamingActive = false;
    state.lastError = ERR_NONE;
    state.errorMessage[0] = '\0';
    
    sensorData.temperature = 0.0;
    sensorData.humidity = 0.0;
    sensorData.valid = false;
    sensorData.timestamp = 0;
    sensorData.lastReadTime = 0;
    
    bufferIndex = 0;
    samplesCollected = 0;
}

// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================

void processSerialInput() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        // Command terminator (newline or carriage return)
        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0';
                parseAndExecuteCommand(cmdBuffer);
                cmdIndex = 0;
            }
        }
        // Buffer the character
        else if (cmdIndex < CMD_BUFFER_SIZE - 1) {
            // Convert to uppercase for case-insensitivity
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            cmdBuffer[cmdIndex++] = c;
        }
        // Buffer overflow - reset
        else {
            cmdIndex = 0;
            setError(ERR_UNKNOWN_CMD, "Command too long");
        }
    }
}

void parseAndExecuteCommand(char* cmd) {
    // Trim leading whitespace
    while (*cmd == ' ') cmd++;
    
    // Empty command
    if (*cmd == '\0') return;
    
    // Find parameter (after space or colon for set commands)
    char* param = NULL;
    char* space = strchr(cmd, ' ');
    if (space) {
        *space = '\0';
        param = space + 1;
        // Trim parameter whitespace
        while (*param == ' ') param++;
    }
    
    // ---- IEEE 488.2 Common Commands ----
    if (strcmp(cmd, "*IDN?") == 0) {
        cmdIDN();
    }
    else if (strcmp(cmd, "*RST") == 0) {
        cmdRST();
    }
    else if (strcmp(cmd, "*OPC?") == 0) {
        cmdOPC();
    }
    // ---- System Commands ----
    else if (strcmp(cmd, "SYST:ERR?") == 0) {
        cmdSystErr();
    }
    else if (strcmp(cmd, "SYST:MODE?") == 0) {
        cmdSystModeQ();
    }
    else if (strcmp(cmd, "SYST:MODE") == 0) {
        cmdSystMode(param);
    }
    else if (strcmp(cmd, "SYST:INTV?") == 0) {
        cmdSystIntvQ();
    }
    else if (strcmp(cmd, "SYST:INTV") == 0) {
        cmdSystIntv(param);
    }
    // ---- Measurement Commands ----
    else if (strcmp(cmd, "MEAS:TEMP?") == 0) {
        cmdMeasTemp();
    }
    else if (strcmp(cmd, "MEAS:HUM?") == 0) {
        cmdMeasHum();
    }
    else if (strcmp(cmd, "MEAS:ALL?") == 0) {
        cmdMeasAll();
    }
    // ---- Configuration Commands ----
    else if (strcmp(cmd, "CONF:UNIT?") == 0) {
        cmdConfUnitQ();
    }
    else if (strcmp(cmd, "CONF:UNIT") == 0) {
        cmdConfUnit(param);
    }
    else if (strcmp(cmd, "CONF:AVG?") == 0) {
        cmdConfAvgQ();
    }
    else if (strcmp(cmd, "CONF:AVG") == 0) {
        cmdConfAvg(param);
    }
    // ---- Data Stream Commands ----
    else if (strcmp(cmd, "DATA:STREAM:START") == 0) {
        cmdDataStreamStart();
    }
    else if (strcmp(cmd, "DATA:STREAM:STOP") == 0) {
        cmdDataStreamStop();
    }
    else if (strcmp(cmd, "DATA:STREAM?") == 0) {
        cmdDataStreamQ();
    }
    // ---- Unknown Command ----
    else {
        setError(ERR_UNKNOWN_CMD, cmd);
        Serial.print("ERR:");
        Serial.print(ERR_UNKNOWN_CMD);
        Serial.print(":Unknown command: ");
        Serial.println(cmd);
    }
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void cmdIDN() {
    // Format: Manufacturer,Model,SerialNumber,FirmwareVersion
    Serial.print(MANUFACTURER);
    Serial.print(",");
    Serial.print(FIRMWARE_NAME);
    Serial.print(",");
    Serial.print(SERIAL_NUMBER);
    Serial.print(",");
    Serial.println(FIRMWARE_VERSION);
}

void cmdRST() {
    initializeState();
    Serial.println("OK");
}

void cmdOPC() {
    // Operation complete - always ready for this simple instrument
    Serial.println("1");
}

void cmdSystErr() {
    if (state.lastError == ERR_NONE) {
        Serial.println("0:No error");
    } else {
        Serial.print(state.lastError);
        Serial.print(":");
        Serial.println(state.errorMessage);
    }
    clearError();
}

void cmdSystMode(char* param) {
    if (param == NULL) {
        setError(ERR_INVALID_PARAM, "Missing parameter");
        Serial.println("ERR:101:Missing parameter");
        return;
    }
    
    if (strcmp(param, "STREAM") == 0) {
        state.mode = MODE_STREAM;
        Serial.println("OK");
    }
    else if (strcmp(param, "QUERY") == 0) {
        state.mode = MODE_QUERY;
        state.streamingActive = false;
        Serial.println("OK");
    }
    else {
        setError(ERR_INVALID_PARAM, param);
        Serial.println("ERR:101:Invalid mode (use STREAM or QUERY)");
    }
}

void cmdSystModeQ() {
    Serial.println(state.mode == MODE_STREAM ? "STREAM" : "QUERY");
}

void cmdSystIntv(char* param) {
    if (param == NULL) {
        setError(ERR_INVALID_PARAM, "Missing parameter");
        Serial.println("ERR:101:Missing parameter");
        return;
    }
    
    unsigned long interval = atol(param);
    
    if (interval < MIN_SAMPLE_INTERVAL) {
        setError(ERR_PARAM_RANGE, "Interval too small");
        Serial.print("ERR:102:Minimum interval is ");
        Serial.print(MIN_SAMPLE_INTERVAL);
        Serial.println(" ms");
        return;
    }
    
    state.streamInterval = interval;
    Serial.println("OK");
}

void cmdSystIntvQ() {
    Serial.println(state.streamInterval);
}

void cmdMeasTemp() {
    if (!sensorData.valid) {
        setError(ERR_NOT_READY, "No valid reading");
        Serial.println("ERR:202:No valid reading available");
        return;
    }
    
    float temp = convertTemperature(sensorData.temperature);
    Serial.println(temp, 2);
}

void cmdMeasHum() {
    if (!sensorData.valid) {
        setError(ERR_NOT_READY, "No valid reading");
        Serial.println("ERR:202:No valid reading available");
        return;
    }
    
    Serial.println(sensorData.humidity, 2);
}

void cmdMeasAll() {
    if (!sensorData.valid) {
        setError(ERR_NOT_READY, "No valid reading");
        Serial.println("ERR:202:No valid reading available");
        return;
    }
    
    float temp = convertTemperature(sensorData.temperature);
    Serial.print("TEMP:");
    Serial.print(temp, 2);
    Serial.print(",HUM:");
    Serial.println(sensorData.humidity, 2);
}

void cmdConfUnit(char* param) {
    if (param == NULL) {
        setError(ERR_INVALID_PARAM, "Missing parameter");
        Serial.println("ERR:101:Missing parameter");
        return;
    }
    
    if (strcmp(param, "C") == 0) {
        state.unit = UNIT_CELSIUS;
        Serial.println("OK");
    }
    else if (strcmp(param, "F") == 0) {
        state.unit = UNIT_FAHRENHEIT;
        Serial.println("OK");
    }
    else if (strcmp(param, "K") == 0) {
        state.unit = UNIT_KELVIN;
        Serial.println("OK");
    }
    else {
        setError(ERR_INVALID_PARAM, param);
        Serial.println("ERR:101:Invalid unit (use C, F, or K)");
    }
}

void cmdConfUnitQ() {
    Serial.println(getUnitString());
}

void cmdConfAvg(char* param) {
    if (param == NULL) {
        setError(ERR_INVALID_PARAM, "Missing parameter");
        Serial.println("ERR:101:Missing parameter");
        return;
    }
    
    int avg = atoi(param);
    
    if (avg < 1 || avg > 16) {
        setError(ERR_PARAM_RANGE, "Out of range");
        Serial.println("ERR:102:Averaging must be 1-16");
        return;
    }
    
    state.averagingCount = (uint8_t)avg;
    // Reset averaging buffer
    bufferIndex = 0;
    samplesCollected = 0;
    Serial.println("OK");
}

void cmdConfAvgQ() {
    Serial.println(state.averagingCount);
}

void cmdDataStreamStart() {
    if (state.mode != MODE_STREAM) {
        setError(ERR_INVALID_PARAM, "Not in STREAM mode");
        Serial.println("ERR:101:Set SYST:MODE STREAM first");
        return;
    }
    state.streamingActive = true;
    Serial.println("OK");
}

void cmdDataStreamStop() {
    state.streamingActive = false;
    Serial.println("OK");
}

void cmdDataStreamQ() {
    Serial.println(state.streamingActive ? "ON" : "OFF");
}

// ============================================================================
// SENSOR READING
// ============================================================================

bool updateSensorReading() {
    static unsigned long lastAttempt = 0;
    float temperature, humidity;
    
    // Don't attempt too frequently
    if (millis() - lastAttempt < MIN_SAMPLE_INTERVAL) {
        return false;
    }
    
    // Try to get a reading
    if (dht_sensor.measure(&temperature, &humidity)) {
        lastAttempt = millis();
        
        // Add to averaging buffer
        tempBuffer[bufferIndex] = temperature;
        humBuffer[bufferIndex] = humidity;
        bufferIndex = (bufferIndex + 1) % state.averagingCount;
        
        if (samplesCollected < state.averagingCount) {
            samplesCollected++;
        }
        
        // Calculate averages
        float tempSum = 0, humSum = 0;
        uint8_t count = min(samplesCollected, state.averagingCount);
        
        for (uint8_t i = 0; i < count; i++) {
            tempSum += tempBuffer[i];
            humSum += humBuffer[i];
        }
        
        sensorData.temperature = tempSum / count;
        sensorData.humidity = humSum / count;
        sensorData.valid = true;
        sensorData.timestamp = millis();
        sensorData.lastReadTime = millis();
        
        return true;
    }
    
    return false;
}

// ============================================================================
// STREAMING
// ============================================================================

void handleStreaming() {
    static unsigned long lastStreamOutput = 0;
    
    if (millis() - lastStreamOutput >= state.streamInterval) {
        if (sensorData.valid) {
            float temp = convertTemperature(sensorData.temperature);
            
            Serial.print("DATA:TEMP:");
            Serial.print(temp, 2);
            Serial.print(",HUM:");
            Serial.print(sensorData.humidity, 2);
            Serial.print(",TIME:");
            Serial.println(sensorData.timestamp);
            
            lastStreamOutput = millis();
        }
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float convertTemperature(float celsius) {
    switch (state.unit) {
        case UNIT_FAHRENHEIT:
            return celsius * 9.0 / 5.0 + 32.0;
        case UNIT_KELVIN:
            return celsius + 273.15;
        case UNIT_CELSIUS:
        default:
            return celsius;
    }
}

const char* getUnitString() {
    switch (state.unit) {
        case UNIT_FAHRENHEIT: return "F";
        case UNIT_KELVIN: return "K";
        case UNIT_CELSIUS:
        default: return "C";
    }
}

void setError(ErrorCode code, const char* msg) {
    state.lastError = code;
    strncpy(state.errorMessage, msg, sizeof(state.errorMessage) - 1);
    state.errorMessage[sizeof(state.errorMessage) - 1] = '\0';
}

void clearError() {
    state.lastError = ERR_NONE;
    state.errorMessage[0] = '\0';
}
