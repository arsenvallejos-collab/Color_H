# BomBap Color (v0.1)

Efecto de audio para darle color de cinta / cassette a tus samples y beats.
Formatos: **VST3** (Windows y Mac), **AU** (Mac) y Standalone.

## Controles

| Control | Que hace |
|---|---|
| **Mode** | `Tape` (calido, agudos suaves) o `Cassette` (mas oscuro, mas sucio, mas wow/flutter y mas siseo) |
| **Drive** | Saturacion de cinta |
| **Wow** | Variacion lenta del pitch (el "mareo" de una cinta gastada) |
| **Flutter** | Variacion rapida del pitch (temblor) |
| **Noise** | Siseo de cinta (hiss) |
| **Tone** | Oscuro <-> brillante (mueve el corte de agudos una octava arriba o abajo) |
| **Mix** | Seco / procesado |
| **Output** | Volumen de salida (-12 a +12 dB) |

El plugin informa 4 ms de latencia al programa; Ableton y FL Studio la compensan solos.

## Como obtener el plugin (sin instalar nada raro)

GitHub compila gratis las versiones de Windows **y** Mac por ti.

1. Crea una cuenta gratis en https://github.com
2. Crea un repositorio nuevo (boton **New**), llamalo `BomBapColor`, dejalo en **Private** si quieres.
3. Sube todo el contenido de esta carpeta (**Add file > Upload files**).
   - Si la carpeta oculta `.github` no se sube, crea el archivo a mano:
     **Add file > Create new file**, y como nombre escribe `.github/workflows/build.yml`
     (al escribir las `/` se crean las carpetas). Pega dentro el contenido del archivo `build.yml` incluido aqui.
4. Entra a la pestana **Actions**. Veras "Build plugin" corriendo. Tarda unos 10 a 15 minutos.
5. Cuando termine (circulo verde), abre esa ejecucion y baja los archivos de **Artifacts**:
   `BomBapColor-Windows` y `BomBapColor-Mac`.

## Instalacion

**Windows**
1. Descomprime `BomBapColor-Windows`. Dentro de `VST3` esta la carpeta `BomBap Color.vst3`.
2. Copiala a `C:\Program Files\Common Files\VST3\`
3. FL Studio: *Options > Manage plugins > Find installed plugins*.
   Ableton: *Preferences > Plug-Ins > Use VST3 System Folders* y pulsa *Rescan*.

**Mac**
1. Descomprime `BomBapColor-Mac`.
2. Copia `BomBap Color.vst3` a `/Library/Audio/Plug-Ins/VST3/`
   y `BomBap Color.component` (AU) a `/Library/Audio/Plug-Ins/Components/`
3. Como el plugin no esta firmado, macOS puede bloquearlo. Abre Terminal y pega:
   `xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/BomBap Color.vst3"`
   (haz lo mismo con el `.component` si lo usas).
4. Ableton: *Settings > Plug-ins > Use Audio Units / VST3* y *Rescan*.

## Para desarrolladores

- `Source/TapeEngine.h` : todo el sonido (sin dependencias, se prueba solo con g++).
- `Source/PluginProcessor.*` : conexion con JUCE (parametros, VST3/AU).
- `tools/engine_test.cpp` : pruebas automaticas del motor.
- `tools/render_demo.cpp` : genera audios de demostracion.

Compilar local: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
