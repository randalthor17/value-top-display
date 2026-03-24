# value-top-display

Linux binary for the ValueTop Tempest DF6 case segmented LCD display (VID:PID `5131:2007`).

Displays CPU temperature, CPU cooler RPM, and GPU temperature in real time.

## Install
```bash
brew tap randalthor17/lcd-display
brew install lcd-display
```

## Usage
```bash
lcd-display
```

## Permissions

A udev rule is installed automatically. If the device isn't accessible:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Known Limitations

RPM values 2560–2599 and 7660–7679 cannot be displayed due to a display firmware quirk.
