(function () {
  "use strict";

  const logEl = document.getElementById("log");
  const form = document.getElementById("composer");
  const input = document.getElementById("msgInput");
  const statusDot = document.getElementById("statusDot");
  const peerCount = document.getElementById("peerCount");

  // Each phone picks a short random handle the first time it connects and
  // remembers it for the session (not persisted - refresh gets a new one).
  const myHandle = "misafir-" + Math.floor(1000 + Math.random() * 9000);

  let ws = null;
  let reconnectTimer = null;

  function addLine(sender, text, kind) {
    const row = document.createElement("div");
    row.className = "line" + (kind ? " " + kind : "");

    const senderEl = document.createElement("span");
    senderEl.className = "sender";
    senderEl.textContent = kind === "system" ? "*" : sender + ":";

    const textEl = document.createElement("span");
    textEl.className = "text";
    textEl.textContent = text;

    row.appendChild(senderEl);
    row.appendChild(textEl);
    logEl.appendChild(row);
    logEl.scrollTop = logEl.scrollHeight;
  }

  function setOnline(isOnline) {
    statusDot.classList.toggle("online", isOnline);
  }

  function connect() {
    const proto = location.protocol === "https:" ? "wss://" : "ws://";
    ws = new WebSocket(proto + location.host + "/ws");

    ws.onopen = function () {
      setOnline(true);
      addLine("", "baglanti kuruldu (" + myHandle + " olarak)", "system");
    };

    ws.onclose = function () {
      setOnline(false);
      addLine("", "baglanti koptu, yeniden deneniyor...", "system");
      clearTimeout(reconnectTimer);
      reconnectTimer = setTimeout(connect, 2000);
    };

    ws.onerror = function () {
      ws.close();
    };

    ws.onmessage = function (evt) {
      try {
        const msg = JSON.parse(evt.data);
        const kind = msg.sender === "system" ? "system" : (msg.sender === myHandle ? "self" : "");
        addLine(msg.sender, msg.text, kind);
      } catch (e) {
        // Non-JSON payload - show it raw rather than silently dropping it.
        addLine("?", String(evt.data), "system");
      }
    };
  }

  form.addEventListener("submit", function (e) {
    e.preventDefault();
    const text = input.value.trim();
    if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;

    ws.send(JSON.stringify({ sender: myHandle, text: text }));
    input.value = "";
  });

  // The ESP32 doesn't currently push a live peer count over the socket;
  // this keeps the badge honest instead of showing a fake number.
  peerCount.textContent = "canli";

  connect();
})();
