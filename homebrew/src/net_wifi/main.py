"""WLAN Status – echtes Python, wird zu .hb transpiliert."""

def main():
    clear()
    color("CYAN")
    center(25, "WLAN Status")
    color("WHITE")
    wifi()
    color("GREEN")
    center(190, "Touch = Zurueck")
    wait_touch()

if __name__ == "__main__":
    main()
