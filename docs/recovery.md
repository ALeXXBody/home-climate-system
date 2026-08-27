# Reviving a dead / bootlooping board over USB

Use this when the board does not appear on the network at all
(no ping, no web UI) — e.g. after a bad firmware flash.

Everything you need is on GitHub:

> **Download:** [Recovery release assets](https://github.com/ALeXXBody/home-climate-system/releases/tag/recovery-v1.4.9)

| File | What it is |
|---|---|
| `hcs-recovery-c3-merged.bin` | **Single file** — bootloader + partition table + recovery app. Flash at `0x0`. Use this. |
| `hcs-recovery-c3-firmware.bin` | App only, flash at `0x10000` (not needed if you use the merged file) |

The recovery image contains **no OpenTherm, no 1-Wire, no WS2812/RMT,
no WiFiManager** — none of the code paths that caused the 1.4.6–1.4.8
bootloops. It never reboots itself.

## What the recovery image does

1. Clears the stuck `unclean_boots` NVS counter
2. Connects to your saved WiFi (SSID/password from NVS are preserved)
   — if that fails it opens an AP `HCS-Recovery-XXXX` (password `homeclimate`)
3. Serves `http://<device-ip>/` with a **web upload form** to flash the
   full firmware back over LAN
4. Prints an alive/heap line every 5 s on serial (115200)

## Step 1 — Flash the recovery image (USB)

Install esptool on your machine:

```bash
pip install esptool
```

Hold **BOOT** on the board, plug in USB, release BOOT once esptool says
"Connecting...". Then:

```bash
# wipe the old (possibly corrupt) flash — settings will be re-entered
esptool erase_flash

# flash the single merged recovery image
esptool write_flash 0x0 hcs-recovery-c3-merged.bin
```

(`erase_flash` wipes WiFi settings too; if you want to **keep** them,
skip `erase_flash` and only run the `write_flash` line.)

## Step 2 — Confirm it is alive

```bash
# PlatformIO: pio device monitor -b 115200
# or any serial terminal at 115200 baud
```

You should see:

```
=== HCS RECOVERY 1.4.9 ===
[rec] unclean_boots: <old> -> 0
[rec] node: hcs-xxxxxxxxxxxx
[rec] connecting to '<your-ssid>'.....
[rec] WiFi up: 192.168.x.y (-XX dBm)
[rec] HTTP :80 up — open the IP above to re-flash
[rec] alive up=5s heap=...
[rec] alive up=10s heap=...
```

Uptime must keep climbing. If it does, the **board is fine**.

If the serial output shows nothing at all or keeps resetting even with
this image → hardware problem (USB cable/power/OT shield short), not firmware.

## Step 3 — Flash the full firmware back

Once recovery is running you have two options:

**A. Over LAN (easiest):** open `http://<device-ip>/` shown on serial,
upload `firmware-lolin_c3_mini.bin` from the latest
[HCS release](https://github.com/ALeXXBody/home-climate-system/releases)
through the web form.

**B. Over USB:**

```bash
esptool write_flash 0x10000 firmware-lolin_c3_mini.bin
```

## Building the recovery image yourself

```bash
git clone https://github.com/ALeXXBody/home-climate-system
cd home-climate-system/firmware
pio run -d recovery -t upload          # build + flash via PlatformIO
pio device monitor -d recovery -b 115200
```

The recovery project is fully standalone (`firmware/recovery/`) and
shares no code with the main firmware.
