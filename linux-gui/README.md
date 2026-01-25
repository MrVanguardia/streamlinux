# StreamLinux GUI

Aplicación de escritorio GTK4 para controlar el streaming de pantalla desde Linux a dispositivos Android.

## 🚀 Instalación Rápida (Todas las distros)

```bash
# 1. Descargar
wget https://github.com/MrVanguardia/streamlinux/releases/download/v1.0.0/streamlinux-1.0.0-linux-universal.tar.gz

# 2. Extraer
tar -xzf streamlinux-1.0.0-linux-universal.tar.gz

# 3. Entrar a la carpeta (se llama "streamlinux")
cd streamlinux

# 4. Instalar (IMPORTANTE: usar "bash", no "./")
sudo bash install.sh
```

**⚠️ IMPORTANTE:** 
- La carpeta se llama `streamlinux`, NO `streamlinux-1.0.0-linux`
- Usar `sudo bash install.sh`, NO `sudo ./install.sh`

### Si hay problemas:
```bash
# Diagnosticar
bash install.sh diagnose

# Instalar dependencias manualmente
pip3 install --user pillow qrcode websocket-client
```

## Características

- 🖥️ **Interfaz moderna** con GTK4 y libadwaita
- 📱 **Código QR** para conexión fácil con dispositivos Android
- 🎮 **Control completo** del streaming (iniciar, detener, configurar)
- 📊 **Estadísticas en tiempo real** (FPS, bitrate, latencia)
- ⚙️ **Configuración flexible** de video, audio y red
- 🔧 **Soporte para múltiples backends** (X11, Wayland, PipeWire)

## Captura de Pantalla

![StreamLinux GUI](docs/screenshot.png)

## Requisitos

### Sistema

- GTK4 >= 4.10
- libadwaita >= 1.4
- Python >= 3.10
- PyGObject

### Instalación de dependencias

**Fedora:**
```bash
sudo dnf install python3-gobject gtk4 libadwaita python3-pillow python3-qrcode
```

**Ubuntu/Debian:**
```bash
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1 python3-pil python3-qrcode
```

**Arch Linux:**
```bash
sudo pacman -S python-gobject gtk4 libadwaita python-pillow python-qrcode
```

## Instalación

### Desde código fuente

```bash
# Clonar o descargar
cd linux-gui

# Instalar dependencias Python
pip install -r requirements.txt

# Instalar localmente
sudo ./scripts/build.sh install

# O ejecutar directamente
python3 streamlinux_gui.py
```

### RPM (Fedora/RHEL)

```bash
# Construir RPM
./scripts/build.sh rpm

# Instalar
sudo dnf install build/streamlinux-1.0.0-1.fc*.noarch.rpm
```

### DEB (Debian/Ubuntu)

```bash
# Construir DEB
./scripts/build.sh deb

# Instalar
sudo dpkg -i build/streamlinux_1.0.0_all.deb
sudo apt-get install -f  # Resolver dependencias
```

### Flatpak

```bash
# Instalar runtime
flatpak install flathub org.gnome.Platform//46

# Construir e instalar
./scripts/build.sh flatpak
flatpak install build/streamlinux-1.0.0.flatpak
```

## Uso

1. **Iniciar la aplicación:**
   ```bash
   streamlinux-gui
   ```

2. **Configurar opciones** de video/audio según necesites

3. **Hacer clic en "Iniciar"** para comenzar el servidor de streaming

4. **Escanear el código QR** con la app de Android para conectar

5. **¡Listo!** Tu pantalla se está transmitiendo al dispositivo Android

## Estructura del Proyecto

```
linux-gui/
├── streamlinux_gui.py      # Aplicación principal
├── requirements.txt        # Dependencias Python
├── data/
│   ├── com.streamlinux.host.desktop    # Archivo .desktop
│   ├── com.streamlinux.host.metainfo.xml  # Metadatos AppStream
│   └── icons/
│       └── streamlinux.svg  # Icono de la app
├── packaging/
│   ├── rpm/
│   │   └── streamlinux.spec  # Spec file para RPM
│   └── flatpak/
│       └── com.streamlinux.host.yaml  # Manifest Flatpak
└── scripts/
    └── build.sh            # Script de construcción
```

## Desarrollo

### Ejecutar en modo desarrollo

```bash
python3 streamlinux_gui.py
```

### Construir todos los paquetes

```bash
./scripts/build.sh all
```

### Limpiar archivos de construcción

```bash
./scripts/build.sh clean
```

## Integración con linux-host

Esta GUI está diseñada para trabajar junto con el backend `linux-host` del proyecto StreamLinux. La GUI:

1. Inicia/detiene el binario `stream-linux` del host
2. Lee estadísticas del proceso de streaming
3. Gestiona el servidor de señalización
4. Genera códigos QR con la información de conexión

## Licencia

MIT License - Ver [LICENSE](../LICENSE) para más detalles.
