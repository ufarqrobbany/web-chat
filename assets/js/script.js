let socket;
let locationSocket;
let map;

// DOM Elements
const connectButton = document.getElementById("connect-button");
const disconnectButton = document.getElementById("disconnect-button");
const inputUsername = document.getElementById("username-input");
const messagesDiv = document.getElementById("messages");
const inputMessage = document.getElementById("message-input");
const sendMessageButton = document.getElementById("send-message-button");

let markers = {}; // Store markers for connected users

// Initialize Map
map = L.map('map').setView([-6.871382, 107.571098], 17);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
}).addTo(map);

// WebSocket Connection
function connect() {
    const username = inputUsername.value.trim();
    if (!username) {
        appendMessage("Username cannot be empty!", "error");
        return;
    }

    socket = new WebSocket("wss://174.138.26.104:8080");
    locationSocket = new WebSocket("wss://139.59.101.121:8080");;


    socket.onopen = () => {
        appendMessage("Connected to the server.", "info");

        // Send username to the server
        const connectMessage = { type: "connect", username };
        socket.send(JSON.stringify(connectMessage));

        updateButtonVisibility(true);
        sendMessageButton.disabled = false;
    };

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);

        if(data.type == "error") {
            appendMessage(data.message, "error");
        } else if(data.type == "announcement") {
            appendMessage(`${data.username} telah bergabung!`, "info");
        } else {
            appendMessage(`<span class="username">${data.username}</span> ${data.message} <span class="time">${data.time}</span>`, "user");
        }
    };

    socket.onclose = () => {
        appendMessage("Disconnected from the server.", "info");
        updateButtonVisibility(false);
        sendMessageButton.disabled = true;
    };

    socket.onerror = (error) => {
        appendMessage("Error: " + error.message, "error");
    };

    // Location socket connection
    locationSocket.onopen = () => {
        // Update client's own location on the map
        if (navigator.geolocation) {
            navigator.geolocation.getCurrentPosition((position) => {
                const lat = position.coords.latitude;
                const lng = position.coords.longitude;
                updateMarker(username, lat, lng);

                // Send the initial location to the server
                const locationMessage = {username, lat, lon: lng };
                locationSocket.send(JSON.stringify(locationMessage));
            });
        }

        // Watch for location changes and update the server
        navigator.geolocation.watchPosition((position) => {
            const lat = position.coords.latitude;
            const lng = position.coords.longitude;

            // Send the updated location to the server
            const locationMessage = { username, lat, lon: lng };
            locationSocket.send(JSON.stringify(locationMessage));

            // Update marker on map
            updateMarker(username, lat, lng);
        }, (error) => {
            console.error("Error getting location: ", error);
        });
    };

    locationSocket.onmessage = (event) => {
        const data = JSON.parse(event.data);
        const { username, lat, lon } = data;
        updateMarker(username, lat, lon);
    };

    locationSocket.onerror = (error) => {
        appendMessage("Location error: " + error.message, "error");
    };
}

function disconnect() {
    if (socket) {
        socket.close();
        appendMessage("Disconnected from the server.", "info");
        updateButtonVisibility(false);
        sendMessageButton.disabled = true;
    }
}

// Update Map Marker for User
function updateMarker(username, lat, lng) {
    if (markers[username]) {
        map.removeLayer(markers[username]);
    }

    const marker = L.marker([lat, lng]).addTo(map).bindPopup(`${username}`);
    markers[username] = marker;

    // Adjust the map view to show all markers
    const group = L.featureGroup(Object.values(markers));
    map.fitBounds(group.getBounds());
}

// Send Message with time
function sendMessage() {
    const message = inputMessage.value.trim();
    const username = inputUsername.value.trim();

    if (message && socket && socket.readyState === WebSocket.OPEN) {
        const chatMessage = {
            type: "message",
            username,
            message,
            time: new Date().toLocaleTimeString("en-GB") // Add current time
        };
        socket.send(JSON.stringify(chatMessage));

        // Display message with time inside a <span> element
        appendMessage(`${message} <span class="time">${chatMessage.time}</span>`, "my-message");
        inputMessage.value = '';
    } else {
        console.log("Cannot send message, socket is not open or message is empty");
    }
}

// Append Message to Chat
function appendMessage(message, type = "message") {
    const newMessage = document.createElement("div");

    if (type === "info") {
        newMessage.className = "info-message";
    } else if (type === "error") {
        newMessage.className = "error-message";
    } else if (type === "user") {
        newMessage.className = "user-message";
    } else {
        newMessage.className = "my-message";
    }

    // Insert the message content with HTML (time is a <span>)
    newMessage.innerHTML = message;

    messagesDiv.appendChild(newMessage);
    messagesDiv.scrollTop = messagesDiv.scrollHeight; // Auto-scroll to bottom
}

// Update Button Visibility
function updateButtonVisibility(isConnected) {
    if (isConnected) {
        connectButton.style.display = "none";
        disconnectButton.style.display = "inline-block";
    } else {
        connectButton.style.display = "inline-block";
        disconnectButton.style.display = "none";
    }
}

updateButtonVisibility(false);

// Input Validation
function validateUsername() {
    const username = inputUsername.value.trim();
    connectButton.disabled = !username;
}

inputUsername.addEventListener("input", validateUsername);

function validateMessage() {
    const message = inputMessage.value.trim();
    sendMessageButton.disabled = !message;
}

inputMessage.addEventListener("input", validateMessage);
