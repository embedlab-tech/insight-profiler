# insight-profiler
High-precision Open Source Power Profiler & Energy Analyzer for IoT Development.

## Hardware Architecture

```mermaid
graph TD
    subgraph "Insight Profiler Hardware Architecture"
        PD[USB-C Input] -->|VBUS| DUT[Device Under Test]

        subgraph "Measurement Engine"
            SHUNT[Shunt Resistor 10 mΩ]
            INA228[INA228 20-bit Power Monitor]
            SHUNT --> INA228
        end

        DUT --- SHUNT
        INA228 -->|I2C 400 kHz| ESP[ESP32-S3 Dual-Core]

        subgraph "Connectivity"
            ESP -->|USB-C Native CDC| MAC[Mac/PC - Python SDK / WebUSB JS SDK]
        end
    end
```

## Stack

| Layer | Technology |
|---|---|
| Firmware | ESP-IDF (C++), TinyUSB CDC |
| Power sensor | TI INA228 — bus voltage, shunt voltage, current, power |
| Host SDK | Python (`pyserial`), JS/WebUSB (TypeScript) |

## Data Stream

The device streams 16-byte binary frames over USB CDC at ~1 kHz:

```
[0:4]   uint32  timestamp_us
[4:8]   float32 bus_voltage_v
[8:12]  float32 current_ma
[12:16] float32 power_mw
```

## Quick Start

### Python SDK

```python
from insight_profiler import InsightClient

client = InsightClient()
client.on_sample = lambda s: print(f"{s.current_ma:.3f} mA  {s.power_mw:.1f} mW")
client.connect()
```

### Firmware

```bash
cd firmware
idf.py build flash monitor
```

## Repository Structure

```
firmware/          ESP32-S3 firmware (ESP-IDF)
  components/
    ina228/        INA228 I2C driver
  main/            Application entry point
sdk-python/        Python host SDK
sdk-js/            TypeScript/WebUSB SDK
hardware/          Schematics, PCB layout, BOM (in progress)
```
