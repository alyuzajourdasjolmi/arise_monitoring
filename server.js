const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const bodyParser = require('body-parser');
const webpush = require('web-push');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: '*',
    methods: ['GET', 'POST']
  }
});

// Middleware
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'data')));

// --- Web Push Setup (Auto-Generated VAPID Keys if missing) ---
let vapidKeys = {
  publicKey: process.env.PUBLIC_VAPID_KEY,
  privateKey: process.env.PRIVATE_VAPID_KEY
};

if (!vapidKeys.publicKey || !vapidKeys.privateKey) {
  console.log('Generating fresh VAPID Keys for Web Push Notifications...');
  const keys = webpush.generateVAPIDKeys();
  vapidKeys = {
    publicKey: keys.publicKey,
    privateKey: keys.privateKey
  };
}

webpush.setVapidDetails(
  'mailto:arise-safety@example.com',
  vapidKeys.publicKey,
  vapidKeys.privateKey
);

console.log('VAPID Public Key:', vapidKeys.publicKey);

// Global State
let latestSensorData = {
  suhu: 0,
  hum: 0,
  gas: 0,
  gasPercent: 0,
  api: false,
  level: 0,
  servo: 0,
  flamePos: -1,
  airQuality: 0,
  rssi: -50,
  freeHeap: 120000,
  uptime: 0
};
let lastNotificationLevel = 0;
let pushSubscriptions = [];

// Serve Webpage Routes
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'data', 'login.html'));
});

app.get('/dashboard', (req, res) => {
  res.sendFile(path.join(__dirname, 'data', 'index.html'));
});

app.get('/education', (req, res) => {
  res.sendFile(path.join(__dirname, 'data', 'education.html'));
});

// ESP32 Updates Endpoint
app.post('/api/update', (req, res) => {
  const data = req.body;
  if (!data) {
    return res.status(400).send('No data provided');
  }

  // Update State
  latestSensorData = { ...latestSensorData, ...data, uptime: Math.floor(Date.now() / 1000) };

  // Broadcast to Socket.io Web Clients
  io.emit('sensor-update', latestSensorData);

  // Trigger Push Notifications on Escalation
  const currentLevel = parseInt(latestSensorData.level || 0);
  if (currentLevel > lastNotificationLevel && currentLevel >= 1) {
    const threatNames = ['NORMAL', 'WASPADA', 'BAHAYA', 'KRITIS'];
    const title = `🚨 ALARM ARISE: STATUS ${threatNames[currentLevel]}!`;
    let body = `Terdeteksi tingkat ancaman ${threatNames[currentLevel]}. `;
    
    if (latestSensorData.api) {
      body += `🔥 TITIK API TERDETEKSI pada koordinat ${latestSensorData.flamePos}°! Segera evakuasi!`;
    } else {
      body += `Parameter sensor (Suhu: ${latestSensorData.suhu}°C, Gas: ${latestSensorData.gasPercent}%) tidak normal. Harap periksa area!`;
    }

    sendPushNotification(title, body);
  }
  
  lastNotificationLevel = currentLevel;
  res.status(200).send('OK');
});

// REST Endpoint for Compatibility/Polling
app.get('/api/status', (req, res) => {
  res.json(latestSensorData);
});

// mock config endpoints to prevent errors
app.post('/saveblynkconfig', (req, res) => res.send('Blynk configuration saved (Hosted)'));
app.post('/webpassconfig', (req, res) => res.send('Credentials updated (Hosted)'));
app.post('/savewifi', (req, res) => res.send('WiFi configuration saved (Hosted)'));
app.get('/calibrate', (req, res) => res.send('Gas sensor calibration triggered (Hosted)'));
app.get('/logout', (req, res) => res.redirect('/'));

// --- Web Push Endpoints ---
app.get('/api/vapid-public-key', (req, res) => {
  res.json({ publicKey: vapidKeys.publicKey });
});

app.post('/api/subscribe', (req, res) => {
  const subscription = req.body;
  
  // Filter out duplicates
  const exists = pushSubscriptions.find(sub => sub.endpoint === subscription.endpoint);
  if (!exists) {
    pushSubscriptions.push(subscription);
    console.log('New browser subscribed to push notifications. Total:', pushSubscriptions.length);
  }
  
  res.status(201).json({ message: 'Subscribed successfully!' });
});

// Broadcast Push function
function sendPushNotification(title, body) {
  const payload = JSON.stringify({ title, body });
  
  console.log(`Broadcasting Push Notification to ${pushSubscriptions.length} devices: "${title}"`);
  
  const promises = pushSubscriptions.map(subscription => {
    return webpush.sendNotification(subscription, payload)
      .catch(err => {
        if (err.statusCode === 410 || err.statusCode === 404) {
          // Subscription expired or removed
          pushSubscriptions = pushSubscriptions.filter(sub => sub.endpoint !== subscription.endpoint);
          console.log('Cleaned up expired subscription.');
        } else {
          console.error('Error sending push notification:', err);
        }
      });
  });

  return Promise.all(promises);
}

// Socket.io Connection
io.on('connection', (socket) => {
  console.log('Client connected to real-time socket:', socket.id);
  
  // Instantly send latest data upon connecting
  socket.emit('sensor-update', latestSensorData);
  
  socket.on('disconnect', () => {
    console.log('Client disconnected:', socket.id);
  });
});

// Port configuration
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`ARISE IoT Server is running on port ${PORT}`);
});
