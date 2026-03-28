#!/usr/bin/env python3
"""Log power samples to a CSV file.

Usage:
    python examples/log_to_csv.py [port] [output.csv]
"""

from __future__ import annotations

import csv
import signal
import sys

from insight_profiler import InsightClient
from insight_profiler.client import PowerSample

DEFAULT_OUTPUT = "power_log.csv"


def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else ""
    output = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUTPUT

    client = InsightClient(port=port)

    with open(output, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp_us", "bus_voltage_v", "current_ma", "power_mw"])

        def on_sample(sample: PowerSample) -> None:
            writer.writerow([
                sample.timestamp_us,
                f"{sample.bus_voltage_v:.6f}",
                f"{sample.current_ma:.6f}",
                f"{sample.power_mw:.6f}",
            ])

        client.on_sample = on_sample

        def shutdown(*_: object) -> None:
            client.disconnect()
            print(f"\nSaved to {output}")
            sys.exit(0)

        signal.signal(signal.SIGINT, shutdown)

        client.connect()
        print(f"Logging to {output} — press Ctrl+C to stop")
        signal.pause()


if __name__ == "__main__":
    main()
