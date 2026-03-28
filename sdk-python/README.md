# Insight Profiler — Python SDK

Python client library for the [Insight Profiler](https://github.com/EmbedLab-Tech/insight-profiler) power measurement tool.

## Quick Start

```bash
# Create a virtual environment (recommended on macOS with Homebrew Python)
python3 -m venv .venv
source .venv/bin/activate

# Install in development mode
pip install -e ".[dev,plot]"

# Run tests
pytest
```

## Usage

```python
from insight_profiler import InsightClient

client = InsightClient(port="/dev/cu.usbmodem1101")
client.on_sample = lambda s: print(f"{s.power_mw:.2f} mW")
client.connect()
```

## Requirements

- Python 3.10+
- macOS / Linux / Windows
