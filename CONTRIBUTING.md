# Contributing to StreamLinux

First off, thank you for considering contributing to StreamLinux! 🎉

This project is in **early alpha development** and we genuinely welcome all kinds of contributions.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Project Structure](#project-structure)
- [Submitting Changes](#submitting-changes)
- [Style Guidelines](#style-guidelines)

## Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive environment. Be kind, be patient, and be helpful.

## How Can I Contribute?

### 🐛 Reporting Bugs

Before creating a bug report, please check if the issue already exists. When creating a bug report, include:

- **Clear title** describing the issue
- **Steps to reproduce** the problem
- **Expected behavior** vs actual behavior
- **System information**: Linux distribution, desktop environment (GNOME/KDE/etc), Wayland or X11
- **Logs**: Run the application from terminal and include any error messages

### 💡 Suggesting Features

Feature suggestions are welcome! Please:

- Check if the feature has already been suggested
- Describe the feature and why it would be useful
- Consider if it aligns with the project's scope

### 🔒 Security Audits

**We especially welcome security reviews!** This codebase has NOT been professionally audited. If you have security expertise:

- Review the WebRTC signaling implementation
- Check for potential vulnerabilities in the network code
- Review the GStreamer pipeline configuration
- Report findings via [SECURITY.md](SECURITY.md)

### 📝 Documentation

Documentation improvements are always helpful:

- Fix typos or unclear explanations
- Add examples or tutorials
- Translate documentation to other languages
- Improve code comments

### 💻 Code Contributions

We accept code contributions for:

- Bug fixes
- Performance improvements
- New features (please discuss first)
- Test coverage
- Refactoring

## Development Setup

### Prerequisites

**Linux Host (Python GUI)**:
```bash
# Fedora/RHEL
sudo dnf install python3 python3-gobject gtk4 libadwaita gstreamer1 gstreamer1-plugins-bad-free

# Ubuntu/Debian
sudo apt install python3 python3-gi gir1.2-gtk-4.0 gir1.2-adw-1 gstreamer1.0-tools gstreamer1.0-plugins-bad
```

**Signaling Server (Go)**:
```bash
# Install Go 1.21+
# Then:
cd signaling-server
go mod download
```

**Android Client**:
- Android Studio (latest)
- Android SDK 34+
- NDK (if doing native work)

### Running Locally

```bash
# Clone the repository
git clone https://github.com/MrVanguardia/streamlinux.git
cd streamlinux

# Linux GUI
cd linux-gui
pip install -r requirements.txt
python streamlinux_gui.py

# Signaling Server
cd signaling-server
go run ./cmd/server

# Android
# Open android-client/ in Android Studio and run
```

## Project Structure

```
streamlinux/
├── linux-gui/              # Python GTK4 application
│   ├── streamlinux_gui.py  # Main application
│   ├── webrtc_streamer.py  # WebRTC/GStreamer logic
│   ├── portal_screencast.py # Screen capture via D-Bus
│   └── i18n.py             # Internationalization
├── signaling-server/       # Go WebSocket server
│   ├── cmd/server/         # Main entry point
│   └── internal/           # Internal packages
├── android-client/         # Kotlin Android app
│   └── app/src/main/       # Source code
└── linux-host/             # C++ host (work in progress)
```

## Submitting Changes

### Pull Request Process

1. **Fork** the repository
2. **Create a branch** for your changes: `git checkout -b feature/my-feature`
3. **Make your changes** with clear, atomic commits
4. **Test** your changes thoroughly
5. **Submit a Pull Request** with a clear description

### Commit Messages

Use clear, descriptive commit messages:

```
feat: add hardware encoder detection for VAAPI

- Check for vainfo availability
- Fall back to software encoding if unavailable
- Add user notification for encoder selection
```

Prefixes:
- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation changes
- `refactor:` - Code refactoring
- `test:` - Adding tests
- `chore:` - Maintenance tasks

## Style Guidelines

### Python (Linux GUI)

- Follow PEP 8
- Use type hints where practical
- Document public functions with docstrings
- Maximum line length: 100 characters

### Go (Signaling Server)

- Follow standard Go conventions
- Run `go fmt` before committing
- Use meaningful variable names
- Handle errors explicitly

### Kotlin (Android)

- Follow Kotlin coding conventions
- Use Kotlin idioms (null safety, data classes, etc.)
- Prefer Compose over XML layouts

## 🙏 A Note on AI-Assisted Development

This project was largely developed with AI assistance. We're transparent about this because:

1. The code may have patterns that experienced developers would do differently
2. There may be edge cases or best practices that were missed
3. Security and performance may benefit from expert human review

**Your contributions help make this project better!** Whether you're fixing a small bug or doing a comprehensive code review, every contribution matters.

## Questions?

If you have questions about contributing, feel free to:

- Open a GitHub issue with the "question" label
- Start a discussion in GitHub Discussions (if enabled)

Thank you for contributing! 🙌
