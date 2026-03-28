# Insight Profiler — Python SDK

Python client library for the [Insight Profiler](https://github.com/EmbedLab-Tech/insight-profiler) power measurement tool.

## Quick Start

```bash
# Install uv (if not already installed)
curl -LsSf https://astral.sh/uv/install.sh | sh

# Create venv + install in dev mode
cd sdk-python
uv sync --all-extras

# Run tests
uv run pytest

# Lint & format
uv run ruff check .
uv run ruff format .

# Type check
uv run mypy insight_profiler
```

## Usage

```python
from insight_profiler import InsightClient

client = InsightClient(port="/dev/cu.usbmodem1101")
client.on_sample = lambda s: print(f"{s.power_mw:.2f} mW")
client.connect()
```

## Requirements

- Python 3.12+
- macOS / Linux / Windows
