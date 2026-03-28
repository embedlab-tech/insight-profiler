/**
 * @fileoverview WebUSB client for the Insight Profiler.
 *
 * Provides a browser-based connection to the Insight Profiler device using
 * the WebUSB API. Handles device discovery, connection, and real-time
 * data streaming.
 *
 * @module @embedlab-tech/insight-profiler
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

/** A single power measurement sample received from the device. */
export interface PowerSample {
  timestampUs: number;
  busVoltageV: number;
  currentMa: number;
  powerMw: number;
}

/** USB device filter for Insight Profiler hardware. */
export interface InsightDeviceFilter {
  vendorId: number;
  productId: number;
}

/** Connection lifecycle states. */
export type ConnectionState = "disconnected" | "connecting" | "connected" | "error";

/** Callback type for incoming power samples. */
type SampleCallback = (sample: PowerSample) => void;

/** Callback type for connection state changes. */
type StateCallback = (state: ConnectionState) => void;

/** Frame size in bytes: uint32 + 3x float32 = 16 bytes. */
const FRAME_SIZE = 16;

/**
 * WebUSB client for the Insight Profiler device.
 *
 * @example
 * ```typescript
 * const client = new InsightWebUSBClient();
 * client.onSample = (sample) => console.log(`${sample.powerMw.toFixed(2)} mW`);
 * await client.requestDevice();
 * await client.connect();
 * ```
 */
export class InsightWebUSBClient {
  /** Default USB filter — update with actual VID/PID for production hardware. */
  static readonly DEFAULT_FILTER: InsightDeviceFilter = {
    vendorId: 0x303a, // Espressif USB VID
    productId: 0x1001, // TinyUSB CDC default PID — customize per device
  };

  private device: USBDevice | null = null;
  private interfaceNumber = 0;
  private endpointIn = 1;
  private endpointOut = 1;
  private reading = false;
  private buffer = new Uint8Array(0);

  /** Current connection state. */
  state: ConnectionState = "disconnected";

  /** Callback invoked for each received power sample. */
  onSample: SampleCallback | null = null;

  /** Callback invoked when the connection state changes. */
  onStateChange: StateCallback | null = null;

  // -- Public API -----------------------------------------------------------

  /**
   * Prompt the user to select an Insight Profiler device.
   * Must be called from a user gesture (click/keypress) due to WebUSB security.
   */
  async requestDevice(
    filters: InsightDeviceFilter[] = [InsightWebUSBClient.DEFAULT_FILTER]
  ): Promise<USBDevice> {
    this.device = await navigator.usb.requestDevice({ filters });
    return this.device;
  }

  /**
   * Open the USB device, claim the CDC interface, and start reading data.
   */
  async connect(): Promise<void> {
    if (!this.device) {
      throw new Error("No device selected. Call requestDevice() first.");
    }

    this.setState("connecting");

    try {
      await this.device.open();

      if (this.device.configuration === null) {
        await this.device.selectConfiguration(1);
      }

      // Find the CDC data interface and its bulk endpoints
      this.findEndpoints();

      await this.device.claimInterface(this.interfaceNumber);
      this.setState("connected");
      this.startReading();
    } catch (err) {
      this.setState("error");
      throw err;
    }
  }

  /**
   * Stop reading and release the USB device.
   */
  async disconnect(): Promise<void> {
    this.reading = false;

    if (this.device) {
      try {
        await this.device.releaseInterface(this.interfaceNumber);
        await this.device.close();
      } catch {
        // Device may already be disconnected
      }
    }

    this.device = null;
    this.buffer = new Uint8Array(0);
    this.setState("disconnected");
  }

  /**
   * Send a command to the device (e.g., start/stop, config).
   */
  async send(data: BufferSource): Promise<void> {
    if (!this.device || this.state !== "connected") {
      throw new Error("Device is not connected.");
    }
    await this.device.transferOut(this.endpointOut, data);
  }

  // -- Internals ------------------------------------------------------------

  private findEndpoints(): void {
    if (!this.device?.configuration) return;

    for (const iface of this.device.configuration.interfaces) {
      for (const alt of iface.alternates) {
        if (alt.interfaceClass === 0x0a) {
          // CDC Data class
          this.interfaceNumber = iface.interfaceNumber;
          for (const ep of alt.endpoints) {
            if (ep.direction === "in") this.endpointIn = ep.endpointNumber;
            if (ep.direction === "out") this.endpointOut = ep.endpointNumber;
          }
          return;
        }
      }
    }
  }

  private startReading(): void {
    this.reading = true;
    this.readLoop();
  }

  private async readLoop(): Promise<void> {
    while (this.reading && this.device) {
      try {
        const result = await this.device.transferIn(this.endpointIn, 512);
        if (result.data && result.data.byteLength > 0) {
          this.appendToBuffer(new Uint8Array(result.data.buffer));
          this.parseFrames();
        }
      } catch {
        if (this.reading) {
          this.setState("error");
          this.reading = false;
        }
        break;
      }
    }
  }

  private appendToBuffer(chunk: Uint8Array): void {
    const merged = new Uint8Array(this.buffer.length + chunk.length);
    merged.set(this.buffer);
    merged.set(chunk, this.buffer.length);
    this.buffer = merged;
  }

  private parseFrames(): void {
    while (this.buffer.length >= FRAME_SIZE) {
      const frame = this.buffer.slice(0, FRAME_SIZE);
      this.buffer = this.buffer.slice(FRAME_SIZE);

      const view = new DataView(frame.buffer);
      const sample: PowerSample = {
        timestampUs: view.getUint32(0, true),
        busVoltageV: view.getFloat32(4, true),
        currentMa: view.getFloat32(8, true),
        powerMw: view.getFloat32(12, true),
      };

      this.onSample?.(sample);
    }
  }

  private setState(state: ConnectionState): void {
    this.state = state;
    this.onStateChange?.(state);
  }
}
