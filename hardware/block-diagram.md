# Insight Profiler — Hardware Block Diagram

```mermaid
graph TD
    USBC[USB-C Connector]
    PD[PD Negotiation\nHUSB238\n5V / 9V / 12V / 15V / 20V]
    BUCK[Buck-Boost Regulator\nDAC-controlled\n1.8V – 5.0V programmable]
    SW[Output Enable\nMOSFET Switch\nGPIO-controlled]

    subgraph MEAS[Auto-Ranging Measurement]
        direction LR
        R1[10 mΩ\n30 µA – 3 A]
        R2[1 Ω\n300 nA – 160 mA]
        R3[100 Ω\n3 nA – 1.6 mA]
        RSEL[Range Selector\nMOSFET Array]
        R1 & R2 & R3 --> RSEL
    end

    INA228[INA228\n20-bit Power Monitor]
    ESP[ESP32-S3\nDual-Core + WiFi]
    DUT[Device Under Test]

    USBC --> PD
    PD --> BUCK
    BUCK --> SW
    SW --> MEAS
    MEAS --> DUT

    RSEL --> INA228
    INA228 -->|I2C 400 kHz| ESP

    ESP -.->|DAC / PWM| BUCK
    ESP -.->|GPIO| SW
    ESP -.->|GPIO| RSEL

    ESP -->|USB-C Native CDC| USBHOST[Host PC\nPython SDK / CLI]
    ESP -->|Wi-Fi| WIFI[Web Dashboard\nBrowser / CI Server / MCP]
```

## Signal Flow

```
USB-C PD → regulated voltage → output switch → shunt array → DUT
                                                    ↓
                                               INA228 (I2C)
                                                    ↓
                                               ESP32-S3
                                              /         \
                                          USB CDC       Wi-Fi
```

## Current Ranges

| Shunt | Resolution | Max Current | Use Case |
|---|---|---|---|
| 100 Ω | ~3 nA | 1.6 mA | Deep sleep, µA IoT |
| 1 Ω | ~300 nA | 160 mA | Active IoT, BLE/WiFi bursts |
| 10 mΩ | ~30 µA | 3 A | Motors, displays, high-power loads |

Range switching is automatic — firmware selects the appropriate shunt based on measured current level.

## Voltage Output

| Source | Range | Notes |
|---|---|---|
| USB-C PD negotiation | 5 / 9 / 12 / 15 / 20 V | Fixed PD profiles |
| Buck-boost output | 1.8 V – 5.0 V | Continuously adjustable, DAC-controlled |

Target output accuracy: ±10 mV. Primarily intended for 1.8V–5.0V embedded devices.
