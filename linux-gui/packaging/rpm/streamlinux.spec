Name:           streamlinux
Version:        0.1.0
Release:        1.alpha%{?dist}
Summary:        Stream your Linux screen to Android devices

License:        MIT
URL:            https://github.com/MrVanguardia/streamlinux
Source0:        %{name}-%{version}.tar.gz

# Not noarch because we include the Go signaling-server binary
# Disable debug package generation (signaling-server is pre-built)
%global debug_package %{nil}

BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
BuildRequires:  desktop-file-utils

Requires:       python3
Requires:       python3-gobject
Requires:       python3-pillow
Requires:       python3-qrcode
Requires:       python3-websockets
Requires:       python3-cryptography
Requires:       gstreamer1
Requires:       gstreamer1-plugins-base
Requires:       gstreamer1-plugins-good
Requires:       gstreamer1-plugins-bad-free
Requires:       gtk4
Requires:       libadwaita

%description
StreamLinux allows you to stream your Linux desktop screen and audio to Android
devices over your local network with ultra-low latency using WebRTC technology.

Features:
- Low latency screen streaming using WebRTC
- Hardware-accelerated video encoding (VAAPI, NVENC)
- High-quality audio streaming with Opus codec
- Support for both X11 and Wayland desktops
- QR code for easy mobile connection
- Modern GTK4/libadwaita interface
- Multi-language support (English/Spanish)
- Token-based security for connections
- Improved streaming lifecycle (start/stop/restart)

%prep
%autosetup

%build
# Nothing to build for Python

%install
# Install main script
install -D -m 755 streamlinux_gui.py "%{buildroot}%{_bindir}/streamlinux-gui"

# Install Python modules
install -D -m 644 webrtc_streamer.py "%{buildroot}%{_datadir}/streamlinux/webrtc_streamer.py"
install -D -m 644 i18n.py "%{buildroot}%{_datadir}/streamlinux/i18n.py"
install -D -m 644 portal_screencast.py "%{buildroot}%{_datadir}/streamlinux/portal_screencast.py"
install -D -m 644 usb_manager.py "%{buildroot}%{_datadir}/streamlinux/usb_manager.py"
install -D -m 644 security.py "%{buildroot}%{_datadir}/streamlinux/security.py"

# Install desktop file
install -D -m 644 data/com.streamlinux.host.desktop "%{buildroot}%{_datadir}/applications/com.streamlinux.host.desktop"

# Install icon
install -D -m 644 data/icons/streamlinux.svg "%{buildroot}%{_datadir}/icons/hicolor/scalable/apps/streamlinux.svg"

# Install metainfo
install -D -m 644 data/com.streamlinux.host.metainfo.xml "%{buildroot}%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml"

# Install signaling server (Go binary)
install -D -m 755 signaling-server "%{buildroot}/usr/lib/signaling-server/signaling-server"

%check
desktop-file-validate "%{buildroot}%{_datadir}/applications/com.streamlinux.host.desktop"

%files
%{_bindir}/streamlinux-gui
%{_datadir}/streamlinux/webrtc_streamer.py
%{_datadir}/streamlinux/i18n.py
%{_datadir}/streamlinux/portal_screencast.py
%{_datadir}/streamlinux/usb_manager.py
%{_datadir}/streamlinux/security.py
%{_datadir}/applications/com.streamlinux.host.desktop
%{_datadir}/icons/hicolor/scalable/apps/streamlinux.svg
%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml
/usr/lib/signaling-server/signaling-server

%changelog
* Sat Jan 25 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 1.1.1-2
- Fixed: Include signaling server binary in RPM package
- Fixed: WebSocket connection error due to missing signaling server

* Sat Jan 25 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 1.1.1-1
- Added security module with PIN-based device authorization
- Cryptographically secure session tokens
- Rate limiting for connection attempts
- Device whitelist with trust management
- Automatic token expiration and refresh

* Sat Jan 25 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 1.0.1-1
- Added multi-language support (English/Spanish)
- Language auto-detection from system settings
- Language selector in preferences
- Fixed various UI strings

* Thu Jan 23 2025 Vanguardia Studio <contact@vanguardiastudio.us> - 1.0.0-1
- Initial release
