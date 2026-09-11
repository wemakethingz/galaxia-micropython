# stress_test.py -- Test intensif Wi-Fi + peripheriques (Galaxia / ESP32-S2)
# But : valider les buffers Wi-Fi/TLS reduits et traquer fuite/fragmentation RAM.
# Arret : bouton A, toucher le bouton tactile Nord, ou Ctrl-C.

import gc
import time
import network
import socket
import ssl

from thingz import led, accelerometer, compass, sound, display, temperature
from thingz import button_a, touch_n

# ----------------- Configuration -----------------
WIFI_SSID = "Freebox-0BCFF8"
WIFI_PASS = "746nzq47n7qhdzfvqk4cfn"

# Endpoints "generate_204" : reponse HTTP 204 vide, concus pour etre interroges
# en boucle (checks de connectivite) -> ideal pour un stress test.
HTTP_HOST = "connectivitycheck.gstatic.com"   # HTTP simple (port 80)
HTTP_PATH = "/generate_204"
HTTPS_HOST = "www.google.com"                 # HTTPS/TLS (port 443) -> stresse mbedTLS
HTTPS_PATH = "/generate_204"
TLS_EVERY = 5                  # une requete HTTPS toutes les N iterations
ENABLE_SOUND = False           # True pour tester aussi le buzzer
LOOP_DELAY = 0.2               # pause entre iterations (s)
MAX_ITER = 0                   # 0 = illimite

# ----------------- Etat / stats -----------------
stats = {
    "iter": 0,
    "http_ok": 0, "http_err": 0,
    "tls_ok": 0, "tls_err": 0,
    "wifi_reconnect": 0,
    "min_free": 1 << 30,
    "start": 0,
}


def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print("Wi-Fi: connexion...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        t0 = time.ticks_ms()
        while not wlan.isconnected():
            if time.ticks_diff(time.ticks_ms(), t0) > 20000:
                raise OSError("Wi-Fi: timeout de connexion")
            time.sleep(0.3)
    print("Wi-Fi OK :", wlan.ifconfig()[0])
    return wlan


def http_get(host, path="/", port=80, use_tls=False):
    """GET minimal en socket brut ; lit toute la reponse. Retourne les octets lus."""
    ai = socket.getaddrinfo(host, port)[0][-1]
    s = socket.socket()
    s.settimeout(10)
    try:
        s.connect(ai)
        if use_tls:
            s = ssl.wrap_socket(s, server_hostname=host)
        req = "GET {} HTTP/1.0\r\nHost: {}\r\nConnection: close\r\n\r\n".format(path, host)
        s.write(req.encode())
        total = 0
        while True:
            chunk = s.read(512)
            if not chunk:
                break
            total += len(chunk)
        return total
    finally:
        try:
            s.close()
        except Exception:
            pass


def read_peripherals(i):
    """Lit/actionne les capteurs et actionneurs (hors reseau)."""
    # LED : cycle de couleurs
    phase = i % 3
    led.set_colors(255 if phase == 0 else 0,
                   255 if phase == 1 else 0,
                   255 if phase == 2 else 0)

    out = {}
    try:
        out["accel"] = accelerometer.get_values()
    except Exception as e:
        out["accel"] = "err:%s" % e
    try:
        out["heading"] = compass.heading()
    except Exception as e:
        out["heading"] = "err:%s" % e
    try:
        out["temp"] = temperature()
    except Exception as e:
        out["temp"] = "err:%s" % e
    try:
        out["light"] = led.read_light_level()
    except Exception as e:
        out["light"] = "err:%s" % e

    if ENABLE_SOUND and (i % 10 == 0):
        try:
            sound.set_volume(20)
            sound.play(True, 440)
            time.sleep(0.05)
            sound.play(False, 0)
        except Exception:
            pass
    return out


def update_display(free):
    try:
        display.raw.print(0, 0, "STRESS #%d" % stats["iter"])
        display.raw.print(0, 16, "RAM libre:%d" % free)
        display.raw.print(0, 32, "HTTP %d/%d" % (stats["http_ok"], stats["http_err"]))
        display.raw.print(0, 48, "TLS  %d/%d" % (stats["tls_ok"], stats["tls_err"]))
        display.raw.show()
    except Exception as e:
        print("display err:", e)


def should_stop():
    try:
        if button_a.was_pressed():
            return True
    except Exception:
        pass
    try:
        if touch_n.was_touched():
            return True
    except Exception:
        pass
    return False


def main():
    stats["start"] = time.ticks_ms()
    wlan = connect_wifi()

    while True:
        stats["iter"] += 1
        i = stats["iter"]
        gc.collect()

        # --- Reconnexion Wi-Fi si perte ---
        if not wlan.isconnected():
            stats["wifi_reconnect"] += 1
            print("Wi-Fi perdu -> reconnexion")
            try:
                connect_wifi()
            except Exception as e:
                print("reconnexion echouee:", e)
                time.sleep(2)
                continue

        # --- Reseau : HTTP a chaque tour, HTTPS periodiquement ---
        try:
            n = http_get(HTTP_HOST, HTTP_PATH, 80, use_tls=False)
            stats["http_ok"] += 1
        except Exception as e:
            stats["http_err"] += 1
            print("HTTP err:", e)

        if TLS_EVERY and (i % TLS_EVERY == 0):
            try:
                gc.collect()
                n = http_get(HTTPS_HOST, HTTPS_PATH, 443, use_tls=True)
                stats["tls_ok"] += 1
            except Exception as e:
                stats["tls_err"] += 1
                print("TLS err:", e)

        # --- Peripheriques ---
        periph = read_peripherals(i)

        # --- RAM ---
        gc.collect()
        free = gc.mem_free()
        alloc = gc.mem_alloc()
        if free < stats["min_free"]:
            stats["min_free"] = free

        update_display(free)

        up = time.ticks_diff(time.ticks_ms(), stats["start"]) // 1000
        print("#%d up=%ds free=%d (min=%d) alloc=%d | HTTP %d/%d TLS %d/%d reconn=%d | T=%s L=%s head=%s" % (
            i, up, free, stats["min_free"], alloc,
            stats["http_ok"], stats["http_err"],
            stats["tls_ok"], stats["tls_err"], stats["wifi_reconnect"],
            periph.get("temp"), periph.get("light"), periph.get("heading"),
        ))

        if should_stop():
            print("Arret demande (bouton).")
            break
        if MAX_ITER and i >= MAX_ITER:
            print("MAX_ITER atteint.")
            break

        time.sleep(LOOP_DELAY)

    # Bilan
    print("=== BILAN ===")
    print("Iterations :", stats["iter"])
    print("HTTP ok/err :", stats["http_ok"], "/", stats["http_err"])
    print("TLS  ok/err :", stats["tls_ok"], "/", stats["tls_err"])
    print("Reconnexions Wi-Fi :", stats["wifi_reconnect"])
    print("RAM libre min observee :", stats["min_free"])
    led.set_colors(0, 0, 0)


try:
    main()
except KeyboardInterrupt:
    print("Interrompu (Ctrl-C).")
    try:
        led.set_colors(0, 0, 0)
    except Exception:
        pass
