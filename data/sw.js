self.addEventListener('push', function(event) {
  let data = { title: 'Peringatan ARISE IoT!', body: 'Deteksi kondisi darurat pada sistem.' };
  if (event.data) {
    try {
      data = event.data.json();
    } catch (e) {
      data.body = event.data.text();
    }
  }

  const options = {
    body: data.body,
    icon: '/logo.png',
    badge: '/logo.png',
    vibrate: [200, 100, 200, 100, 200, 100, 400],
    data: {
      url: '/dashboard'
    },
    tag: 'arise-fire-alert', // Prevents notification stacking
    renotify: true,
    actions: [
      { action: 'open', title: 'Buka Dashboard' }
    ]
  };

  event.waitUntil(
    self.registration.showNotification(data.title, options)
  );
});

self.addEventListener('notificationclick', function(event) {
  event.notification.close();
  
  const targetUrl = '/dashboard';
  
  event.waitUntil(
    clients.matchAll({ type: 'window', includeUncontrolled: true }).then(function(clientList) {
      // If a tab is already open, focus it
      for (let i = 0; i < clientList.length; i++) {
        let client = clientList[i];
        if (client.url.includes(targetUrl) && 'focus' in client) {
          return client.focus();
        }
      }
      // Otherwise, open a new window/tab
      if (clients.openWindow) {
        return clients.openWindow(targetUrl);
      }
    })
  );
});
