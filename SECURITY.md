# Security Policy

## ⚠️ Important Disclaimer

**StreamLinux is alpha software that has NOT been professionally security audited.**

While we use standard security mechanisms (WebRTC DTLS/SRTP), we cannot make guarantees about the security of this software. Please consider this when deciding whether to use StreamLinux and in what contexts.

## Security Model

### What We Use

StreamLinux relies on the following security mechanisms provided by underlying libraries:

| Component | Security Mechanism | Notes |
|:----------|:-------------------|:------|
| WebRTC Media | DTLS + SRTP | Provided by GStreamer webrtcbin |
| Signaling | WebSocket | No TLS by default (local network assumption) |
| Authentication | None | QR code contains connection info |

### Assumptions

StreamLinux is designed with the following assumptions:

1. **Trusted local network**: The software assumes you're using it on a trusted WiFi network
2. **Physical proximity**: QR code scanning requires physical access to both devices
3. **No malicious actors**: We don't defend against man-in-the-middle on the signaling path

### Known Limitations

- **Signaling is not encrypted by default**: The WebSocket connection between devices uses plain WebSocket, not WSS (TLS)
- **No authentication mechanism**: Anyone who can access the signaling server and intercept the QR code could potentially connect
- **No client verification**: The host doesn't verify the identity of connecting clients
- **Depends on library security**: Security ultimately depends on GStreamer, OpenSSL, and libwebrtc versions

## Reporting a Vulnerability

If you discover a security vulnerability, please:

### Do

1. **Report privately** by emailing  I studiovanguardia3@gmail.com or opening a private security advisory on GitHub
2. **Provide details**: Include steps to reproduce, potential impact, and affected versions
3. **Give us time**: Allow reasonable time (30-90 days) to address the issue before public disclosure

### Don't

- **Don't open a public issue** for security vulnerabilities
- **Don't exploit** vulnerabilities beyond what's needed to demonstrate them
- **Don't access** other users' data or systems

## What to Expect

Given the early stage of this project:

1. **We may not have immediate fixes**: We're a small project and may take time to address issues
2. **We may need help**: If you report an issue and can suggest a fix, that's very welcome
3. **We'll be transparent**: We'll acknowledge the issue and keep you updated on progress

## Recommendations for Users

Until this software matures and receives proper security review:

1. **Use only on trusted networks** - Don't use on public WiFi or untrusted networks
2. **Don't stream sensitive content** - Assume the stream could potentially be intercepted
3. **Keep software updated** - Install updates promptly
4. **Monitor network** - If you're security-conscious, monitor network traffic for anomalies

## Security Improvements We'd Welcome

If you have security expertise and want to contribute, we'd appreciate:

- [ ] Code review of `webrtc_streamer.py` WebRTC implementation
- [ ] Code review of `signaling-server/` Go signaling code
- [ ] Review of Android app network code
- [ ] Adding TLS support to the signaling server
- [ ] Implementing proper authentication/pairing
- [ ] Security hardening recommendations
- [ ] Penetration testing results (reported privately)

## Scope

This security policy covers:

- The StreamLinux Linux GUI (`linux-gui/`)
- The signaling server (`signaling-server/`)
- The Android client (`android-client/`)

This does NOT cover:

- Third-party dependencies (GStreamer, WebRTC libraries, etc.)
- The operating system or device security
- Network infrastructure

## Version Support

As an alpha project, we currently only support the latest release. We recommend always using the most recent version.

| Version | Supported |
|:--------|:----------|
| 0.1.x (alpha) | ✅ Current |
| < 0.1.0 | ❌ Not supported |

---

Thank you for helping keep StreamLinux and its users safe!
