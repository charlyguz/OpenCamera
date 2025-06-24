# OpenCamera

Implementación de acceso a cámara desde cero en Windows y Linux utilizando programación de bajo nivel.

## 📋 Descripción

Este proyecto demuestra cómo activar y controlar la cámara del sistema sin depender de frameworks de alto nivel. Se enfoca en la comunicación directa con los controladores de hardware y las APIs del sistema operativo para lograr el acceso completo a dispositivos de captura de video.

## 🔧 Arquitectura Técnica

### Acceso a la Cámara en Linux (`camaraLinux.c`)

El archivo `camaraLinux.c` utiliza las siguientes tecnologías de bajo nivel:

- Video4Linux2 (V4L2): Interfaz estándar del kernel de Linux para dispositivos de video
- Llamadas al sistema: Acceso directo a `/dev/video*` para comunicarse con el controlador de la cámara
- Memory mapping (mmap): Mapeo de memoria para transferencia eficiente de frames
- IOCTL: Comandos de control de entrada/salida para configurar parámetros de captura
- File descriptors: Manejo directo de descriptores de archivo del dispositivo

### Acceso a la Cámara en Windows (`camaraWin.cpp`)

El archivo `camaraWin.cpp` implementa:

- DirectShow API: Interfaz de Microsoft para captura multimedia
- COM (Component Object Model): Comunicación con componentes del sistema
- Media Foundation: Framework moderno de Windows para multimedia
- Kernel32: Acceso a funciones de bajo nivel del sistema
- Win32 API: Interfaz nativa de Windows para control de hardware

## 🚀 Compilación

### Para Linux:
```bash
gcc -D_GNU_SOURCE -Wall -O2 -o camaraLinux camaraLinux.c -lX11
```

Parámetros explicados:
- `-D_GNU_SOURCE`: Habilita extensiones GNU para acceso completo a APIs del sistema
- `-Wall`: Activa todas las advertencias del compilador para código más robusto
- `-O2`: Optimización de código para mejor rendimiento
- `-lX11`: Enlaza con X11 para manejo de ventanas y display del sistema gráfico

### Para Windows:
```bash
g++ camaraWin.cpp -o camara.exe -DUNICODE -D_UNICODE -lmfplat -lmf -lmfreadwrite -lmfuuid -lole32 -lgdi32 -luser32 -luuid
```

Parámetros explicados:
- `-DUNICODE -D_UNICODE`: Soporte completo para caracteres Unicode
- `-lmfplat -lmf -lmfreadwrite -lmfuuid`: Media Foundation APIs para multimedia
- `-lole32`: Component Object Model para comunicación entre componentes
- `-lgdi32 -luser32`: APIs gráficas nativas de Windows
- `-luuid`: Identificadores únicos universales para componentes COM

## 🔍 Funcionamiento Interno

### Proceso de Activación de Cámara

#### 1. Enumeración de Dispositivos
- Escaneo automático de dispositivos de video disponibles en el sistema
- Identificación de capacidades específicas de cada cámara
- Detección de formatos de video soportados (YUV, RGB, MJPEG)
- Verificación de resoluciones disponibles

#### 2. Inicialización del Dispositivo
- Apertura del canal de comunicación directa con el hardware
- Configuración de parámetros de captura (resolución, formato, FPS)
- Establecimiento de la comunicación con el controlador del dispositivo
- Validación de permisos y disponibilidad del hardware

#### 3. Configuración de Buffers
- Asignación de memoria para almacenamiento temporal de frames
- Configuración de cola de buffers para captura continua
- Implementación de double buffering para evitar pérdida de frames
- Optimización de memoria para transferencias de alta velocidad

#### 4. Activación del Stream
- Inicio del flujo de datos desde el sensor de la cámara
- Configuración de callbacks para procesamiento de frames en tiempo real
- Establecimiento de sincronización entre captura y procesamiento
- Activación de interrupciones de hardware para notificaciones

### Técnicas de Optimización Implementadas

- Zero-copy operations: Minimiza copias de memoria innecesarias entre buffers
- Buffer pooling: Reutilización eficiente de memoria para reducir fragmentación
- Asynchronous I/O: Operaciones no bloqueantes para mejor rendimiento
- Hardware acceleration: Aprovecha capacidades del GPU cuando está disponible
- Direct memory access: Acceso directo a memoria del dispositivo

## 📁 Estructura del Proyecto

```
opencamera/
├── README.md           # Documentación principal del proyecto
├── LICENSE            # Licencia MIT del proyecto
├── .gitignore         # Archivos ignorados por Git
├── camaraLinux.c      # Implementación para sistemas Linux
└── camaraWin.cpp      # Implementación para sistemas Windows
```

## 🛠️ Requisitos del Sistema

### Linux:
- Kernel: Linux 2.6+ con soporte V4L2 habilitado
- Controladores: Controladores de cámara instalados y funcionando
- Permisos: Acceso de lectura/escritura a `/dev/video*`
- Dependencias: X11 development headers (`libx11-dev`)

### Windows:
- Sistema Operativo: Windows 7 o superior (recomendado Windows 10+)
- Runtime: Visual C++ Redistributable instalado
- Controladores: Controladores de cámara compatibles con DirectShow/Media Foundation
- Compilador: MinGW-w64 o Visual Studio Build Tools

## 🚀 Uso

### Compilación y Ejecución en Linux:
```bash
# Instalar dependencias (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential libx11-dev

# Compilar el proyecto
gcc -D_GNU_SOURCE -Wall -O2 -o camaraLinux camaraLinux.c -lX11

# Agregar permisos de cámara (si es necesario)
sudo usermod -a -G video $USER

# Ejecutar (puede requerir reiniciar sesión)
./camaraLinux
```

### Compilación y Ejecución en Windows:
```bash
# Compilar con MinGW
g++ camaraWin.cpp -o camara.exe -DUNICODE -D_UNICODE -lmfplat -lmf -lmfreadwrite -lmfuuid -lole32 -lgdi32 -luser32 -luuid

# Ejecutar (puede requerir permisos de administrador)
./camara.exe
```

## 🔧 Resolución de Problemas

### Errores Comunes en Linux:

`Permission denied` al acceder a /dev/video0:
```bash
# Solución 1: Agregar usuario al grupo video
sudo usermod -a -G video $USER
# Reiniciar sesión después

# Solución 2: Cambiar permisos temporalmente
sudo chmod 666 /dev/video0
```

`Device or resource busy`:
```bash
# Verificar procesos usando la cámara
lsof /dev/video0

# Cerrar aplicaciones que usen la cámara
sudo pkill -f "cheese|vlc|firefox"
```

`No such file or directory` para /dev/video0:
```bash
# Verificar dispositivos disponibles
ls -la /dev/video*

# Verificar módulos del kernel
lsmod | grep uvcvideo
```

### Errores Comunes en Windows:

`COM initialization failed`:
- Ejecutar el programa como administrador
- Verificar que no haya otras aplicaciones usando la cámara

`Device not found`:
- Abrir Device Manager y verificar que la cámara esté instalada
- Actualizar controladores de la cámara
- Verificar que la cámara no esté deshabilitada

`Access denied`:
- Verificar configuración de privacidad de Windows
- Permitir acceso a cámara para aplicaciones de escritorio

## 🧪 Pruebas y Validación

### Verificar Funcionamiento:

Linux:
```bash
# Verificar que la cámara es detectada
v4l2-ctl --list-devices

# Probar captura básica
v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=1
```

Windows:
```cmd
# Verificar dispositivos en PowerShell
Get-PnpDevice -Class Camera

# Probar con aplicación nativa
start ms-settings:privacy-webcam
```

## 🤝 Contribuciones

Las contribuciones son bienvenidas y apreciadas. Para contribuir:

1. Fork el repositorio
2. Crea una rama para tu feature (`git checkout -b feature/nueva-funcionalidad`)
3. Commit tus cambios (`git commit -am 'Agregar nueva funcionalidad'`)
4. Push a la rama (`git push origin feature/nueva-funcionalidad`)
5. Abre un Pull Request con descripción detallada

### Áreas de Mejora:
- Soporte para más formatos de video
- Implementación de controles de cámara (zoom, enfoque, exposición)
- Optimizaciones de rendimiento
- Soporte para múltiples cámaras simultáneas
- Interfaz gráfica de usuario

## 📄 Licencia

Este proyecto está bajo la Licencia MIT - ver el archivo [LICENSE](LICENSE) para detalles completos.

## 🎯 Objetivos de Aprendizaje

Este proyecto está diseñado para enseñar:

- Programación de sistemas: Acceso directo a hardware y APIs de bajo nivel
- Comunicación con hardware: Interfaces entre software y dispositivos físicos
- Programación multiplataforma: Desarrollo para diferentes sistemas operativos
- Optimización de rendimiento: Técnicas para manejo eficiente de multimedia
- Arquitectura de controladores: Comprensión de la comunicación con el kernel

## 🔗 Referencias Técnicas

- [Video4Linux2 API Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)
- [DirectShow Programming Guide](https://docs.microsoft.com/en-us/windows/win32/directshow/directshow-programming-guide)
- [Media Foundation Documentation](https://docs.microsoft.com/en-us/windows/win32/medfound/microsoft-media-foundation-sdk)

## 📊 Estado del Proyecto

- ✅ Implementación básica para Linux
- ✅ Implementación básica para Windows  
- ✅ Documentación completa
- 🔄 Pruebas en diferentes distribuciones Linux
- 🔄 Optimizaciones de rendimiento
- ⏳ Interfaz gráfica de usuario
- ⏳ Soporte para streaming en red

---

Desarrollado con ❤️ para aprender sobre programación de sistemas y acceso a hardware
```

Este README completo incluye:

- ✅ Descripción técnica detallada de cómo funciona el acceso a la cámara
- ✅ Instrucciones de compilación con explicación de parámetros
- ✅ Guía de resolución de problemas específica para cada plataforma
- ✅ Documentación del proceso interno de activación de cámara
- ✅ Requisitos del sistema claramente especificados
- ✅ Ejemplos de uso prácticos
- ✅ Información para contribuidores
- ✅ Referencias técnicas para profundizar

