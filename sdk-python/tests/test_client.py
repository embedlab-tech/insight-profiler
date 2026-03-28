"""Unit tests for InsightClient."""

import struct

from insight_profiler.client import InsightClient, PowerSample


def test_parse_single_frame():
    """Verify that a single binary frame is correctly parsed into a PowerSample."""
    client = InsightClient()
    samples: list[PowerSample] = []
    client.on_sample = samples.append

    frame = struct.pack("<Ifff", 1000, 3.3, 15.2, 50.16)
    buf = bytearray(frame)
    client._parse_buffer(buf)

    assert len(samples) == 1
    assert samples[0].timestamp_us == 1000
    assert abs(samples[0].bus_voltage_v - 3.3) < 0.01
    assert abs(samples[0].current_ma - 15.2) < 0.1
    assert len(buf) == 0


def test_parse_partial_frame_buffered():
    """Partial frames should remain in the buffer until complete."""
    client = InsightClient()
    samples: list[PowerSample] = []
    client.on_sample = samples.append

    frame = struct.pack("<Ifff", 2000, 5.0, 100.0, 500.0)
    buf = bytearray(frame[:10])
    client._parse_buffer(buf)
    assert len(samples) == 0
    assert len(buf) == 10

    buf.extend(frame[10:])
    client._parse_buffer(buf)
    assert len(samples) == 1
    assert samples[0].power_mw == 500.0
