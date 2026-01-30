Name:           streamlinux
Version:        0.2.0
Release:        1.alpha%{?dist}
Summary:        Stream your Linux screen to Android devices with WebRTC

License:        MIT
URL:            https://github.com/MrVanguardia/streamlinux
Source0:        %{name}-%{version}.tar.gz

# Binary package - contains pre-built Go signaling server
%global debug_package %{nil}

# Build requirements
BuildRequires:  python3-devel >= 3.10
BuildRequires:  python3-setuptools
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

# Runtime requirements
Requires:       python3 >= 3.10
Requires:       python3-gobject >= 3.42
Requires:       python3-pillow >= 9.0
Requires:       python3-qrcode >= 7.3
Requires:       python3-websocket-client >= 1.4
Requires:       python3-cryptography >= 38.0
Requires:       gstreamer1 >= 1.20
Requires:       gstreamer1-plugins-base >= 1.20
Requires:       gstreamer1-plugins-good >= 1.20
Requires:       gstreamer1-plugins-bad-free >= 1.20
Requires:       gtk4 >= 4.8
Requires:       libadwaita >= 1.2

# Recommended for hardware encoding
Recommends:     gstreamer1-vaapi
Recommends:     libva-utils

%description
StreamLinux allows you to stream your Linux desktop screen and audio to Android
devices over your local network with ultra-low latency using WebRTC technology.

*** ALPHA RELEASE - For testing purposes only ***

Features:
- Low latency screen streaming using WebRTC
- Hardware-accelerated video encoding (VAAPI, NVENC)
- High-quality audio streaming with Opus codec
- Support for both X11 and Wayland desktops
- QR code for easy mobile connection
- Modern GTK4/libadwaita interface
- Multi-language support (English/Spanish)
- Secure token-based authentication
- PIN-based device authorization
- Rate limiting and security hardening

Security (v0.2.0-alpha):
- TLS 1.2/1.3 with modern ciphers
- PBKDF2 100,000 iterations
- SSRF/injection protection
- Secure file permissions

%prep
%autosetup -n %{name}-%{version}

%build
# Python - no compilation needed

%install
rm -rf %{buildroot}

# Create directories
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_datadir}/streamlinux
install -d %{buildroot}%{_datadir}/applications
install -d %{buildroot}%{_datadir}/icons/hicolor/scalable/apps
install -d %{buildroot}%{_datadir}/metainfo
install -d %{buildroot}%{_libdir}/streamlinux
install -d %{buildroot}%{_userunitdir}

# Install launcher script
cat > %{buildroot}%{_bindir}/streamlinux << 'EOF'
#!/bin/bash
# StreamLinux Launcher v0.2.0-alpha
export PYTHONPATH="${PYTHONPATH}:/usr/share/streamlinux"
cd /usr/share/streamlinux
exec python3 streamlinux_gui.py "$@"
EOF
chmod 755 %{buildroot}%{_bindir}/streamlinux

# Install Python modules
install -m 644 streamlinux_gui.py %{buildroot}%{_datadir}/streamlinux/
install -m 644 webrtc_streamer.py %{buildroot}%{_datadir}/streamlinux/
install -m 644 i18n.py %{buildroot}%{_datadir}/streamlinux/
install -m 644 portal_screencast.py %{buildroot}%{_datadir}/streamlinux/
install -m 644 usb_manager.py %{buildroot}%{_datadir}/streamlinux/
install -m 644 security.py %{buildroot}%{_datadir}/streamlinux/

# Install desktop file
install -m 644 data/com.streamlinux.host.desktop %{buildroot}%{_datadir}/applications/

# Install icon
install -m 644 data/icons/streamlinux.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/

# Install metainfo
install -m 644 data/com.streamlinux.host.metainfo.xml %{buildroot}%{_datadir}/metainfo/

# Install signaling server
install -m 755 signaling-server %{buildroot}%{_libdir}/streamlinux/

# Install systemd user service
cat > %{buildroot}%{_userunitdir}/streamlinux-signaling.service << 'EOF'
[Unit]
Description=StreamLinux Signaling Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/lib64/streamlinux/signaling-server -port 8080
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/com.streamlinux.host.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml || true

%post
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
update-desktop-database %{_datadir}/applications &>/dev/null || :

%postun
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
update-desktop-database %{_datadir}/applications &>/dev/null || :

%files
%license LICENSE
%doc README.md
%{_bindir}/streamlinux
%{_datadir}/streamlinux/
%{_datadir}/applications/com.streamlinux.host.desktop
%{_datadir}/icons/hicolor/scalable/apps/streamlinux.svg
%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml
%{_libdir}/streamlinux/signaling-server
%{_userunitdir}/streamlinux-signaling.service

%changelog
* Wed Jan 29 2026 Vanguardia Studio <contact@vanguardiastudio.us> - 0.2.0-1.alpha
- Security: Fixed 14 vulnerabilities from penetration test
- Security: TLS 1.2/1.3 enforced with modern ciphers
- Security: PBKDF2 with 100,000 iterations
- Security: Rate limiting (10 attempts/minute/IP)
- Security: SSRF and GStreamer injection protection
- Security: Secure file permissions (0600/0700)
- Security: Token authentication enforced
- Added systemd user service for signaling server
- Improved error handling and logging

* Sat Jan 25 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 0.1.1-1
- Added security module with PIN-based device authorization
- Cryptographically secure session tokens
- Rate limiting for connection attempts

* Fri Jan 24 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 0.1.0-1
- Initial alpha release
