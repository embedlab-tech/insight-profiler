# Insight Profiler — Planned Features

## 1. Programmable Power Supply

Insight Profiler delivers a clean, adjustable voltage to the Device Under Test (DUT) — no bench PSU required.

- **Input:** USB-C Power Delivery (5 V – 20 V negotiated from any PD charger)
- **Output:** 1.8 V – 5.0 V, DAC-controlled, ±10 mV accuracy
- **Output switch:** GPIO-controlled MOSFET — enable/disable power from SDK or CLI
- **Target loads:** embedded MCUs, sensors, IoT modules, small displays, low-power radios

## 2. Wide-Range Current Measurement

Auto-ranging across three shunt resistors covers everything from deep-sleep microcontrollers to more power-hungry embedded devices without manual range switching.

| Range | Resolution | Max | Typical Use |
|---|---|---|---|
| Low (100 Ω) | ~3 nA | 1.6 mA | Deep sleep, RTC, µA IoT |
| Mid (1 Ω) | ~300 nA | 160 mA | BLE/WiFi active, sensors |
| High (10 mΩ) | ~30 µA | 3 A | Motors, displays, peak loads |

Switching between ranges is automatic and transparent to the user. The INA228 (20-bit) reads bus voltage, shunt voltage, current, and power simultaneously at up to 1 kHz.

## 3. Battery Profile Simulation

Define a voltage-vs-time discharge profile and Insight Profiler will emulate it on the output — letting you test how your firmware behaves as a battery drains, without draining a real battery.

- Built-in profiles: LiPo 3.7 V, LiFePO4 3.2 V, AA alkaline, coin cell
- Custom profiles: provide `[(time_ms, voltage_v)]` array via SDK
- Useful for: brownout detection, low-battery warnings, power-off behaviour

```python
client.play_profile("lipo_3000mah")
# or custom:
client.play_profile([(0, 4.2), (60_000, 3.7), (120_000, 3.3), (180_000, 3.0)])
```

## 4. Connectivity

| Interface | Use Case |
|---|---|
| USB-C CDC | Local development, scripting, CI pipelines |
| Wi-Fi | Remote monitoring, web dashboard, wireless CI |

Both interfaces expose the same SDK API. The device streams 16-byte binary frames at ~1 kHz over USB CDC; Wi-Fi mode uses WebSocket for equivalent real-time throughput.

## 5. SDK Ecosystem

### Python SDK

```python
from insight_profiler import InsightClient

client = InsightClient()
client.connect()
client.power_on(voltage=3.3)
client.on_sample = lambda s: print(f"{s.current_ma:.3f} mA")

trace = client.record(duration_ms=5000)
trace.to_csv("run.csv")
trace.assert_mean_below(current_ma=50)
```

### TypeScript / WebUSB SDK

```typescript
import { InsightClient } from "@embedlab-tech/insight-profiler";

const client = new InsightClient();
await client.connect();
await client.powerOn({ voltage: 3.3 });
client.onSample = (s) => console.log(s.currentMa);
```

### CLI

```bash
insight power on --voltage 3.3
insight record --duration 5s --output run.csv
insight play-profile lipo_3000mah
```

## 6. MCP Server (AI Integration)

Insight Profiler ships with a Model Context Protocol (MCP) server, making the device directly controllable by AI coding assistants (Claude, Cursor, etc.).

**Example tools exposed:**

| Tool | Description |
|---|---|
| `insight_power_on(voltage)` | Enable output at specified voltage |
| `insight_power_off()` | Disable output |
| `insight_get_sample()` | Read single measurement |
| `insight_record(duration_ms)` | Capture a power trace |
| `insight_play_profile(name_or_curve)` | Start battery simulation |
| `insight_assert_current_below(ma)` | Assert threshold — raises on failure |

**Example AI workflow:**

> *"Flash this firmware and check if deep sleep is under 10 µA"*

The AI assistant calls `insight_power_on(3.3)`, waits for boot, calls `insight_record(10000)`, analyses the trace, and reports back — without opening any GUI.

## 7. Web Dashboard

A browser-based dashboard accessible over Wi-Fi or WebUSB:

- Live current, voltage, and power plots
- Zoom, pan, annotate
- Export to CSV / JSON
- Trigger recordings and battery profiles from the UI
- Works on mobile, tablet, desktop — no installation required

## 8. CI / CD Integration

```yaml
# GitHub Actions example
- name: Power-on DUT and assert idle current
  run: |
    insight power on --voltage 3.3
    insight record --duration 3s | insight assert --mean-below 20mA
```

- pytest fixtures for in-process use
- Exit codes suitable for CI pipelines
- JSON output for downstream processing

---

## Implementation Status

| Feature | Status |
|---|---|
| INA228 firmware driver | Done |
| USB CDC streaming | Done |
| Python SDK (basic) | In progress |
| TypeScript/WebUSB SDK (basic) | In progress |
| Programmable voltage output | Planned |
| Auto-ranging shunts | Planned |
| Output enable switch | Planned |
| Battery profile simulation | Planned |
| Wi-Fi streaming | Planned |
| Web dashboard | Planned |
| CLI | Planned |
| MCP server | Planned |
