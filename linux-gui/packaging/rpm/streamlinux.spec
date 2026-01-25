Name:           streamlinux
Version:        1.0.0
Release:        1%{?dist}
Summary:        Stream your Linux screen to Android devices

License:        MIT
URL:            https://github.com/streamlinux
Source0:        %{name}-%{version}.tar.gz

BuildArch:      noarch
BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
BuildRequires:  desktop-file-utils

Requires:       python3
Requires:       python3-gobject
Requires:       python3-pillow
Requires:       python3-qrcode
Requires:       python3-websockets
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

%prep
%autosetup

%build
# Nothing to build for Python

%install
# Install main script
install -D -m 755 streamlinux_gui.py "%{buildroot}%{_bindir}/streamlinux-gui"

# Install webrtc streamer module
install -D -m 644 webrtc_streamer.py "%{buildroot}%{_datadir}/streamlinux/webrtc_streamer.py"

# Install desktop file
install -D -m 644 data/com.streamlinux.host.desktop "%{buildroot}%{_datadir}/applications/com.streamlinux.host.desktop"

# Install icon
install -D -m 644 data/icons/streamlinux.svg "%{buildroot}%{_datadir}/icons/hicolor/scalable/apps/streamlinux.svg"

# Install metainfo
install -D -m 644 data/com.streamlinux.host.metainfo.xml "%{buildroot}%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml"

%check
desktop-file-validate "%{buildroot}%{_datadir}/applications/com.streamlinux.host.desktop"

%files
%{_bindir}/streamlinux-gui
%{_datadir}/streamlinux/webrtc_streamer.py
%{_datadir}/applications/com.streamlinux.host.desktop
%{_datadir}/icons/hicolor/scalable/apps/streamlinux.svg
%{_datadir}/metainfo/com.streamlinux.host.metainfo.xml

%changelog
* Sat Jan 24 2026 StreamLinux Team <contact@streamlinux.dev> - 1.0.0-1
- Initial release
