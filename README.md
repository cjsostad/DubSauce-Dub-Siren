# DubSauce Dub Siren

A C++17 dub siren for Raspberry Pi 3 and Raspberry Pi 4. The hardware application uses DaisySP for its oscillator and DSP, ALSA for audio playback, and GPIO controls for live performance. Audio output is intended for a PCM5102 I2S DAC.

## Sound Engine

The current voice combines a band-limited oscillator, an LFO that modulates pitch, a tone low-pass filter, and a feedback delay with filtering and soft clipping. The default sample rate is 48 kHz.

**There is no reverb in this version.** Reverb and filter-resonance methods remain in the C++ interface for compatibility with older controller code, but the voice does not implement those effects. Pitch-envelope methods are also compatibility-only and do not make the voice pitch up or down. The three-position switch currently gates the siren instead; see [Controls](#controls).

## Hardware

- Raspberry Pi 3 or Raspberry Pi 4 running Raspberry Pi OS
- PCM5102 I2S DAC and a suitable amplifier or powered speakers
- Five rotary encoders
- Trigger, Shift, and Shutdown buttons
- Three-position Up/Off/Down switch
- Optional WS2812 status LED

The table uses **BCM GPIO numbers** and **physical 40-pin header numbers**. Input signals use pull-ups and are active when connected to ground. Connect encoder common terminals and switch/button ground terminals to a Pi GND pin. Check [HARDWARE.md](HARDWARE.md) for DAC power and board-specific wiring details before connecting hardware.

### GPIO and Wiring

| Component | Signal | BCM GPIO | Physical header pin | Wire to |
| --- | --- | ---: | ---: | --- |
| Encoder 1 | CLK | 17 | 11 | Encoder 1 CLK |
| Encoder 1 | DT | 5 | 29 | Encoder 1 DT |
| Encoder 2 | CLK | 27 | 13 | Encoder 2 CLK |
| Encoder 2 | DT | 22 | 15 | Encoder 2 DT |
| Encoder 3 | CLK | 23 | 16 | Encoder 3 CLK |
| Encoder 3 | DT | 24 | 18 | Encoder 3 DT |
| Encoder 4 | CLK | 20 | 38 | Encoder 4 CLK |
| Encoder 4 | DT | 26 | 37 | Encoder 4 DT |
| Encoder 5 | CLK | 14 | 8 | Encoder 5 CLK |
| Encoder 5 | DT | 13 | 33 | Encoder 5 DT |
| Trigger button | Signal | 4 | 7 | Button terminal; other terminal to GND |
| Shift button | Signal | 15 | 10 | Button terminal; other terminal to GND |
| Shutdown button | Signal | 3 | 5 | Button terminal; other terminal to GND |
| 3-position switch | Up contact | 10 | 19 | Up throw |
| 3-position switch | Down contact | 9 | 21 | Down throw |
| WS2812 LED (optional) | Data | 12 | 32 | LED data input; power as specified by LED/module |
| PCM5102 DAC | LCK/LRCK | 18 | 12 | DAC LCK/LRCK |
| PCM5102 DAC | BCK/BCLK | 19 | 35 | DAC BCK/BCLK |
| PCM5102 DAC | DIN | 21 | 40 | DAC DIN |

The 3-position switch common connects to GND; center/off leaves both throws open. Encoder common pins also connect to GND. The DAC uses BCM GPIO 18, 19, and 21 for I2S; do not connect controls to those BCM pins. The pin table above reflects the current C++ GPIO definitions. Some older wiring notes in `GPIO_WIRING_GUIDE.md` list earlier assignments, so use this table for this build.

## Install and Build

On the Pi, clone the project and its DaisySP dependency:

```bash
git clone https://github.com/cjsostad/DubSauce-Dub-Siren.git
cd DubSauce-Dub-Siren
git clone https://github.com/electro-smith/DaisySP.git cpp/DaisySP
```

CMake requires DaisySP at `cpp/DaisySP`; the dependency is not bundled in the project checkout.

For a full Raspberry Pi setup, run:

```bash
bash cpp/setup.sh
```

The setup script installs build and audio dependencies, configures I2S, builds the application, and installs the `dubsiren-cpp.service` systemd unit. Review the script before running it if the Pi has custom audio or system configuration.

To build manually, install `build-essential`, `cmake`, `git`, `libasound2-dev`, and `libgpiod-dev`, then run:

```bash
./cpp/build.sh
```

The build script detects the Pi architecture and enables Raspberry Pi build options. Add `--clean` to remove and recreate the build directory:

```bash
./cpp/build.sh --clean
```

Or configure and build directly from the repository root:

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release -DBUILD_FOR_PI=ON
cmake --build cpp/build --parallel 2
```

The executable is `cpp/build/dubsiren`.

## Run

With the DAC connected and I2S configured, start the hardware application:

```bash
./cpp/build/dubsiren
```

The default ALSA device is `default`. Select another playback device with `--device DEVICE`. Other options are:

```text
--sample-rate RATE    Sample rate (default: 48000)
--buffer-size SIZE    Buffer size in samples (default: 256)
--device DEVICE       ALSA playback device (default: default)
--simulate            Run DSP without physical audio output
--interactive         Enable keyboard controls
--help                Print usage information
```

For a no-hardware, no-audio test, run both simulation and interactive mode:

```bash
./cpp/build/dubsiren --simulate --interactive
```

In interactive mode, `t` toggles the siren gate, `s` prints status, `h` prints help, and `q` exits. `p` cycles the stored compatibility pitch-envelope setting; it does not change the sound.

### Run as a Service

After `cpp/setup.sh` has installed the service:

```bash
sudo systemctl enable --now dubsiren-cpp.service
sudo systemctl status dubsiren-cpp.service
```

Follow its logs with `sudo journalctl -u dubsiren-cpp.service -f`. Stop it with `sudo systemctl stop dubsiren-cpp.service`; disable automatic startup with `sudo systemctl disable dubsiren-cpp.service`.

## Controls

Hold **Shift** to select Bank B and release it to return to Bank A. Encoders are numbered from left to right.

| Encoder | Bank A (Shift released) | Bank B (Shift held) |
| --- | --- | --- |
| 1 | LFO depth (0 to 1) | Level / output volume (0 to 1) |
| 2 | Base pitch (50 to 2000 Hz) | Delay time (0.001 to 2 seconds) |
| 3 | Tone filter cutoff (250 to 20000 Hz) | LFO/wail shape: Sine, Square, Saw, Triangle |
| 4 | Delay feedback (0 to 0.95) | Oscillator shape: Sine, Square, Saw, Triangle |
| 5 | LFO rate (0.1 to 20 Hz) | Unused (former reverb-size control) |

Hold **Trigger** to sound the siren; release it to stop. The three-position switch also operates the gate: either Up or Down turns the siren on, while Off/center turns the switch gate off. The siren remains on while the trigger button is held or the switch is off-center. **The switch does not currently pitch the siren up or down.**

There is no reverb processing. The former reverb-mix encoder position has been repurposed as **Level**; the former reverb-size position is **unused**. The Shutdown button requests a system shutdown.

## Troubleshooting

- **No audio:** Check PCM5102 wiring and I2S configuration, list ALSA devices with `aplay -l`, and try selecting the correct device with `--device`. See [PCM5102_TROUBLESHOOTING.md](PCM5102_TROUBLESHOOTING.md).
- **GPIO controls do not respond:** Verify the BCM and physical pin numbers in the table above. Install `libgpiod-dev` and `gpiod`; this build requests inputs through `/dev/gpiochip0` when libgpiod is available.
- **CMake cannot find DaisySP:** Verify `cpp/DaisySP/CMakeLists.txt` exists. Clone DaisySP into `cpp/DaisySP` if it is missing.
- **Audio glitches:** Check CPU load, try a larger `--buffer-size`, and run `bash audio_diagnostics.sh`.

## Tests

The Jest suite covers the retained browser/Web Audio prototype:

```bash
npm install
npm test
```

It does not test the C++ DaisySP voice, Raspberry Pi GPIO, ALSA playback, or physical hardware. Test those on the target Pi.

## License

MIT. See [LICENSE](LICENSE).
