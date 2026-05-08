# 💬 Real-Time Web Chat & Location Tracker

## 📖 Deskripsi

Web Chat adalah aplikasi *real-time messaging* berbasis *client-server* yang mengimplementasikan komunikasi dua arah secara langsung melalui browser. Keunikan dari proyek ini adalah penggunaan **bahasa C murni** untuk membangun **WebSocket Server** dari nol (tanpa *framework* backend modern), yang kemudian diintegrasikan dengan antarmuka *frontend* interaktif menggunakan HTML, CSS, dan Vanilla JavaScript.

Selain fitur obrolan (*chatting*), proyek ini juga dilengkapi dengan fitur pelacakan/pembaruan lokasi pengguna (*location sharing*) yang berjalan pada server terpisah.

## ✨ Fitur Utama

- ⚡ **Real-Time Communication:** Pengiriman dan penerimaan pesan secara instan menggunakan protokol WebSocket (*full-duplex*).
- 📍 **Location Tracking:** Pembaruan dan penyebaran informasi lokasi pengguna secara *real-time* melalui server terpisah (`server_location`).
- 🗄️ **JSON Data Storage:** Penyimpanan data pesan, pengguna, dan lokasi secara persisten dalam format file `.json` (`chats.json`, `users.json`, `locations.json`).
- 🖥️ **Interactive Web UI:** Antarmuka pengguna yang responsif dan mudah digunakan untuk pengalaman *chat* yang mulus.

## 👥 Informasi Pengembang

**Politeknik Negeri Bandung**

- **Anggota Tim:**
  - Umar Faruq Robbany (231524028)
  - Fredy Kurniadi (231524010)

## 🛠️ Teknologi yang Digunakan

- **Backend / Server:** C (Membangun WebSocket & TCP Socket Server secara *low-level*)
- **Frontend / Client:** HTML5, CSS3, Vanilla JavaScript
- **Penyimpanan Data:** JSON (File I/O)

## 📂 Struktur Proyek

```text
web-chat/
├── assets/
│   ├── css/style.css       # Gaya tampilan antarmuka chat
│   └── js/script.js        # Logika client-side dan koneksi WebSocket frontend
├── data/
│   ├── chats.json          # Database berbasis file untuk riwayat chat
│   ├── locations.json      # Database untuk riwayat lokasi
│   └── users.json          # Database untuk data pengguna terdaftar
├── index.html              # Halaman utama antarmuka pengguna
├── server_chat.c           # Program server utama untuk menangani pesan chat
├── server_location.c       # Program server khusus untuk menangani data lokasi
├── websocket.c             # Modul implementasi protokol WebSocket (Handshake, Framing)
└── websocket.h             # Header file untuk modul WebSocket
```

## 🚀 Instalasi dan Cara Penggunaan

### Prasyarat
Pastikan sistem operasi Anda (Linux/Windows dengan MinGW) telah terpasang *compiler* C seperti `gcc` untuk melakukan kompilasi program server.

### Langkah-langkah Menjalankan Program

**1. Kompilasi Server**
Buka Terminal/Command Prompt, lalu kompilasi *source code* C untuk masing-masing server:

Untuk Server Chat:
```bash
gcc server_chat.c websocket.c -o server_chat
```

Untuk Server Lokasi:
```bash
gcc server_location.c websocket.c -o server_location
```

**2. Jalankan Server**
Jalankan *executable* file yang baru saja dikompilasi di dua terminal yang berbeda.
```bash
# Terminal 1
./server_chat

# Terminal 2
./server_location
```
*(Server akan mulai berjalan dan mendengarkan koneksi pada port yang telah ditentukan di dalam kode C).*

**3. Buka Aplikasi di Browser**
Buka file `index.html` menggunakan browser modern (Chrome, Firefox, Edge, dll). Anda dapat membuka file ini secara langsung (`file:///.../index.html`) atau menyajikannya menggunakan ekstensi seperti *Live Server* di VSCode.
