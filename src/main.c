// VID:PID = 0x5131:0x2007
// The Schema:
// buf[0] = Report ID (0x00)
// buf[1] = Padding
// buf[2] = CPU Temperature
// buf[3] to buf[5] = Padding
// buf[6] to buf[7] = Big-endian 16-bit CPU Cooler RPM
// buf[8] to buf[9] = Padding
// buf[10] = GPU Temperature
// buf[11] onwards = Padding
//
// The RPM Value has to be patched because of quirks
// The behavior shown is the following:
// tens_ones = i % 100
// i < 2560:         direct (no offset)
// 2560 <= i < 6000: +40 if tens_ones < 60, else -60
// 6000 <= i < 7680: +80 if tens_ones < 20, else -20
// else:             +20 if tens_ones < 80, else -80
//
// As such, even if patched, 2560-2599 and 7660-7679 values
// cannot be printed.

#include <libusb-1.0/libusb.h>
#include <sensors/sensors.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define VENDOR_ID 0x5131
#define PRODUCT_ID 0x2007
#define REPORT_SIZE 63
#define VERSION "1.0.0"

/* ------------------------------------------------------------------ */
/* RPM patch                                                           */
/* ------------------------------------------------------------------ */

int patch_rpm(int desired) {
  if (desired < 2560)
    return desired;

  int candidate;

  candidate = desired - 40;
  if (candidate >= 2560 && candidate < 6000 && (candidate % 100) < 60)
    return candidate;
  candidate = desired + 60;
  if (candidate >= 2560 && candidate < 6000 && (candidate % 100) >= 60)
    return candidate;

  candidate = desired - 80;
  if (candidate >= 6000 && candidate < 7680 && (candidate % 100) < 20)
    return candidate;
  candidate = desired + 20;
  if (candidate >= 6000 && candidate < 7680 && (candidate % 100) >= 20)
    return candidate;

  candidate = desired - 20;
  if (candidate >= 7680 && candidate < 10000 && (candidate % 100) < 80)
    return candidate;
  candidate = desired + 80;
  if (candidate >= 7680 && candidate < 10000 && (candidate % 100) >= 80)
    return candidate;

  return desired;
}

/* ------------------------------------------------------------------ */
/* Sensor reading via libsensors                                       */
/* ------------------------------------------------------------------ */

typedef struct {
  double cpu_temp;
  int cpu_rpm;
  double gpu_temp;
} sensor_data_t;

static int is_cpu_chip(const char *prefix) {
  return strcmp(prefix, "k10temp") == 0 || strcmp(prefix, "coretemp") == 0;
}

static int is_gpu_chip(const char *prefix) {
  return strcmp(prefix, "amdgpu") == 0 || strcmp(prefix, "nouveau") == 0 ||
         strcmp(prefix, "nvidia") == 0;
}

static int is_fan_chip(const char *prefix) {
  return !is_cpu_chip(prefix) && !is_gpu_chip(prefix);
}

sensor_data_t read_sensors(void) {
  sensor_data_t data = {.cpu_temp = 0, .cpu_rpm = 0, .gpu_temp = 0};

  const sensors_chip_name *chip;
  int chip_nr = 0;

  while ((chip = sensors_get_detected_chips(NULL, &chip_nr)) != NULL) {
    const sensors_feature *feature;
    int feat_nr = 0;

    while ((feature = sensors_get_features(chip, &feat_nr)) != NULL) {
      const sensors_subfeature *sub;
      double val;

      if (feature->type == SENSORS_FEATURE_TEMP &&
          (is_cpu_chip(chip->prefix) || is_gpu_chip(chip->prefix))) {
        sub = sensors_get_subfeature(chip, feature,
                                     SENSORS_SUBFEATURE_TEMP_INPUT);
        if (!sub)
          continue;
        if (sensors_get_value(chip, sub->number, &val) < 0)
          continue;

        if (is_cpu_chip(chip->prefix) && data.cpu_temp == 0)
          data.cpu_temp = val;
        else if (is_gpu_chip(chip->prefix) && data.gpu_temp == 0)
          data.gpu_temp = val;
      }

      if (feature->type == SENSORS_FEATURE_FAN && is_fan_chip(chip->prefix)) {
        sub =
            sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_FAN_INPUT);
        if (!sub)
          continue;
        if (sensors_get_value(chip, sub->number, &val) < 0)
          continue;

        if (data.cpu_rpm == 0 && (int)val > 0)
          data.cpu_rpm = (int)val;
      }
    }
  }

  return data;
}

/* ------------------------------------------------------------------ */
/* HID report                                                          */
/* ------------------------------------------------------------------ */

int send_report(libusb_device_handle *dev, uint8_t cpu_temp, int cpu_rpm,
                uint8_t gpu_temp) {
  uint8_t buf[REPORT_SIZE];
  memset(buf, 0, sizeof(buf));

  int patched = patch_rpm(cpu_rpm);

  buf[1] = cpu_temp;              /* CPU temperature  */
  buf[5] = (patched >> 8) & 0xFF; /* RPM high byte    */
  buf[6] = patched & 0xFF;        /* RPM low byte     */
  buf[9] = gpu_temp;              /* GPU temperature  */

  int transferred = 0;
  int r = libusb_interrupt_transfer(dev, 0x01, buf, REPORT_SIZE, &transferred,
                                    1000);
  if (r < 0) {
    fprintf(stderr, "Transfer error: %s\n", libusb_error_name(r));
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    printf("%s %s\n", argv[0], VERSION);
    return 0;
  }

  if (sensors_init(NULL) != 0) {
    fprintf(stderr, "Failed to init libsensors\n");
    return 1;
  }

  libusb_context *ctx = NULL;
  libusb_init(&ctx);

  libusb_device_handle *dev =
      libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
  if (!dev) {
    fprintf(stderr, "Device not found\n");
    fprintf(stderr, "If permissions are the issue, install the udev rule:\n");
    fprintf(stderr, "  sudo cp /etc/udev/rules.d/99-value-top-display.rules "
                    "/etc/udev/rules.d/\n");
    fprintf(stderr,
            "  sudo udevadm control --reload-rules && sudo udevadm trigger\n");
    sensors_cleanup();
    libusb_exit(ctx);
    return 1;
  }

  if (libusb_kernel_driver_active(dev, 0))
    libusb_detach_kernel_driver(dev, 0);
  libusb_claim_interface(dev, 0);

  while (1) {
    sensor_data_t s = read_sensors();

    printf("CPU: %.0f°C  RPM: %d  GPU: %.0f°C\n", s.cpu_temp, s.cpu_rpm,
           s.gpu_temp);

    send_report(dev, (uint8_t)s.cpu_temp, s.cpu_rpm, (uint8_t)s.gpu_temp);
    sleep(1);
  }

  libusb_release_interface(dev, 0);
  libusb_close(dev);
  libusb_exit(ctx);
  sensors_cleanup();
  return 0;
}
