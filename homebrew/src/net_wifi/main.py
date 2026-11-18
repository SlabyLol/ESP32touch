"""WLAN Status – GUI mit echten Buttons."""

def main():
    clear()
    bg("BLACK")
    color("CYAN")
    box(8, 8, 304, 50)
    center(30, "WLAN Status")

    color("WHITE")
    wifi()

    color("YELLOW")
    button(1, 20, 180, 120, 40, "Refresh")
    button(2, 180, 180, 120, 40, "Zurueck")
    show()  # Touch-Loop bis Button

if __name__ == "__main__":
    main()
