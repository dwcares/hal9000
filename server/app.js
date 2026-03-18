require('dotenv').config()

const fs = require('fs')
const path = require('path')
const os = require('os')
const Particle = require('particle-api-js')
const { MicrostreamServer } = require('microstream-server')
const SpeechPipeline = require('./SpeechPipeline')

const PORT = process.env.PORT || 5000
const SAVE_RECORDINGS = process.env.SAVE_RECORDINGS === 'true'

// Get local IP address (prefer Wi-Fi)
function getLocalIP() {
  const interfaces = os.networkInterfaces()
  for (const name of ['Wi-Fi', 'en0', 'eth0', 'Ethernet']) {
    if (interfaces[name]) {
      for (const iface of interfaces[name]) {
        if (iface.family === 'IPv4' && !iface.internal) {
          return iface.address
        }
      }
    }
  }
  // Fallback: first non-internal IPv4
  for (const ifaces of Object.values(interfaces)) {
    for (const iface of ifaces) {
      if (iface.family === 'IPv4' && !iface.internal) {
        return iface.address
      }
    }
  }
  return '127.0.0.1'
}

// Particle Cloud integration
const particle = new Particle()
let particleToken = null

async function particleLogin() {
  if (!process.env.PARTICLE_USERNAME || !process.env.PARTICLE_PASSWORD) {
    console.warn('PARTICLE_USERNAME/PASSWORD not set - cloud discovery disabled')
    return false
  }

  try {
    const result = await particle.login({
      username: process.env.PARTICLE_USERNAME,
      password: process.env.PARTICLE_PASSWORD
    })
    particleToken = result.body.access_token
    console.log('Particle Cloud: logged in')
    return true
  } catch (err) {
    console.error('Particle login failed:', err.message)
    return false
  }
}

async function publishServerIP() {
  if (!particleToken) return

  const ip = getLocalIP()
  console.log(`Publishing server IP: ${ip}`)
  try {
    await particle.publishEvent({
      name: 'microstream/server-ip',
      data: ip,
      isPrivate: true,
      auth: particleToken
    })
  } catch (err) {
    console.error('Failed to publish IP:', err.message)
  }
}

async function subscribeToDevices() {
  if (!particleToken) return

  try {
    const eventStream = await particle.getEventStream({
      name: 'microstream/device-online',
      auth: particleToken
    })

    eventStream.on('event', (event) => {
      console.log('Device came online, publishing server IP...')
      publishServerIP()
    })

    console.log('Subscribed to device-online events')
  } catch (err) {
    console.error('Failed to subscribe to events:', err.message)
  }
}

// Directory to save audio files (only created if saving is enabled)
const AUDIO_DIR = path.join(__dirname, 'audio')
if (SAVE_RECORDINGS && !fs.existsSync(AUDIO_DIR)) {
  fs.mkdirSync(AUDIO_DIR, { recursive: true })
}

const server = new MicrostreamServer({
  port: PORT,
  audio: { sampleRate: 11200, bitDepth: 16, channels: 1 }
})

const pipeline = new SpeechPipeline({
  apiKey: process.env.OPENAI_API_KEY,
  model: process.env.OPENAI_MODEL || 'gpt-4o',
  voice: process.env.OPENAI_VOICE || 'alloy',
  systemPrompt: process.env.SYSTEM_PROMPT || undefined
})

server.on('session', (session) => {
  const tag = session.id.slice(0, 8)
  console.log(`[${tag}] Device connected`)

  pipeline.initSession(session.id)

  session.on('audioStart', () => {
    console.log(`[${tag}] Recording started`)
  })

  session.on('audioEnd', async (wavBuffer) => {
    console.log(`[${tag}] Recording ended (${wavBuffer.length} bytes)`)

    // Optionally save incoming audio to file
    if (SAVE_RECORDINGS) {
      const timestamp = new Date().toISOString().replace(/[:.]/g, '-')
      const filename = `recording-${tag}-${timestamp}.wav`
      const filepath = path.join(AUDIO_DIR, filename)
      fs.writeFileSync(filepath, wavBuffer)
      console.log(`[${tag}] Saved audio to: ${filepath}`)
    }

    try {
      const result = await pipeline.processAudio(session.id, wavBuffer)
      if (result && result.audio) {
        session.play(result.audio)
      }
    } catch (err) {
      console.error(`[${tag}] Pipeline error:`, err.message)
    }
  })

  session.on('disconnect', () => {
    console.log(`[${tag}] Device disconnected`)
    pipeline.cleanupSession(session.id)
  })

  session.on('error', (err) => {
    console.error(`[${tag}] Session error:`, err.message)
  })
})

server.on('error', (err) => {
  console.error('Server error:', err.message)
})

server.on('listening', async (port) => {
  console.log(`HAL 9000 server listening on port ${port}`)
  console.log(`Network: ${getLocalIP()}`)

  // Login to Particle Cloud and set up discovery
  if (await particleLogin()) {
    await subscribeToDevices()
    await publishServerIP()
  }

  console.log(`Waiting for device connections...`)
})

server.listen()
