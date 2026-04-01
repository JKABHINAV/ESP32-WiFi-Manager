# ESP32 Dual WiFi Manager 📶

A robust, dual-network Wi-Fi manager for the ESP32, built using **ESP-IDF v4.4.7**. 

This project allows the ESP32 to act as both an Access Point (AP) and a Station (STA). It hosts a modern, interactive web interface directly from its internal SPIFFS memory, allowing users to seamlessly switch the ESP32 between two pre-configured Wi-Fi networks without ever needing to re-flash the board.

## ✨ Features
* **Dual-Mode Operation:** Runs an Access Point (`ESP32_Config`) and connects to a router simultaneously.
* **SPIFFS Storage:** Wi-Fi credentials and HTML files are stored securely in a custom 1MB flash partition.
* **Interactive Web UI:** A responsive, app-like web interface that polls the ESP32 via a custom `/status` JSON API to display real-time connection status.
* **Auto-Fallback:** If both saved networks are unavailable, the ESP32 safely falls back to AP-only mode so you don't lose access.

## 📂 Project Structure
* `main/` - Contains the C-code (`main.c`) and CMake configurations.
* `spiffs_image/` - The folder flashed to memory.
  * `html/index.html` - The frontend UI.
  * `wifi/wifi1.txt` & `wifi2.txt` - The network credentials.
* `partitions.csv` - Custom partition table allocating 1.5MB for the app and 1MB for SPIFFS storage.
* `sdkconfig.defaults` - Forces the compiler to use 4MB flash and the custom partition table.

## 🛠️ Setup & Installation
1. Clone this repository.
2. Navigate to `spiffs_image/wifi/` and update `wifi1.txt` and `wifi2.txt` with your actual SSIDs (Line 1) and Passwords (Line 2).
3. Open the ESP-IDF terminal in the project root.
4. Run the build and flash commands:
   ```bash
   idf.py build
   idf.py flash monitor

   ## 🚀 How to Use

**1. Power On & Connect**
* Plug in your ESP32. It will automatically start broadcasting its own Wi-Fi network.
* On your smartphone or computer, open your Wi-Fi settings and connect to the ESP32's network:
  * **Network Name (SSID):** `ESP32_Config`
  * **Password:** `12345678`
* *(Note: If your phone warns you that there is "No Internet Connection," tell it to stay connected).*

**2. Access the Dashboard**
* Open any web browser (Chrome, Safari, Edge).
* Type **`http://192.168.4.1`** into the address bar and press Enter.
* You will be greeted by the ESP32 Control Center dashboard.

**3. Read the Live Status**
The dashboard features an auto-updating status indicator:
* 🔴 **Red Dot ("Disconnected / Setup Mode"):** The ESP32 is currently acting only as an Access Point and is not connected to the internet.
* 🟢 **Green Dot ("Connected: [Network Name]"):** The ESP32 is successfully connected to your home router or mobile hotspot.

**4. Switch Networks**
* Click either the **Connect to WiFi 1** or **Connect to WiFi 2** button.
* The dashboard will immediately display a "Switching..." screen. 
* Behind the scenes, the ESP32 will disconnect from its current network and attempt to connect to the new one.
* After 3 seconds, the page will automatically redirect you back to the home screen, and the status dot will turn green once the new connection is established!
