# Security Policy

## ⚠️ Alpha Software Disclaimer

**StreamLinux is currently in ALPHA stage.** This means:

- The software may contain bugs, vulnerabilities, or unexpected behavior
- Features may be incomplete or change without notice
- **Use at your own risk** - we are not responsible for any damages, data loss, or security issues that may arise from using this software
- This software is provided "AS IS" without warranty of any kind

By downloading, installing, or using StreamLinux, you acknowledge and accept these terms.

---

## 🔒 Security Features

StreamLinux implements several security measures to protect your data:

| Feature | Description |
|---------|-------------|
| **Local Network Only** | All streaming occurs over your local network (LAN). No data is sent to external servers. |
| **End-to-End Encryption** | Media streams are encrypted using DTLS-SRTP (WebRTC standard). |
| **Token Authentication** | QR code pairing uses cryptographically secure tokens (CSPRNG). |
| **Token Rotation** | Authentication tokens expire after 60 seconds and are automatically rotated. |
| **HMAC-SHA256 Signatures** | All tokens are signed to prevent tampering. |
| **No Data Collection** | We do not collect, store, or transmit any personal data or usage analytics. |
| **Open Source** | All code is publicly available for review and audit. |

---

## 🔐 Privacy

### What data does StreamLinux access?

- **Screen content**: Only when you explicitly grant permission via the system dialog
- **System audio**: Only when you enable audio streaming
- **Local network**: To communicate between your Linux computer and Android device

### What data does StreamLinux NOT collect?

- ❌ Personal information
- ❌ Usage statistics or analytics
- ❌ Crash reports (unless you manually send them)
- ❌ Location data
- ❌ Device identifiers
- ❌ Any data transmitted to external servers

**Your screen content and audio are streamed directly to your device over your local network. Nothing leaves your network.**

---

## 🛡️ Reporting Security Vulnerabilities

We take security seriously. If you discover a security vulnerability in StreamLinux, please report it responsibly.

### How to Report

**DO NOT** create a public GitHub issue for security vulnerabilities.

Instead, please send a detailed report to:

📧 **studiovanguardia3@gmail.com**

### What to Include

- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Your suggested fix (if any)
- Your name/handle for credit (optional)

### What to Expect

- We will acknowledge your report within 48 hours
- We will investigate and work on a fix
- We will credit you in the release notes (unless you prefer anonymity)
- We ask that you do not publicly disclose the vulnerability until we have released a fix

---

## 📋 Supported Versions

| Version | Supported | Notes |
|---------|-----------|-------|
| 0.2.x (alpha) | ✅ | Current development version |
| < 0.2.0 | ❌ | No longer supported |

---

## ⚖️ Liability

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.

IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 📬 Contact

For security concerns: **studiovanguardia3@gmail.com**

For general questions: Open a [GitHub Discussion](https://github.com/MrVanguardia/streamlinux/discussions)

---

<div align="center">

**StreamLinux** by [Vanguardia Studio](https://vanguardiastudio.us)

</div>
