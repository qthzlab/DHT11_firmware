# Arduino DHT11 SCPI-like Instrument Firmware

## Hardware Setup

### Components
- Testing on Elegoo starter kit
- Arduino Mega 2560
- DHT11 Temperature/Humidity Sensor

### Wiring Diagram

```
  Pin 1: VCC (5.5V)
  Pin 2: DATA
  Pin 3: GND
```

### Library Installation

Need the DHT_nonblocking.h library from ELEGOO

## Quick Start

### Testing with Serial Monitor

After uploading, open Serial Monitor (115200 baud) and try:

```
*IDN?
→ GSU-Physics,DHT11-SCPI,001,1.0.0

MEAS:TEMP?
→ 23.50

MEAS:HUM?
→ 45.00

MEAS:ALL?
→ TEMP:23.50,HUM:45.00
```

### Using with QCoDeS

link to jupyterlab notebook for QCoDeS integration and usage.

## Command Reference

### IEEE 488.2 Common Commands

| Command | Description | Response |
|---------|-------------|----------|
| `*IDN?` | Identification query | `GSU-Physics,DHT11-SCPI,001,1.0.0` |
| `*RST` | Reset to defaults | `OK` |
| `*OPC?` | Operation complete | `1` |

### System Commands

| Command | Description | Response |
|---------|-------------|----------|
| `SYST:ERR?` | Query/clear last error | `0:No error` or `<code>:<message>` |
| `SYST:MODE?` | Query operating mode | `QUERY` or `STREAM` |
| `SYST:MODE <mode>` | Set mode (QUERY/STREAM) | `OK` |
| `SYST:INTV?` | Query stream interval | `3000` (ms) |
| `SYST:INTV <ms>` | Set stream interval (≥2000) | `OK` |

### Measurement Commands

| Command | Description | Response |
|---------|-------------|----------|
| `MEAS:TEMP?` | Query temperature | `23.50` |
| `MEAS:HUM?` | Query humidity | `45.00` |
| `MEAS:ALL?` | Query both | `TEMP:23.50,HUM:45.00` |

### Configuration Commands

| Command | Description | Response |
|---------|-------------|----------|
| `CONF:UNIT?` | Query temp unit | `C`, `F`, or `K` |
| `CONF:UNIT <unit>` | Set unit (C/F/K) | `OK` |
| `CONF:AVG?` | Query averaging count | `1` to `16` |
| `CONF:AVG <n>` | Set averaging (1-16) | `OK` |

### Streaming Commands

| Command | Description | Response |
|---------|-------------|----------|
| `DATA:STREAM?` | Query stream status | `ON` or `OFF` |
| `DATA:STREAM:START` | Start streaming | `OK` |
| `DATA:STREAM:STOP` | Stop streaming | `OK` |

### Stream Data Format

When streaming is active:
```
DATA:TEMP:23.50,HUM:45.00,TIME:123456
```
Where `TIME` is milliseconds since Arduino boot.

### Error Codes

| Code | Meaning |
|------|---------|
| 0 | No error |
| 100 | Unknown command |
| 101 | Invalid parameter |
| 102 | Parameter out of range |
| 200 | Sensor failure |
| 201 | Sensor timeout |
| 202 | No valid reading available |

## DHT11 Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature Range | 0-50°C | ±2°C accuracy |
| Humidity Range | 20-80% RH | ±5% accuracy |
| Sampling Rate | 1 Hz max | Firmware enforces 2s minimum |
| Supply Voltage | 3.3-5.5V | 5V recommended |
| Output | Digital | Single-wire protocol |

**For higher precision**, consider upgrading to:
- **DHT22/AM2302**: ±0.5°C, ±2-5% RH, -40 to 80°C
- **SHT31**: ±0.3°C, ±2% RH, high repeatability

The firmware architecture makes it easy to swap sensors - just change the sensor type define and adjust timing.

## Expanding the System

### Adding More Sensors

The firmware is structured to easily add additional sensors. To add a new sensor:

1. Add pin definition and initialization
2. Add measurement function
3. Add SCPI commands following the pattern:
   - `MEAS:NEWSENSOR?` for readings
   - `CONF:NEWSENSOR:*` for configuration

Example for adding a second DHT11:
```cpp
// In definitions section:
#define DHT2_SENSOR_PIN 3
DHT_nonblocking dht2_sensor(DHT2_SENSOR_PIN, DHT_SENSOR_TYPE);

// Add commands like:
// MEAS:TEMP2?
// MEAS:HUM2?
```

### Suggested Expansions

1. **Pressure sensor (BMP280)**: Add `MEAS:PRES?` command
2. **Light sensor (BH1750)**: Add `MEAS:LUX?` command  
3. **Multiple temperature sensors (DS18B20)**: Add indexed commands
4. **Analog inputs**: Add `MEAS:AIN<n>?` for ADC readings

## Troubleshooting

### "No valid reading available" (ERR:202)

The DHT11 needs 2+ seconds between readings. The firmware handles this, but if you query too fast, you may get stale data. Enable averaging to smooth this out:
```
CONF:AVG 4
```

### Garbled responses

- Check terminator settings: Should be `\n` (newline)
- Verify baud rate matches (115200)

## Files

```
file structure
```

## License

MIT License - Free to use and modify for research purposes.

## Author

QTHz Lab @ Georgia State University
Department of Physics and Astronomy
