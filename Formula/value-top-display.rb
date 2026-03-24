class ValueTopDisplay < Formula
  desc "Driver for value-top case LCD display (VID:PID 5131:2007)"
  homepage "https://github.com/randalthor17/value-top-display"
  url "https://github.com/randalthor17/value-top-display/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "d51c55573ec027b7d27b3fc3955bd8f7b6efdaee0c5e16bae17606339efa2a45"
  license "MIT"

  depends_on "pkg-config" => :build  # MUST have this to use pkg-config in Makefile
  depends_on "libusb"
  depends_on "lm-sensors"

  def install
    # Ensure pkg-config can find the .pc files for your dependencies
    ENV.append_path "PKG_CONFIG_PATH", "#{Formula["libusb"].opt_lib}/pkgconfig"
    ENV.append_path "PKG_CONFIG_PATH", "#{Formula["lm-sensors"].opt_lib}/pkgconfig"

    # Compile
    system "make", "PREFIX=#{prefix}"

    # Install using Homebrew's directory structure
    # Avoid writing to /etc or ~/.config during 'make install'
    bin.install "value-top-display"
    etc.install "udev/99-value-top-display.rules"
    
    # Handle systemd files manually to keep them in the Cellar
    (prefix/"share/systemd/user").install "systemd/value-top-display.service"
  end
end

  def caveats
    <<~EOS
      To allow access to the display without sudo, install the udev rule:
        sudo cp #{etc}/udev/99-value-top-display.rules /etc/udev/rules.d/
        sudo udevadm control --reload-rules && sudo udevadm trigger

      This only needs to be done once.

      A systemd user service has been installed. To enable it:
        systemctl --user daemon-reload
        systemctl --user enable --now value-top-display

      To check status:
        systemctl --user status value-top-display
    EOS
  end

  test do
    assert_match "value-top-display #{version}", shell_output("#{bin}/value-top-display --version")
  end
end
