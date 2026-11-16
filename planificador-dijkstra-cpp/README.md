# Planificador de Rutas con Dijkstra (Bogotá D.C.) en C++17

Aplicación de escritorio interactiva desarrollada en **C++17** para el curso de **Ciencias de la Computación I** de la **Universidad Distrital Francisco José de Caldas**.

El sistema modela **22 lugares emblemáticos de Bogotá** (incluyendo las sedes de la Universidad Distrital) y **38 corredores viales** como un grafo ponderado mediante **lista de adyacencia**, calcula la ruta óptima entre cualquier par de ubicaciones usando el **algoritmo de Dijkstra implementado desde cero** sobre un **Montículo Binario Mínimo (`MinHeap`) propio** con soporte `decrease-key` en tiempo $O((V + E) \log V)$, y permite visualizar tanto el mapa interactivo como la ejecución paso a paso (*snapshots*) del algoritmo.

---

## Estructura del Proyecto

```text
planificador-dijkstra-cpp/
├── CMakeLists.txt               # Configuración CMake con FetchContent (raylib, ImGui, rlImGui, json, Catch2)
├── README.md                    # Documentación técnica y guía de compilación
├── assets/
│   └── mapa_bogota.png          # Lienzo cartográfico de Bogotá (1280x720) con Bounding Box geográfico
├── data/
│   └── bogota.json              # Dataset real con 22 lugares de Bogotá y 38 corredores viales
├── include/
│   ├── Modelos.hpp              # Structs: Lugar, Arista, ResultadoRuta, PasoDijkstra, CriterioPeso
│   ├── Haversine.hpp            # Cálculo de distancia ortodrómica (km) y tiempo estimado (minutos)
│   ├── MinHeap.hpp              # Cola de prioridad propia (montículo binario mínimo) O((V+E) log V)
│   ├── Grafo.hpp                # Lista de adyacencia, carga desde JSON, activar/desactivar vías
│   └── Dijkstra.hpp             # Algoritmo de Dijkstra con grabación de instantáneas (snapshots)
├── src/
│   ├── main.cpp                 # Bucle principal raylib + rlImGui + interacción con el mapa
│   ├── Haversine.cpp
│   ├── MinHeap.cpp
│   ├── Grafo.cpp
│   └── Dijkstra.cpp
└── tests/
    └── test_dijkstra.cpp        # Suite de pruebas unitarias automatizadas en Catch2 (v3)
```

---

## Características Principales

1. **Estructuras de Datos Propia (`MinHeap` + `Grafo`)**:
   - `MinHeap`: Montículo binario mínimo indexado mediante vector de posiciones (`posicionEnHeap_`), permitiendo consultar pertenencia en $O(1)$ y ejecutar `insertar`, `extraerMinimo` y `disminuirPrioridad` (*decrease-key*) en $O(\log V)$.
   - `Grafo`: Lista de adyacencia (`std::vector<std::vector<Arista>>`) cargada dinámicamente desde `data/bogota.json`.
2. **Doble Criterio de Optimización**:
   - **Distancia más corta (`DISTANCIA_KM`)**: Minimiza la longitud vial acumulada en kilómetros.
   - **Menor tiempo estimado (`TIEMPO_MINUTOS`)**: Pondera cada vía según su distancia, velocidad promedio (`velocidad_kmh`) y multiplicador de congestión (`factor_trafico`):
     $$t_{\text{min}} = \left(\frac{d_{\text{km}}}{v_{\text{km/h}}}\right) \times 60 \times f_{\text{tráfico}}$$
3. **Visualizador Paso a Paso (*Snapshots* de Dijkstra)**:
   - Reproductor interactivo (`|< Inicio`, `< Anterior`, `Reproducir / Pausar`, `Siguiente >`, `Fin >|`) con control de velocidad y tabla en vivo de distancias tentativas $d[v]$ y predecesores $\pi[v]$.
4. **Simulación de Cierres Viales y Tráfico en Tiempo Real**:
   - Permite cerrar o habilitar cualquiera de las 38 vías de Bogotá (desde el panel lateral o haciendo clic derecho sobre la vía en el mapa) y ajustar el multiplicador de congestión vehicular.

---

## Requisitos y Compilación con CMake

El proyecto utiliza `FetchContent` de **CMake** (3.16+) para descargar y compilar automáticamente todas las dependencias (`raylib 5.5`, `Dear ImGui`, `rlImGui`, `nlohmann/json` y `Catch2 v3`) sin requerir instalaciones globales previas de librerías.

### 1. Configurar y compilar

```bash
cd planificador-dijkstra-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

*(En Windows con MSYS2 / MinGW-w64, si no tienes `cmake` instalado aún en MSYS2, puedes instalarlo con `pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`)*

### 2. Ejecutar la aplicación interactiva

```bash
./build/planificador_bogota
```

### 3. Ejecutar la suite de pruebas unitarias (Catch2)

```bash
ctest --test-dir build --output-on-failure
# o directamente:
./build/test_dijkstra
```

---

## Controles en el Mapa Interactivo

| Acción | Control |
| :--- | :--- |
| **Seleccionar Origen** | Clic izquierdo sobre un nodo en el mapa (o selector en panel lateral) |
| **Seleccionar Destino** | Clic derecho (o `Shift + Clic izquierdo`) sobre un nodo en el mapa |
| **Cerrar / Abrir una Vía** | Clic derecho sobre la línea de la vía en el mapa (o Checkbox en el panel) |
| **Acercar / Alejar (Zoom)** | Rueda del ratón sobre el mapa |
| **Desplazar Mapa (Pan)** | Arrastrar con el botón central del ratón (o `Alt + Clic derecho`) |
| **Restablecer Cámara** | Botón `Centrar Mapa` en el panel lateral |
