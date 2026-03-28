#!/usr/bin/env python3
"""Live power plot using matplotlib.

Usage:
    python -m venv .venv && source .venv/bin/activate
    pip install -e ".[plot]"
    python examples/plot_live.py
"""

from __future__ import annotations

import sys
import collections

import matplotlib.pyplot as plt
import matplotlib.animation as animation

from insight_profiler import InsightClient
from insight_profiler.client import PowerSample

WINDOW = 500  # Number of samples to display

power_data: collections.deque[float] = collections.deque(maxlen=WINDOW)


def on_sample(sample: PowerSample) -> None:
    power_data.append(sample.power_mw)


def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else ""

    client = InsightClient(port=port)
    client.on_sample = on_sample
    client.connect()

    fig, ax = plt.subplots()
    ax.set_title("Insight Profiler — Live Power")
    ax.set_xlabel("Sample")
    ax.set_ylabel("Power (mW)")
    (line,) = ax.plot([], [], lw=1.5, color="#0066cc")

    def update(_frame: int) -> tuple:
        if power_data:
            line.set_data(range(len(power_data)), list(power_data))
            ax.set_xlim(0, WINDOW)
            ax.set_ylim(0, max(power_data) * 1.2 or 1)
        return (line,)

    _anim = animation.FuncAnimation(fig, update, interval=50, blit=True)

    try:
        plt.show()
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()
