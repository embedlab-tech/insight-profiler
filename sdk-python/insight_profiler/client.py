"""Core client for communicating with the Insight Profiler device over USB CDC."""

from __future__ import annotations

import struct
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

import serial
import serial.tools.list_ports


@dataclass(frozen=True)
class PowerSample:
    """A single power measurement sample from the device."""

    timestamp_us: int
    bus_voltage_v: float
    current_ma: float
    power_mw: float


@dataclass
class InsightClient:
    """High-level client for the Insight Profiler device.

    Connects over USB CDC (serial) and reads a continuous stream of power
    measurement data in a background thread.

    Usage::

        from insight_profiler import InsightClient

        client = InsightClient(port="/dev/cu.usbmodem*")
        client.on_sample = lambda s: print(f"{s.current_ma:.3f} mA")
        client.connect()
        # ... profiling runs ...
        client.disconnect()
    """

    port: str = ""
    baudrate: int = 115200
    on_sample: Callable[[PowerSample], Any] | None = None

    _serial: serial.Serial | None = field(default=None, init=False, repr=False)
    _thread: threading.Thread | None = field(default=None, init=False, repr=False)
    _running: bool = field(default=False, init=False, repr=False)

    # -- Public API -----------------------------------------------------------

    def connect(self) -> None:
        """Open the serial connection and start the reader thread."""
        if self._running:
            return

        resolved_port = self.port or self._auto_detect_port()
        self._serial = serial.Serial(resolved_port, self.baudrate, timeout=0.1)
        self._running = True
        self._thread = threading.Thread(target=self._reader_loop, daemon=True, name="insight-reader")
        self._thread.start()

    def disconnect(self) -> None:
        """Stop the reader thread and close the serial connection."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        if self._serial is not None and self._serial.is_open:
            self._serial.close()
            self._serial = None

    @property
    def is_connected(self) -> bool:
        return self._running and self._serial is not None and self._serial.is_open

    # -- Internals ------------------------------------------------------------

    def _reader_loop(self) -> None:
        """Background thread that continuously reads and parses incoming data."""
        assert self._serial is not None
        buf = bytearray()

        while self._running:
            try:
                chunk = self._serial.read(256)
                if not chunk:
                    continue
                buf.extend(chunk)
                self._parse_buffer(buf)
            except serial.SerialException:
                break

    def _parse_buffer(self, buf: bytearray) -> None:
        """Parse binary frames from the buffer.

        Expected frame format (16 bytes):
            [0:4]   uint32  timestamp_us
            [4:8]   float32 bus_voltage_v
            [8:12]  float32 current_ma
            [12:16] float32 power_mw

        TODO: Add frame sync header and CRC once firmware protocol is finalized.
        """
        frame_size = 16
        while len(buf) >= frame_size:
            frame = bytes(buf[:frame_size])
            del buf[:frame_size]

            ts, voltage, current, power = struct.unpack("<Ifff", frame)
            sample = PowerSample(
                timestamp_us=ts,
                bus_voltage_v=voltage,
                current_ma=current,
                power_mw=power,
            )
            if self.on_sample is not None:
                self.on_sample(sample)

    @staticmethod
    def _auto_detect_port() -> str:
        """Attempt to find the Insight Profiler USB CDC port automatically."""
        for port_info in serial.tools.list_ports.comports():
            desc = (port_info.description or "").lower()
            if "insight" in desc or "esp32s3" in desc or "tinyusb" in desc:
                return port_info.device
        raise ConnectionError(
            "Insight Profiler not found. Connect the device or specify the port manually."
        )
