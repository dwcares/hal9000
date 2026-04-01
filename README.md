# HAL9000 Voice Assistant

A voice assistant powered by OpenAI running on Particle microcontrollers. Press a button to talk, receive AI-generated spoken responses. Designed to fit inside the [Moebius Models 1:1 HAL9000 kit](https://www.amazon.com/Moebius-Models-HAL9000-Styrene-MOE20015/dp/B07SVCLLFW).

> "I'm sorry Dave, I'm afraid I can't do that."

## Overview

HAL9000 demonstrates bidirectional audio streaming between a Particle device (Photon/Argon) and a Node.js server. The server uses OpenAI's Whisper for speech-to-text, GPT for conversation, and TTS for speech synthesis.

## Hardware

A custom carrier PCB is available for clean assembly. See the [hardware folder](hardware/) for KiCad source files and manufacturing outputs.

![HAL9000 PCB](hardware/hal9000-kicad.png)

### Components

| Component | Description | Connection |
|-----------|-------------|------------|
| Particle Photon | Wi-Fi MCU | Socket headers |
| MAX9814 Mic Module | Electret mic with AGC amp | J_MIC1 (3.3V/MIC/GND) |
| PAM8302A Amp Module | 2.5W Class-D mono amp | J_AMP1 (5V/AMP/GND) |
| 6mm Tactile Button | Push-to-talk | SW1 |
| 5mm LED | Status indicator | J_LED1 |
| Battery + Switch | Power input | J_BATT1, J_SW1 |
| Buck/Boost Converter | 5V regulated supply | J_BUCK1 |

### Quick Wiring (without PCB)

```
Particle Photon
┌─────────────────┐
│              A6 │──── Mic OUT (MAX9814)
│              A3 │──── Amp IN (PAM8302A)
│              D4 │──── Button ────┐
│             GND │────────────────┴── GND
│            3.3V │──── Mic VCC
│              5V │──── Amp VCC
│              D0 │──── LED+
└─────────────────┘
```

## Setup

### 1. Server Setup

```bash
cd server
npm install
```

Create a `.env` file:

```bash
cp .env.example .env
```

Edit `.env` with your credentials:

```
OPENAI_API_KEY=sk-your-key-here
PARTICLE_USERNAME=your-email@example.com
PARTICLE_PASSWORD=your-password
```

Start the server:

```bash
npm start
```

The server will:
- Login to Particle Cloud
- Publish its IP address for device discovery
- Listen for incoming device connections

### 2. Firmware Setup

Install the Particle CLI if you haven't:

```bash
npm install -g particle-cli
particle login
```

Install dependencies and flash:

```bash
cd firmware
particle library install
```

Compile and flash:

```bash
particle compile photon . --saveTo hal9000.bin
particle flash YOUR_DEVICE_NAME hal9000.bin
```

Or use Particle Workbench in VS Code.

> **Note:** No need to configure server IP - the device discovers it automatically via Particle Cloud!

## Usage

1. Power on the Particle device
2. Wait for the LED to indicate connection (solid on)
3. Press and hold the button to speak
4. Release the button when done
5. Wait for HAL's response through the speaker

## Configuration

### Server Options

| Environment Variable | Default | Description |
|---------------------|---------|-------------|
| `OPENAI_API_KEY` | - | Your OpenAI API key (required) |
| `PARTICLE_USERNAME` | - | Particle account email (for cloud discovery) |
| `PARTICLE_PASSWORD` | - | Particle account password |
| `PORT` | `5000` | Server port |
| `OPENAI_MODEL` | `gpt-4o` | Chat model |
| `OPENAI_VOICE` | `alloy` | TTS voice (alloy, echo, fable, onyx, nova, shimmer) |
| `SYSTEM_PROMPT` | (default) | AI personality prompt |
| `SAVE_RECORDINGS` | `false` | Save audio recordings to disk |

### Firmware Options

Edit the defines at the top of `hal9000.ino`:

```cpp
#define MIC_PIN      A6       // Microphone input
#define SPEAKER_PIN  A3       // Speaker output (DAC)
#define BUTTON_PIN   D4       // Push-to-talk button
#define LED_PIN      D0       // Status LED
#define SERVER_PORT  5000
```

## How Discovery Works

The server and device find each other automatically via Particle Cloud:

```
┌─────────────────┐                      ┌─────────────────┐
│  Node.js Server │                      │  Particle       │
│                 │                      │  Device         │
│ 1. Login to     │                      │                 │
│    Particle     │                      │ 3. Subscribe to │
│    Cloud        │                      │    server-ip    │
│                 │                      │    events       │
│ 2. Publish      │─── server-ip ───────►│                 │
│    server IP    │    (via cloud)       │ 4. Cache IP in  │
│                 │                      │    EEPROM       │
│ 5. Listen for   │◄── device-online ────│                 │
│    device-online│    (via cloud)       │ 5. Connect via  │
│    (republish)  │                      │    TCP          │
└─────────────────┘                      └─────────────────┘
```

- **First boot:** Device waits for server IP via cloud event
- **Subsequent boots:** Device uses cached IP for instant connect
- **Server restart:** Publishes new IP, devices update cache

## Dependencies

### Server
- [microstream-server](https://www.npmjs.com/package/microstream-server) - Audio streaming
- [particle-api-js](https://www.npmjs.com/package/particle-api-js) - Cloud discovery
- [openai](https://www.npmjs.com/package/openai) - AI services
- [dotenv](https://www.npmjs.com/package/dotenv) - Environment config

### Firmware
- [microstream](https://build.particle.io/libs/microstream) - Audio streaming library

## Troubleshooting

### No audio heard from speaker
- Check speaker wiring to DAC pin (A3 on Photon)
- Verify amplifier has power
- Check server console for "Playing response..." messages

### "Disconnected" status
- Verify server is running and reachable
- Check firewall allows port 5000
- Confirm device is on same network

### Transcription errors
- Speak clearly and close to microphone
- Check microphone wiring and amplifier gain
- Try adjusting `sampleRate` if audio quality is poor

## Architecture

```
┌─────────────────┐     TCP/WS      ┌─────────────────┐
│  Particle       │◄───────────────►│  Node.js        │
│  (microstream)  │                 │  (microstream-  │
│                 │                 │   server)       │
│  - Mic capture  │                 │                 │
│  - DAC playback │                 │  ┌───────────┐  │
│  - Button input │                 │  │ OpenAI    │  │
└─────────────────┘                 │  │ - Whisper │  │
                                    │  │ - GPT-4   │  │
                                    │  │ - TTS     │  │
                                    │  └───────────┘  │
                                    └─────────────────┘
```

## License

MIT License - see [LICENSE](LICENSE)

## Credits

Built with:
- [Microstream](https://github.com/dwcares/microstream) - Bidirectional audio streaming
- [OpenAI](https://openai.com) - AI services
- [Particle](https://particle.io) - IoT platform
