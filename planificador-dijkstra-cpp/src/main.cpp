#include "Dijkstra.hpp"
#include "Grafo.hpp"
#include "Haversine.hpp"
#include "Modelos.hpp"

#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using namespace planificador;

constexpr int ANCHO_VENTANA_INICIAL = 1400;
constexpr int ALTO_VENTANA_INICIAL = 800;
constexpr float ANCHO_PANEL_LATERAL = 390.0f;
constexpr float ALTO_HERRAMIENTAS = 88.0f;
constexpr float ALTO_EVENTOS = 104.0f;
constexpr float ANCHO_MAPA_BASE = 1280.0f;
constexpr float ALTO_MAPA_BASE = 720.0f;

/**
 * @brief Busca un archivo en rutas relativas habituales (ejecución desde raíz o desde build/).
 */
std::string resolverRutaArchivo(const std::string& rutaRelativa) {
    const std::vector<std::string> prefijos = {
        "",
        GetApplicationDirectory(),
        "../",
        "../../",
        "planificador-dijkstra-cpp/",
        "../planificador-dijkstra-cpp/"
    };
    for (const auto& prefijo : prefijos) {
        const std::string candidata = prefijo + rutaRelativa;
        if (std::filesystem::exists(candidata)) {
            return candidata;
        }
    }
    return rutaRelativa;
}

/**
 * @brief Proyecta una coordenada geográfica (lat, lon) a coordenadas de píxel sobre el lienzo 1280x720.
 */
Vector2 proyectarCoordenada(double lat, double lon, const BoundingBoxGeo& box) {
    const double margenX = 55.0;
    const double margenY = 45.0;
    const double anchoUtil = static_cast<double>(ANCHO_MAPA_BASE) - 2.0 * margenX;
    const double altoUtil = static_cast<double>(ALTO_MAPA_BASE) - 2.0 * margenY;

    const double normX = (lon - box.lonMin) / (box.lonMax - box.lonMin);
    const double normY = (box.latMax - lat) / (box.latMax - box.latMin);

    return Vector2{
        static_cast<float>(margenX + normX * anchoUtil),
        static_cast<float>(margenY + normY * altoUtil)
    };
}

/**
 * @brief Calcula la distancia de un punto P al segmento AB.
 */
float distanciaPuntoSegmento(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab = Vector2Subtract(b, a);
    const Vector2 ap = Vector2Subtract(p, a);
    const float lenSq = Vector2LengthSqr(ab);
    if (lenSq <= 0.0001f) {
        return Vector2Distance(p, a);
    }
    const float t = std::clamp(Vector2DotProduct(ap, ab) / lenSq, 0.0f, 1.0f);
    const Vector2 proyeccion = Vector2Add(a, Vector2Scale(ab, t));
    return Vector2Distance(p, proyeccion);
}

/**
 * @brief Dibuja una línea discontinua (para vías cerradas).
 */
void dibujarLineaDiscontinua(Vector2 inicio, Vector2 fin, float grosor, Color color) {
    const float longitudTotal = Vector2Distance(inicio, fin);
    if (longitudTotal < 1.0f) {
        return;
    }
    const Vector2 dir = Vector2Scale(Vector2Subtract(fin, inicio), 1.0f / longitudTotal);
    const float largoTrazo = 10.0f;
    const float largoEspacio = 7.0f;
    float cursor = 0.0f;
    while (cursor < longitudTotal) {
        const float finTrazo = std::min(cursor + largoTrazo, longitudTotal);
        const Vector2 p1 = Vector2Add(inicio, Vector2Scale(dir, cursor));
        const Vector2 p2 = Vector2Add(inicio, Vector2Scale(dir, finTrazo));
        DrawLineEx(p1, p2, grosor, color);
        cursor += largoTrazo + largoEspacio;
    }
}

/**
 * @brief Carga una fuente TrueType en raylib con soporte completo para español (Latin-1 + símbolos).
 */
Font cargarFuenteMapaRaylib(const std::string& rutaTtf, int tamanoPx) {
    std::vector<int> codepoints;
    for (int cp = 32; cp <= 255; ++cp) {
        codepoints.push_back(cp);
    }
    // Símbolos adicionales útiles (viñeta, flechas, guiones)
    const int extras[] = {0x2022, 0x2013, 0x2014, 0x2190, 0x2192, 0x2194};
    for (int cp : extras) {
        codepoints.push_back(cp);
    }

    Font fuente = LoadFontEx(rutaTtf.c_str(), tamanoPx, codepoints.data(), static_cast<int>(codepoints.size()));
    if (fuente.texture.id != 0) {
        SetTextureFilter(fuente.texture, TEXTURE_FILTER_BILINEAR);
    }
    return fuente;
}

/**
 * @brief Dibuja texto con la fuente personalizada en raylib y una etiqueta tipo píldora opcional.
 */
void dibujarEtiquetaPildora(
    const Font& fuente,
    const std::string& texto,
    Vector2 centroSuperior,
    float tamanoFuente,
    Color colorTexto,
    Color colorFondo,
    Color colorBorde) {

    const float espaciado = 0.5f;
    const Vector2 dim = MeasureTextEx(fuente, texto.c_str(), tamanoFuente, espaciado);
    const float padX = 7.0f;
    const float padY = 3.0f;
    const Rectangle rect{
        centroSuperior.x - dim.x * 0.5f - padX,
        centroSuperior.y - dim.y - padY * 2.0f,
        dim.x + padX * 2.0f,
        dim.y + padY * 2.0f
    };

    DrawRectangleRounded(rect, 0.45f, 8, colorFondo);
    DrawRectangleRoundedLinesEx(rect, 0.45f, 8, 1.2f, colorBorde);
    DrawTextEx(
        fuente,
        texto.c_str(),
        Vector2{rect.x + padX, rect.y + padY},
        tamanoFuente,
        espaciado,
        colorTexto);
}

/**
 * @brief Configura un tema moderno con bordes redondeados, superficies suaves y acentos vibrantes.
 */
void aplicarTemaModernoImGui() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(11.0f, 7.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 12.0f;

    // Bordes redondeados modernos
    style.WindowRounding = 0.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.93f, 0.96f, 0.99f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.52f, 0.62f, 0.75f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.07f, 0.10f, 0.16f, 0.98f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.14f, 0.22f, 0.95f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.09f, 0.13f, 0.21f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.29f, 0.42f, 0.60f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg]               = ImVec4(0.13f, 0.19f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.18f, 0.26f, 0.39f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.22f, 0.32f, 0.48f, 1.00f);

    colors[ImGuiCol_TitleBg]               = ImVec4(0.06f, 0.09f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.12f, 0.20f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.08f, 0.12f, 0.20f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.09f, 0.14f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.34f, 0.49f, 0.85f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.45f, 0.65f, 0.95f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.56f, 0.80f, 1.00f);

    colors[ImGuiCol_CheckMark]             = ImVec4(0.22f, 0.88f, 0.58f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.25f, 0.62f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.38f, 0.75f, 1.00f, 1.00f);

    colors[ImGuiCol_Button]                = ImVec4(0.16f, 0.25f, 0.39f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.23f, 0.37f, 0.58f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.18f, 0.50f, 0.85f, 1.00f);

    colors[ImGuiCol_Header]                = ImVec4(0.16f, 0.26f, 0.42f, 0.90f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.36f, 0.56f, 0.95f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.44f, 0.68f, 1.00f);

    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.29f, 0.42f, 0.55f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.17f, 0.26f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.42f, 0.68f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.19f, 0.36f, 0.60f, 1.00f);

    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.13f, 0.20f, 0.31f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.31f, 0.45f, 0.70f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.16f, 0.24f, 0.36f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(
        ANCHO_VENTANA_INICIAL,
        ALTO_VENTANA_INICIAL,
        "Planificador de Rutas con Dijkstra (Bogotá D.C.) - UDistrital CCI");
    SetWindowMinSize(1040, 680);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Escape cancela la colocación antes de cerrar la ventana.

    rlImGuiSetup(true);
    aplicarTemaModernoImGui();

    // Cargar fuentes TrueType amigables (Segoe UI) tanto para ImGui como para el mapa en raylib
    std::string rutaFuenteReg = resolverRutaArchivo("assets/fuente_ui.ttf");
    if (!std::filesystem::exists(rutaFuenteReg) && std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
        rutaFuenteReg = "C:/Windows/Fonts/segoeui.ttf";
    }
    std::string rutaFuenteBold = resolverRutaArchivo("assets/fuente_ui_bold.ttf");
    if (!std::filesystem::exists(rutaFuenteBold) && std::filesystem::exists("C:/Windows/Fonts/segoeuib.ttf")) {
        rutaFuenteBold = "C:/Windows/Fonts/segoeuib.ttf";
    }

    ImFont* fuenteRegularUI = nullptr;
    ImFont* fuenteNegritaUI = nullptr;
    ImFont* fuenteTituloUI = nullptr;
    {
        ImGuiIO& io = ImGui::GetIO();
        if (std::filesystem::exists(rutaFuenteReg)) {
            io.Fonts->Clear();
            fuenteRegularUI = io.Fonts->AddFontFromFileTTF(rutaFuenteReg.c_str(), 16.0f);
            if (std::filesystem::exists(rutaFuenteBold)) {
                fuenteNegritaUI = io.Fonts->AddFontFromFileTTF(rutaFuenteBold.c_str(), 17.0f);
                fuenteTituloUI = io.Fonts->AddFontFromFileTTF(rutaFuenteBold.c_str(), 20.0f);
            } else {
                fuenteNegritaUI = fuenteRegularUI;
                fuenteTituloUI = fuenteRegularUI;
            }
            if (fuenteRegularUI != nullptr) {
                io.FontDefault = fuenteRegularUI;
            }
        }
    }

    Font fuenteMapa = GetFontDefault();
    bool tieneFuenteMapaPropia = false;
    if (std::filesystem::exists(rutaFuenteBold)) {
        fuenteMapa = cargarFuenteMapaRaylib(rutaFuenteBold, 28);
        tieneFuenteMapaPropia = (fuenteMapa.texture.id != 0);
    } else if (std::filesystem::exists(rutaFuenteReg)) {
        fuenteMapa = cargarFuenteMapaRaylib(rutaFuenteReg, 28);
        tieneFuenteMapaPropia = (fuenteMapa.texture.id != 0);
    }

    // Cargar grafo desde JSON
    Grafo grafo;
    const std::string rutaJson = resolverRutaArchivo("data/bogota.json");
    if (!grafo.cargarDesdeJSON(rutaJson)) {
        TraceLog(LOG_WARNING, "No se encontro data/bogota.json, cargando nodos de respaldo.");
        grafo.agregarLugar({0, "Plaza de Bolívar", "La Candelaria", 4.5981, -74.0758});
        grafo.agregarLugar({1, "UDistrital - Ingeniería (Calle 40)", "Chapinero", 4.6283, -74.0654});
        grafo.agregarLugar({2, "Aeropuerto El Dorado", "Fontibón", 4.7016, -74.1469});
        grafo.agregarArista({0, 0, 1, "Carrera Séptima", 3.6, 30.0, 1.1, true, true});
        grafo.agregarArista({1, 1, 2, "Av. El Dorado (Calle 26)", 11.5, 50.0, 1.0, true, true});
    }

    // Cargar textura del mapa de Bogotá
    const std::string rutaMapa = resolverRutaArchivo("assets/mapa_bogota.png");
    Texture2D texturaMapa{};
    bool tieneTexturaMapa = false;
    if (std::filesystem::exists(rutaMapa)) {
        texturaMapa = LoadTexture(rutaMapa.c_str());
        SetTextureFilter(texturaMapa, TEXTURE_FILTER_BILINEAR);
        tieneTexturaMapa = (texturaMapa.id != 0);
    }

    // Estado de selección y algoritmo
    int idOrigen = 0;   // Plaza de Bolívar
    int idDestino = 12; // Aeropuerto Internacional El Dorado
    if (!grafo.existeLugar(idDestino) && !grafo.obtenerLugares().empty()) {
        idDestino = grafo.obtenerLugares().back().id;
    }

    CriterioPeso criterioActual = CriterioPeso::DISTANCIA_KM;
    ResultadoRuta resultadoActual = Dijkstra::calcularRuta(grafo, idOrigen, idDestino, criterioActual, true);

    // Estado del reproductor paso a paso
    bool modoPasoAPaso = false;
    bool reproduciendoPasos = false;
    int indicePasoActual = 0;
    float velocidadReproduccion = 2.5f;
    float acumuladorTiempoPaso = 0.0f;
    bool mostrarEtiquetasNodos = true;
    bool mostrarPesosAristas = false;
    char filtroVias[128] = "";

    auto recalcularRuta = [&]() {
        resultadoActual = Dijkstra::calcularRuta(grafo, idOrigen, idDestino, criterioActual, true);
        if (!resultadoActual.pasos.empty()) {
            indicePasoActual = std::clamp(
                indicePasoActual, 0, static_cast<int>(resultadoActual.pasos.size()) - 1);
        } else {
            indicePasoActual = 0;
        }
    };

    const auto viasIniciales = grafo.obtenerAristas();
    bool colocandoPunto = false;
    char nombrePunto[80]{};
    std::string avisoMapa;
    std::mt19937 azar(std::random_device{}());
    struct Meteoro {
        int via;
        Vector2 destino;
        double impacto;
        bool cayo;
    };
    std::vector<Meteoro> meteoros;
    bool automaticos = false;
    double siguienteEvento = 0;
    std::string informeEvento = "Red en condiciones iniciales";

    auto iniciarEvento = [&](bool lluvia) {
        std::vector<int> candidatas;
        for (const auto& via : grafo.obtenerAristas()) {
            if (via.activa && (lluvia || via.factorTrafico < 3.0)) candidatas.push_back(via.id);
        }
        std::shuffle(candidatas.begin(), candidatas.end(), azar);
        const int cantidad = std::min(static_cast<int>(candidatas.size()),
                                      std::uniform_int_distribution<int>(8, 15)(azar));
        if (!cantidad) {
            informeEvento = "Sin vías disponibles. Restablece la circulación.";
            return;
        }
        for (int i = 0; i < cantidad; ++i) {
            const auto& via = *grafo.obtenerArista(candidatas[i]);
            if (lluvia) {
                const auto& a = *grafo.obtenerLugar(via.origen);
                const auto& b = *grafo.obtenerLugar(via.destino);
                const auto& box = grafo.obtenerBoundingBox();
                Vector2 punto = proyectarCoordenada((a.latitud + b.latitud) / 2,
                                                     (a.longitud + b.longitud) / 2, box);
                meteoros.push_back({via.id, punto, GetTime() + 0.9 + i * 0.16, false});
            } else {
                grafo.establecerFactorTrafico(via.id, 3.0);
            }
        }
        informeEvento = lluvia ? "Lluvia de meteoritos en curso" :
            "Trancón masivo en " + std::to_string(cantidad) + " vías (tiempo de viaje ×3)";
        if (!lluvia) recalcularRuta();
    };

    auto agregarPunto = [&](Vector2 punto) {
        const auto& box = grafo.obtenerBoundingBox();
        Vector2 superior = proyectarCoordenada(box.latMax, box.lonMin, box);
        Vector2 inferior = proyectarCoordenada(box.latMin, box.lonMax, box);
        if (!CheckCollisionPointRec(punto, {superior.x, superior.y, inferior.x-superior.x, inferior.y-superior.y})) {
            avisoMapa = "Elige un lugar dentro del mapa.";
            return;
        }
        double lon = box.lonMin + (punto.x-superior.x)/(inferior.x-superior.x)*(box.lonMax-box.lonMin);
        double lat = box.latMax - (punto.y-superior.y)/(inferior.y-superior.y)*(box.latMax-box.latMin);
        std::vector<std::pair<double,int>> vecinos;
        for (const auto& lugar : grafo.obtenerLugares()) {
            vecinos.emplace_back(Haversine::calcularDistanciaKm(lat, lon, lugar.latitud, lugar.longitud), lugar.id);
        }
        std::sort(vecinos.begin(), vecinos.end());
        int id = static_cast<int>(grafo.capacidadNodos());
        std::string nombre = nombrePunto[0] ? nombrePunto : "Punto " + std::to_string(id);
        grafo.agregarLugar({id, nombre, "Personalizado", lat, lon});
        for (size_t i = 0; i < std::min<size_t>(3, vecinos.size()); ++i) {
            int vecino = vecinos[i].second;
            grafo.agregarArista({-1, id, vecino, nombre + " – " + grafo.obtenerLugar(vecino)->nombre});
        }
        colocandoPunto = false;
        nombrePunto[0] = 0;
        avisoMapa = nombre + " agregado a la red";
        recalcularRuta();
    };

    // Cámara 2D para el lienzo del mapa
    Camera2D camaraMapa{};
    auto centrarCamara = [&]() {
        const float anchoDisponible = static_cast<float>(GetScreenWidth()) - ANCHO_PANEL_LATERAL;
        const float altoDisponible = static_cast<float>(GetScreenHeight()) - ALTO_HERRAMIENTAS - ALTO_EVENTOS;
        camaraMapa.offset = Vector2{ANCHO_PANEL_LATERAL + anchoDisponible * 0.5f, ALTO_HERRAMIENTAS + altoDisponible * 0.5f};
        camaraMapa.target = Vector2{ANCHO_MAPA_BASE * 0.5f, ALTO_MAPA_BASE * 0.5f};
        camaraMapa.rotation = 0.0f;
        const float escalaX = anchoDisponible / ANCHO_MAPA_BASE;
        const float escalaY = altoDisponible / ALTO_MAPA_BASE;
        camaraMapa.zoom = std::min(escalaX, escalaY) * 0.96f;
    };
    centrarCamara();

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const float anchoPantalla = static_cast<float>(GetScreenWidth());
        const float altoPantalla = static_cast<float>(GetScreenHeight());
        const float anchoAreaMapa = std::max(200.0f, anchoPantalla - ANCHO_PANEL_LATERAL);

        if (IsWindowResized()) centrarCamara();
        const double ahora = GetTime();
        if (automaticos && ahora >= siguienteEvento && meteoros.empty()) {
            iniciarEvento(std::bernoulli_distribution(0.5)(azar));
            siguienteEvento = ahora + std::uniform_int_distribution<int>(20, 40)(azar);
        }
        bool huboImpacto = false;
        for (auto& meteoro : meteoros) {
            if (!meteoro.cayo && ahora >= meteoro.impacto) {
                grafo.establecerEstadoVia(meteoro.via, false);
                meteoro.cayo = true;
                huboImpacto = true;
            }
        }
        if (huboImpacto) recalcularRuta();
        if (!meteoros.empty() && ahora > meteoros.back().impacto + 0.6) {
            informeEvento = "Lluvia terminada · " + std::to_string(meteoros.size()) + " impactos";
            meteoros.clear();
        }
        // Avance automático del reproductor paso a paso
        if (modoPasoAPaso && reproduciendoPasos && !resultadoActual.pasos.empty()) {
            acumuladorTiempoPaso += dt * velocidadReproduccion;
            if (acumuladorTiempoPaso >= 1.0f) {
                const int pasosAvanzar = static_cast<int>(acumuladorTiempoPaso);
                acumuladorTiempoPaso -= static_cast<float>(pasosAvanzar);
                indicePasoActual += pasosAvanzar;
                if (indicePasoActual >= static_cast<int>(resultadoActual.pasos.size()) - 1) {
                    indicePasoActual = static_cast<int>(resultadoActual.pasos.size()) - 1;
                    reproduciendoPasos = false;
                }
            }
        }

        // Interacción de ratón sobre el lienzo del mapa
        const Vector2 posRaton = GetMousePosition();
        const bool ratonEnMapa = (posRaton.x >= ANCHO_PANEL_LATERAL && posRaton.y >= ALTO_HERRAMIENTAS && posRaton.y < altoPantalla - ALTO_EVENTOS) && !ImGui::GetIO().WantCaptureMouse;
        const Vector2 posMundoRaton = GetScreenToWorld2D(posRaton, camaraMapa);

        int idNodoHover = -1;
        int idAristaHover = -1;

        if (IsKeyPressed(KEY_ESCAPE) && !colocandoPunto && !ImGui::GetIO().WantCaptureKeyboard) break;
        if (IsKeyPressed(KEY_ESCAPE) && colocandoPunto) {
            colocandoPunto = false;
            avisoMapa.clear();
        }
        if (ratonEnMapa) {
            const float rueda = GetMouseWheelMove();
            if (std::abs(rueda) > 0.001f) {
                const Vector2 mundoAntes = GetScreenToWorld2D(posRaton, camaraMapa);
                camaraMapa.offset = posRaton;
                camaraMapa.target = mundoAntes;
                camaraMapa.zoom = std::clamp(camaraMapa.zoom * (1.0f + rueda * 0.12f), 0.35f, 4.0f);
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
                (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_ALT))) {
                const Vector2 delta = GetMouseDelta();
                camaraMapa.target = Vector2Subtract(
                    camaraMapa.target, Vector2Scale(delta, 1.0f / camaraMapa.zoom));
            }

            const float radioDeteccionNodo = 15.0f / std::sqrt(camaraMapa.zoom);
            float menorDistNodo = radioDeteccionNodo;
            for (const auto& lugar : grafo.obtenerLugares()) {
                const Vector2 p = proyectarCoordenada(
                    lugar.latitud, lugar.longitud, grafo.obtenerBoundingBox());
                const float d = Vector2Distance(posMundoRaton, p);
                if (d < menorDistNodo) {
                    menorDistNodo = d;
                    idNodoHover = lugar.id;
                }
            }

            if (idNodoHover == -1) {
                const float umbralArista = 8.0f / std::sqrt(camaraMapa.zoom);
                float menorDistArista = umbralArista;
                for (const auto& arista : grafo.obtenerAristas()) {
                    const Lugar* lA = grafo.obtenerLugar(arista.origen);
                    const Lugar* lB = grafo.obtenerLugar(arista.destino);
                    if (!lA || !lB) {
                        continue;
                    }
                    const Vector2 pA = proyectarCoordenada(lA->latitud, lA->longitud, grafo.obtenerBoundingBox());
                    const Vector2 pB = proyectarCoordenada(lB->latitud, lB->longitud, grafo.obtenerBoundingBox());
                    const float d = distanciaPuntoSegmento(posMundoRaton, pA, pB);
                    if (d < menorDistArista) {
                        menorDistArista = d;
                        idAristaHover = arista.id;
                    }
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (colocandoPunto) {
                    if (idNodoHover == -1) agregarPunto(posMundoRaton);
                    else avisoMapa = "Elige un espacio libre entre los puntos.";
                } else if (idNodoHover != -1) {
                    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                        idDestino = idNodoHover;
                    } else {
                        idOrigen = idNodoHover;
                    }
                    recalcularRuta();
                } else if (idAristaHover != -1 && IsKeyDown(KEY_LEFT_CONTROL)) {
                    grafo.alternarEstadoVia(idAristaHover);
                    recalcularRuta();
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !IsKeyDown(KEY_LEFT_ALT)) {
                if (idNodoHover != -1) {
                    idDestino = idNodoHover;
                    recalcularRuta();
                } else if (idAristaHover != -1) {
                    grafo.alternarEstadoVia(idAristaHover);
                    recalcularRuta();
                }
            }
        }

        if (ratonEnMapa) SetMouseCursor(colocandoPunto ? MOUSE_CURSOR_CROSSHAIR :
            ((idNodoHover >= 0 || idAristaHover >= 0) ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT));

        std::unordered_set<int> aristasEnCaminoOptimo(
            resultadoActual.caminoAristas.begin(), resultadoActual.caminoAristas.end());
        std::unordered_set<int> nodosEnCaminoOptimo(
            resultadoActual.caminoNodos.begin(), resultadoActual.caminoNodos.end());

        const PasoDijkstra* pasoActivo = nullptr;
        std::unordered_set<int> aristasArbolPaso;
        if (modoPasoAPaso && !resultadoActual.pasos.empty()) {
            indicePasoActual = std::clamp(
                indicePasoActual, 0, static_cast<int>(resultadoActual.pasos.size()) - 1);
            pasoActivo = &resultadoActual.pasos[static_cast<std::size_t>(indicePasoActual)];
            for (int idAristaPred : pasoActivo->aristaPredecesora) {
                if (idAristaPred != -1) {
                    aristasArbolPaso.insert(idAristaPred);
                }
            }
        }

        // ==================== RENDERIZADO ====================
        BeginDrawing();
        ClearBackground(Color{11, 16, 26, 255});

        BeginScissorMode(int(ANCHO_PANEL_LATERAL), int(ALTO_HERRAMIENTAS), int(anchoAreaMapa), int(altoPantalla - ALTO_HERRAMIENTAS - ALTO_EVENTOS));
        BeginMode2D(camaraMapa);

        // 1. Fondo del mapa de Bogotá
        if (tieneTexturaMapa) {
            DrawTexturePro(
                texturaMapa,
                Rectangle{0.0f, 0.0f, static_cast<float>(texturaMapa.width), static_cast<float>(texturaMapa.height)},
                Rectangle{0.0f, 0.0f, ANCHO_MAPA_BASE, ALTO_MAPA_BASE},
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE);
        } else {
            DrawRectangle(0, 0, static_cast<int>(ANCHO_MAPA_BASE), static_cast<int>(ALTO_MAPA_BASE), Color{18, 25, 38, 255});
        }
        DrawRectangleLinesEx(Rectangle{0.0f, 0.0f, ANCHO_MAPA_BASE, ALTO_MAPA_BASE}, 2.0f, Color{65, 95, 135, 180});

        // 2. Dibujar vías (aristas)
        const float tiempoAnim = static_cast<float>(GetTime());
        for (const auto& arista : grafo.obtenerAristas()) {
            const Lugar* lOrigen = grafo.obtenerLugar(arista.origen);
            const Lugar* lDestino = grafo.obtenerLugar(arista.destino);
            if (!lOrigen || !lDestino) {
                continue;
            }

            const Vector2 p1 = proyectarCoordenada(lOrigen->latitud, lOrigen->longitud, grafo.obtenerBoundingBox());
            const Vector2 p2 = proyectarCoordenada(lDestino->latitud, lDestino->longitud, grafo.obtenerBoundingBox());

            const bool esHover = (arista.id == idAristaHover);
            const bool enCaminoOptimo = !modoPasoAPaso && (aristasEnCaminoOptimo.count(arista.id) > 0);
            const bool esAristaEvaluadaPaso = pasoActivo && (pasoActivo->aristaEvaluadaId == arista.id);
            const bool enArbolPaso = pasoActivo && (aristasArbolPaso.count(arista.id) > 0);

            if (!arista.activa) {
                dibujarLineaDiscontinua(p1, p2, (esHover ? 6.0f : 4.5f) / camaraMapa.zoom, Color{239, 68, 68, 215});
                const Vector2 medio = Vector2Lerp(p1, p2, 0.5f);
                DrawCircleV(medio, 7.5f, Color{185, 28, 28, 245});
                DrawTextEx(fuenteMapa, "X", Vector2{medio.x - 4.0f, medio.y - 6.0f}, 13.0f, 0.0f, WHITE);
                continue;
            }

            Color colorVia = Color{100, 140, 190, 165};
            float grosorVia = 4.0f;

            if (arista.factorTrafico >= 1.30) {
                colorVia = Color{235, 125, 55, 190};
            } else if (arista.factorTrafico >= 1.18) {
                colorVia = Color{210, 175, 75, 180};
            }

            if (enArbolPaso) {
                colorVia = Color{56, 189, 248, 235};
                grosorVia = 4.2f;
            }
            if (esAristaEvaluadaPaso) {
                colorVia = pasoActivo->huboRelajacion
                               ? Color{250, 204, 21, 255}
                               : Color{244, 114, 182, 245};
                grosorVia = 6.0f;
            }
            if (enCaminoOptimo) {
                DrawLineEx(p1, p2, 12.0f / camaraMapa.zoom, Color{16, 185, 129, 80});
                colorVia = Color{52, 211, 153, 255};
                grosorVia = 7.0f;
            } else if (esHover) {
                colorVia = Color{220, 238, 255, 250};
                grosorVia += 1.6f;
            }

            DrawLineEx(p1, p2, (grosorVia + 2) / camaraMapa.zoom, Color{12, 23, 37, 225});
            DrawLineEx(p1, p2, grosorVia / camaraMapa.zoom, colorVia);
            if (arista.factorTrafico >= 3.0) {
                for (int i = 0; i < 4; ++i) {
                    const float fase = 0.30f + 0.085f * i + 0.02f * std::sin(tiempoAnim);
                    DrawCircleV(Vector2Lerp(p1, p2, fase), 2.8f / camaraMapa.zoom, ORANGE);
                }
            }

            if (mostrarPesosAristas) {
                const Vector2 medio = Vector2Lerp(p1, p2, 0.5f);
                char bufPeso[32];
                if (criterioActual == CriterioPeso::DISTANCIA_KM) {
                    std::snprintf(bufPeso, sizeof(bufPeso), "%.1f km", arista.distanciaKm);
                } else {
                    std::snprintf(bufPeso, sizeof(bufPeso), "%.1f min", arista.obtenerPeso(criterioActual));
                }
                dibujarEtiquetaPildora(
                    fuenteMapa,
                    bufPeso,
                    Vector2{medio.x, medio.y + 7.0f},
                    12.0f,
                    Color{226, 238, 255, 255},
                    Color{13, 20, 33, 220},
                    Color{65, 105, 160, 180});
            }
        }

        // 3. Partículas animadas de flujo sobre la ruta óptima
        if (!modoPasoAPaso && resultadoActual.encontrada && resultadoActual.caminoNodos.size() >= 2) {
            for (std::size_t i = 0; i + 1 < resultadoActual.caminoNodos.size(); ++i) {
                const Lugar* lA = grafo.obtenerLugar(resultadoActual.caminoNodos[i]);
                const Lugar* lB = grafo.obtenerLugar(resultadoActual.caminoNodos[i + 1]);
                if (!lA || !lB) {
                    continue;
                }
                const Vector2 pA = proyectarCoordenada(lA->latitud, lA->longitud, grafo.obtenerBoundingBox());
                const Vector2 pB = proyectarCoordenada(lB->latitud, lB->longitud, grafo.obtenerBoundingBox());
                const float fase = std::fmod(tiempoAnim * 0.65f + static_cast<float>(i) * 0.25f, 1.0f);
                const Vector2 posParticula = Vector2Lerp(pA, pB, fase);
                DrawCircleV(posParticula, 4.6f, Color{236, 253, 245, 255});
            }
        }

        // Reservar espacio para origen y destino antes de colocar nombres secundarios.
        std::vector<Rectangle> etiquetasVisibles;
        auto espacioEtiqueta = [&](const Lugar& lugar, float tam) {
            const Vector2 p = proyectarCoordenada(lugar.latitud, lugar.longitud, grafo.obtenerBoundingBox());
            const Vector2 dim = MeasureTextEx(fuenteMapa, lugar.nombre.c_str(), tam, 0.5f);
            return Rectangle{p.x - dim.x / 2 - 12, p.y - dim.y - 30, dim.x + 24, dim.y + 24};
        };
        for (int id : {idOrigen, idDestino, idNodoHover}) {
            if (const auto* lugar = grafo.obtenerLugar(id))
                etiquetasVisibles.push_back(espacioEtiqueta(*lugar, 14.5f / camaraMapa.zoom));
        }
        for (const auto& lugar : grafo.obtenerLugares()) {
            const Vector2 pos = proyectarCoordenada(
                lugar.latitud, lugar.longitud, grafo.obtenerBoundingBox());

            const bool esOrigen = (lugar.id == idOrigen);
            const bool esDestino = (lugar.id == idDestino);
            const bool esHover = (lugar.id == idNodoHover);
            const bool enCamino = !modoPasoAPaso && (nodosEnCaminoOptimo.count(lugar.id) > 0);

            Color colorNodo = Color{148, 175, 210, 255};
            float radio = 7.0f;

            if (pasoActivo) {
                const auto idx = static_cast<std::size_t>(lugar.id);
                if (lugar.id == pasoActivo->nodoActual) {
                    colorNodo = Color{250, 204, 21, 255};
                    radio = 9.8f;
                } else if (lugar.id == pasoActivo->nodoVecino) {
                    colorNodo = Color{244, 114, 182, 255};
                    radio = 9.0f;
                } else if (idx < pasoActivo->visitados.size() && pasoActivo->visitados[idx]) {
                    colorNodo = Color{56, 189, 248, 255};
                    radio = 7.8f;
                } else if (idx < pasoActivo->distancias.size() && std::isfinite(pasoActivo->distancias[idx])) {
                    colorNodo = Color{251, 146, 60, 255};
                    radio = 7.4f;
                }
            } else if (enCamino) {
                colorNodo = Color{52, 211, 153, 255};
                radio = 8.4f;
            }

            if (esOrigen) {
                DrawCircleV(pos, 14.5f, Color{16, 185, 129, 90});
                colorNodo = Color{16, 215, 135, 255};
                radio = 10.0f;
            } else if (esDestino) {
                DrawCircleV(pos, 14.5f, Color{239, 68, 68, 90});
                colorNodo = Color{248, 85, 85, 255};
                radio = 10.0f;
            }

            if (esHover) {
                DrawCircleV(pos, radio + 5.0f, Color{255, 255, 255, 120});
            }

            DrawCircleV(pos, radio + 2.0f, Color{11, 17, 28, 255});
            DrawCircleV(pos, radio, colorNodo);

            if (mostrarEtiquetasNodos || esOrigen || esDestino || esHover) {
                const bool destacada = esOrigen || esDestino || esHover;
                const float tamTexto = (destacada ? 14.5f : 12.0f) / camaraMapa.zoom;
                if (!destacada) {
                    const Rectangle area = espacioEtiqueta(lugar, tamTexto);
                    if (std::any_of(etiquetasVisibles.begin(), etiquetasVisibles.end(),
                                    [&](Rectangle otra) { return CheckCollisionRecs(area, otra); })) continue;
                    etiquetasVisibles.push_back(area);
                }
                Color colTexto = Color{232, 240, 252, 255};
                Color colFondo = Color{13, 20, 34, 215};
                Color colBorde = Color{55, 85, 130, 170};

                if (esOrigen) {
                    colTexto = Color{210, 255, 235, 255};
                    colFondo = Color{10, 42, 32, 235};
                    colBorde = Color{35, 205, 130, 235};
                } else if (esDestino) {
                    colTexto = Color{255, 225, 225, 255};
                    colFondo = Color{48, 16, 22, 235};
                    colBorde = Color{245, 85, 85, 235};
                } else if (enCamino) {
                    colBorde = Color{52, 211, 153, 210};
                }

                dibujarEtiquetaPildora(
                    fuenteMapa,
                    lugar.nombre,
                    Vector2{pos.x, pos.y - radio - 4.0f},
                    tamTexto,
                    colTexto,
                    colFondo,
                    colBorde);
            }
        }

        for (const auto& meteoro : meteoros) {
            const float t = static_cast<float>(meteoro.impacto - ahora);
            if (t > 0.85f || t < -0.55f) continue;
            if (t > 0) {
                Vector2 cabeza{meteoro.destino.x - 180*t, meteoro.destino.y - 340*t};
                DrawLineEx({cabeza.x - 25, cabeza.y - 47}, cabeza, 7, Color{251, 129, 46, 225});
                DrawCircleV(cabeza, 5, Color{255, 235, 154, 255});
            } else {
                DrawRing(meteoro.destino, 5-50*t, 8-50*t, 0, 360, 36, Fade(ORANGE, 1+t/0.55f));
            }
        }
        EndMode2D();
        // 6. Tooltip flotante moderno al pasar el ratón sobre un nodo o vía
        if (idNodoHover != -1) {
            const Lugar* l = grafo.obtenerLugar(idNodoHover);
            if (l) {
                char linea1[128], linea2[128], linea3[128];
                std::snprintf(linea1, sizeof(linea1), "#%d  %s", l->id, l->nombre.c_str());
                std::snprintf(linea2, sizeof(linea2), "Localidad: %s", l->localidad.c_str());
                std::snprintf(linea3, sizeof(linea3), "Coordenadas: Lat %.4f, Lon %.4f", l->latitud, l->longitud);
                const float bx = std::min(posRaton.x + 16.0f, anchoPantalla - 305.0f);
                const float by = std::max(16.0f, posRaton.y - 76.0f);
                const Rectangle tipRect{bx, by, 295.0f, 70.0f};
                DrawRectangleRounded(tipRect, 0.25f, 8, Color{10, 16, 28, 245});
                DrawRectangleRoundedLinesEx(tipRect, 0.25f, 8, 1.5f, Color{56, 189, 248, 220});
                DrawTextEx(fuenteMapa, linea1, Vector2{bx + 12.0f, by + 8.0f}, 15.5f, 0.4f, Color{253, 224, 71, 255});
                DrawTextEx(fuenteMapa, linea2, Vector2{bx + 12.0f, by + 29.0f}, 13.5f, 0.4f, Color{226, 232, 240, 255});
                DrawTextEx(fuenteMapa, linea3, Vector2{bx + 12.0f, by + 47.0f}, 13.0f, 0.4f, Color{148, 163, 184, 255});
            }
        } else if (idAristaHover != -1) {
            const Arista* a = grafo.obtenerArista(idAristaHover);
            if (a) {
                char linea1[128], linea2[128];
                std::snprintf(linea1, sizeof(linea1), "%s (%s)", a->nombreVia.c_str(), a->activa ? "ACTIVA" : "CERRADA");
                std::snprintf(
                    linea2,
                    sizeof(linea2),
                    "Distancia: %.2f km  •  Vel: %.0f km/h  •  Tiempo: %.1f min",
                    a->distanciaKm,
                    a->velocidadPromedioKmh,
                    a->obtenerPeso(CriterioPeso::TIEMPO_MINUTOS));
                const float bx = std::min(posRaton.x + 16.0f, anchoPantalla - 355.0f);
                const float by = std::max(16.0f, posRaton.y - 58.0f);
                const Rectangle tipRect{bx, by, 345.0f, 52.0f};
                DrawRectangleRounded(tipRect, 0.25f, 8, Color{10, 16, 28, 245});
                DrawRectangleRoundedLinesEx(tipRect, 0.25f, 8, 1.5f, Color{251, 191, 36, 220});
                DrawTextEx(fuenteMapa, linea1, Vector2{bx + 12.0f, by + 8.0f}, 15.0f, 0.4f, Color{253, 224, 71, 255});
                DrawTextEx(fuenteMapa, linea2, Vector2{bx + 12.0f, by + 28.0f}, 13.5f, 0.4f, Color{226, 232, 240, 255});
            }
        }

        EndScissorMode();

        // ==================== PANEL LATERAL MODERNO (DEAR IMGUI) ====================
        rlImGuiBegin();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(ANCHO_PANEL_LATERAL, altoPantalla), ImGuiCond_Always);
        ImGui::Begin(
            "##PanelControlDijkstra",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::TextDisabled("BOGOTÁ D.C.  /  MOVILIDAD");
        if (fuenteTituloUI) ImGui::PushFont(fuenteTituloUI, 22.0f);
        ImGui::TextUnformatted("Planificador de rutas");
        if (fuenteTituloUI) ImGui::PopFont();
        ImGui::TextDisabled("%zu puntos   ·   %zu vías", grafo.numeroLugares(), grafo.numeroAristas());
        ImGui::Spacing();
        // Barra de Pestañas Moderna
        if (ImGui::BeginTabBar("TabsNavegacion", ImGuiTabBarFlags_None)) {

            // =========================================================================
            // PESTAÑA 1: PLANIFICADOR DE RUTA Y RESULTADOS
            // =========================================================================
            if (ImGui::BeginTabItem("Ruta")) {
                ImGui::Spacing();

                // Tarjeta de Selección de Puntos
                ImGui::BeginChild("CardSeleccion", ImVec2(0.0f, 222.0f), ImGuiChildFlags_Borders);
                if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "Planear recorrido");
                if (fuenteNegritaUI) ImGui::PopFont();
                ImGui::Spacing();

                const auto& lugares = grafo.obtenerLugares();
                const Lugar* lugarOrigPtr = grafo.obtenerLugar(idOrigen);
                const Lugar* lugarDestPtr = grafo.obtenerLugar(idDestino);

                ImGui::SetNextItemWidth(-70.0f);
                if (ImGui::BeginCombo("Origen", lugarOrigPtr ? lugarOrigPtr->nombre.c_str() : "Seleccione...")) {
                    for (const auto& l : lugares) {
                        const bool seleccionado = (l.id == idOrigen);
                        const std::string etiqueta = "[" + std::to_string(l.id) + "] " + l.nombre + " (" + l.localidad + ")";
                        if (ImGui::Selectable(etiqueta.c_str(), seleccionado)) {
                            idOrigen = l.id;
                            recalcularRuta();
                        }
                        if (seleccionado) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::SetNextItemWidth(-70.0f);
                if (ImGui::BeginCombo("Destino", lugarDestPtr ? lugarDestPtr->nombre.c_str() : "Seleccione...")) {
                    for (const auto& l : lugares) {
                        const bool seleccionado = (l.id == idDestino);
                        const std::string etiqueta = "[" + std::to_string(l.id) + "] " + l.nombre + " (" + l.localidad + ")";
                        if (ImGui::Selectable(etiqueta.c_str(), seleccionado)) {
                            idDestino = l.id;
                            recalcularRuta();
                        }
                        if (seleccionado) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Intercambiar Origen y Destino", ImVec2(-1.0f, 30.0f))) {
                    std::swap(idOrigen, idDestino);
                    recalcularRuta();
                }

                ImGui::Spacing();
                int critInt = (criterioActual == CriterioPeso::DISTANCIA_KM) ? 0 : 1;
                if (ImGui::RadioButton("Menor Distancia (km)", &critInt, 0)) {
                    criterioActual = CriterioPeso::DISTANCIA_KM;
                    recalcularRuta();
                }
                ImGui::SameLine(0.0f, 18.0f);
                if (ImGui::RadioButton("Menor Tiempo (min)", &critInt, 1)) {
                    criterioActual = CriterioPeso::TIEMPO_MINUTOS;
                    recalcularRuta();
                }
                ImGui::EndChild();

                ImGui::Spacing();

                // Tarjetas de Métricas KPI (Grid 2x2)
                if (resultadoActual.encontrada) {
                    const float anchoCol = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 9));
                    ImGui::BeginChild("KPI_Distancia", ImVec2(anchoCol, 68.0f), ImGuiChildFlags_Borders);
                    ImGui::TextDisabled("DISTANCIA TOTAL");
                    if (fuenteTituloUI) ImGui::PushFont(fuenteTituloUI);
                    ImGui::TextColored(ImVec4(0.20f, 0.92f, 0.62f, 1.0f), "%.2f km", resultadoActual.distanciaTotalKm);
                    if (fuenteTituloUI) ImGui::PopFont();
                    ImGui::EndChild();

                    ImGui::SameLine();

                    ImGui::BeginChild("KPI_Tiempo", ImVec2(anchoCol, 68.0f), ImGuiChildFlags_Borders);
                    ImGui::TextDisabled("TIEMPO ESTIMADO");
                    if (fuenteTituloUI) ImGui::PushFont(fuenteTituloUI);
                    ImGui::TextColored(ImVec4(0.98f, 0.80f, 0.25f, 1.0f), "%.1f min", resultadoActual.tiempoTotalMin);
                    if (fuenteTituloUI) ImGui::PopFont();
                    ImGui::EndChild();

                    ImGui::BeginChild("KPI_Explorados", ImVec2(anchoCol, 68.0f), ImGuiChildFlags_Borders);
                    ImGui::TextDisabled("NODOS EXPLORADOS");
                    if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                    ImGui::TextColored(ImVec4(0.38f, 0.78f, 1.0f, 1.0f), "%d de %zu lugares", resultadoActual.nodosExplorados, grafo.numeroLugares());
                    if (fuenteNegritaUI) ImGui::PopFont();
                    ImGui::EndChild();

                    ImGui::SameLine();

                    ImGui::BeginChild("KPI_Tramos", ImVec2(anchoCol, 68.0f), ImGuiChildFlags_Borders);
                    ImGui::TextDisabled("CORREDORES VIALES");
                    if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                    ImGui::TextColored(ImVec4(0.85f, 0.70f, 1.0f, 1.0f), "%zu tramos", resultadoActual.caminoAristas.size());
                    if (fuenteNegritaUI) ImGui::PopFont();
                    ImGui::EndChild();
                    ImGui::PopStyleVar();

                    ImGui::Spacing();

                    // Tarjeta del Itinerario Paso a Paso
                    ImGui::BeginChild("CardItinerario", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                    if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                    ImGui::TextColored(ImVec4(0.30f, 0.92f, 0.62f, 1.0f), "Itinerario Detallado del Recorrido");
                    if (fuenteNegritaUI) ImGui::PopFont();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::PushTextWrapPos(0);
                    for (std::size_t i = 0; i < resultadoActual.caminoNodos.size(); ++i) {
                        const Lugar* l = grafo.obtenerLugar(resultadoActual.caminoNodos[i]);
                        if (!l) continue;

                        if (i == 0) {
                            ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.60f, 1.0f), "[ORIGEN]  %s", l->nombre.c_str());
                        } else {
                            const Arista* via = grafo.obtenerArista(resultadoActual.caminoAristas[i - 1]);
                            if (via) {
                                ImGui::TextDisabled(
                                    "    |  por %s (%.2f km • %.1f min)",
                                    via->nombreVia.c_str(),
                                    via->distanciaKm,
                                    via->obtenerPeso(CriterioPeso::TIEMPO_MINUTOS));
                            }
                            if (i + 1 == resultadoActual.caminoNodos.size()) {
                                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "[DESTINO] %s", l->nombre.c_str());
                            } else {
                                ImGui::Text("   (%zu) %s", i, l->nombre.c_str());
                            }
                        }
                    }
                    ImGui::PopTextWrapPos();
                    ImGui::EndChild();
                } else {
                    ImGui::BeginChild("CardSinRuta", ImVec2(0.0f, 90.0f), ImGuiChildFlags_Borders);
                    if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                    ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.40f, 1.0f), "Ruta Inalcanzable");
                    if (fuenteNegritaUI) ImGui::PopFont();
                    ImGui::TextWrapped("Reabre un tramo en Vías o elige otro destino.");
                    ImGui::EndChild();
                }

                ImGui::EndTabItem();
            }

            // =========================================================================
            // PESTAÑA 2: REPRODUCTOR PASO A PASO DE DIJKSTRA (SNAPSHOTS)
            // =========================================================================
            if (ImGui::BeginTabItem("Dijkstra")) {
                ImGui::Spacing();

                ImGui::BeginChild("CardReproductor", ImVec2(0.0f, 235.0f), ImGuiChildFlags_Borders);
                if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                ImGui::TextColored(ImVec4(0.98f, 0.82f, 0.30f, 1.0f), "Simulador de Instantáneas (Snapshots)");
                if (fuenteNegritaUI) ImGui::PopFont();

                if (ImGui::Checkbox("Colorear exploración paso a paso en el mapa", &modoPasoAPaso)) {
                    reproduciendoPasos = false;
                }

                const int totalPasos = static_cast<int>(resultadoActual.pasos.size());
                if (totalPasos > 0) {
                    const float wBtn = (ImGui::GetContentRegionAvail().x - 32.0f) / 5.0f;
                    if (ImGui::Button("Inicio", ImVec2(wBtn, 30.0f))) {
                        modoPasoAPaso = true;
                        reproduciendoPasos = false;
                        indicePasoActual = 0;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Ant.", ImVec2(wBtn, 30.0f))) {
                        modoPasoAPaso = true;
                        reproduciendoPasos = false;
                        indicePasoActual = std::max(0, indicePasoActual - 1);
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, reproduciendoPasos ? ImVec4(0.85f, 0.32f, 0.32f, 1.0f) : ImVec4(0.16f, 0.65f, 0.45f, 1.0f));
                    if (ImGui::Button(reproduciendoPasos ? "Pausar" : "Play", ImVec2(wBtn, 30.0f))) {
                        modoPasoAPaso = true;
                        if (indicePasoActual >= totalPasos - 1) {
                            indicePasoActual = 0;
                        }
                        reproduciendoPasos = !reproduciendoPasos;
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::Button("Sig.", ImVec2(wBtn, 30.0f))) {
                        modoPasoAPaso = true;
                        reproduciendoPasos = false;
                        indicePasoActual = std::min(totalPasos - 1, indicePasoActual + 1);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Fin", ImVec2(wBtn, 30.0f))) {
                        modoPasoAPaso = true;
                        reproduciendoPasos = false;
                        indicePasoActual = totalPasos - 1;
                    }

                    ImGui::SetNextItemWidth(-85.0f);
                    if (ImGui::SliderInt("Iteración", &indicePasoActual, 0, totalPasos - 1, "Paso %d")) {
                        modoPasoAPaso = true;
                        reproduciendoPasos = false;
                    }
                    ImGui::SetNextItemWidth(-85.0f);
                    ImGui::SliderFloat("Velocidad", &velocidadReproduccion, 0.5f, 12.0f, "%.1f pasos/s");

                    const auto& paso = resultadoActual.pasos[static_cast<std::size_t>(indicePasoActual)];
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Operación en Paso #%d de %d:", paso.iteracion + 1, totalPasos);
                    ImGui::TextWrapped("%s", paso.descripcion.c_str());
                }
                ImGui::EndChild();

                ImGui::Spacing();

                // Tabla de Distancias Tentativas d[v] y Predecesores
                ImGui::BeginChild("CardTablaPasos", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                if (fuenteNegritaUI) ImGui::PushFont(fuenteNegritaUI);
                ImGui::TextColored(ImVec4(0.55f, 0.82f, 1.0f, 1.0f), "Vector de Distancias d[v] y Predecesores");
                if (fuenteNegritaUI) ImGui::PopFont();
                ImGui::Spacing();

                if (!resultadoActual.pasos.empty()) {
                    const auto& paso = resultadoActual.pasos[static_cast<std::size_t>(indicePasoActual)];
                    if (ImGui::BeginTable(
                            "tabla_dijkstra",
                            4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                            ImVec2(0.0f, 0.0f))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Lugar", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                        ImGui::TableSetupColumn("d[v]", ImGuiTableColumnFlags_WidthFixed, 62.0f);
                        ImGui::TableSetupColumn("Predecesor", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        for (const auto& l : grafo.obtenerLugares()) {
                            const auto idx = static_cast<std::size_t>(l.id);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", l.nombre.c_str());

                            ImGui::TableSetColumnIndex(1);
                            if (l.id == paso.nodoActual) {
                                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Actual");
                            } else if (idx < paso.visitados.size() && paso.visitados[idx]) {
                                ImGui::TextColored(ImVec4(0.35f, 0.82f, 1.0f, 1.0f), "Visitado");
                            } else {
                                ImGui::TextDisabled("En espera");
                            }

                            ImGui::TableSetColumnIndex(2);
                            if (idx < paso.distancias.size() && std::isfinite(paso.distancias[idx])) {
                                ImGui::TextColored(ImVec4(0.30f, 0.95f, 0.65f, 1.0f), "%.2f", paso.distancias[idx]);
                            } else {
                                ImGui::TextDisabled("INF");
                            }

                            ImGui::TableSetColumnIndex(3);
                            if (idx < paso.predecesores.size() && paso.predecesores[idx] != -1) {
                                const Lugar* pred = grafo.obtenerLugar(paso.predecesores[idx]);
                                ImGui::Text("%s", pred ? pred->nombre.c_str() : "-");
                            } else {
                                ImGui::TextDisabled("-");
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            // =========================================================================
            // PESTAÑA 3: GESTOR DE CIERRE DE VÍAS Y TRÁFICO
            // =========================================================================
            if (ImGui::BeginTabItem("Vías")) {
                ImGui::Spacing();

                ImGui::BeginChild("CardControlesVias", ImVec2(0.0f, 96.0f), ImGuiChildFlags_Borders);
                const float wBotonAccion = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
                if (ImGui::Button("Habilitar Todas las Vías", ImVec2(wBotonAccion, 32.0f))) {
                    grafo.activarTodasLasVias();
                    recalcularRuta();
                }
                ImGui::SameLine();
                if (ImGui::Button("Tráfico Fluido (1.0x)", ImVec2(wBotonAccion, 32.0f))) {
                    for (const auto& a : grafo.obtenerAristas()) {
                        grafo.establecerFactorTrafico(a.id, 1.0);
                    }
                    recalcularRuta();
                }

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint(
                    "##filtro_vias",
                    "Buscar corredor vial (ej. Caracas, Autopista, NQS, Boyacá)...",
                    filtroVias,
                    sizeof(filtroVias));
                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("ListaTarjetasVias", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                const std::string filtroStr(filtroVias);
                for (const auto& arista : grafo.obtenerAristas()) {
                    if (!filtroStr.empty()) {
                        std::string nombreMin = arista.nombreVia;
                        std::string filtroMin = filtroStr;
                        auto aMinuscula = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
                        std::transform(nombreMin.begin(), nombreMin.end(), nombreMin.begin(), aMinuscula);
                        std::transform(filtroMin.begin(), filtroMin.end(), filtroMin.begin(), aMinuscula);
                        if (nombreMin.find(filtroMin) == std::string::npos) {
                            continue;
                        }
                    }

                    ImGui::PushID(arista.id);
                    bool activa = arista.activa;
                    if (ImGui::Checkbox("##activa", &activa)) {
                        grafo.establecerEstadoVia(arista.id, activa);
                        recalcularRuta();
                    }
                    ImGui::SameLine();
                    if (!activa) {
                        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "[CERRADA] %s", arista.nombreVia.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(0.92f, 0.96f, 1.0f, 1.0f), "%s", arista.nombreVia.c_str());
                    }

                    ImGui::TextDisabled(
                        "   %.2f km  •  Vel. base: %.0f km/h  •  Tiempo actual: %.1f min",
                        arista.distanciaKm,
                        arista.velocidadPromedioKmh,
                        arista.obtenerPeso(CriterioPeso::TIEMPO_MINUTOS));

                    float factor = static_cast<float>(arista.factorTrafico);
                    ImGui::SetNextItemWidth(210.0f);
                    if (ImGui::SliderFloat("Factor Tráfico", &factor, 0.8f, 3.0f, "%.2fx congestión")) {
                        grafo.establecerFactorTrafico(arista.id, static_cast<double>(factor));
                        recalcularRuta();
                    }
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
        const auto panelFijo = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoSavedSettings;
        ImGui::SetNextWindowPos({ANCHO_PANEL_LATERAL, 0});
        ImGui::SetNextWindowSize({anchoAreaMapa, ALTO_HERRAMIENTAS});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14, 8});
        ImGui::Begin("Herramientas del mapa", nullptr, panelFijo);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored({0.43f,0.83f,0.98f,1}, "Mapa vial");
        ImGui::SameLine();
        if (ImGui::Button("Centrar")) centrarCamara();
        ImGui::SameLine();
        ImGui::Checkbox("Nombres", &mostrarEtiquetasNodos);
        ImGui::SameLine();
        ImGui::Checkbox("Pesos", &mostrarPesosAristas);
        ImGui::SameLine();
        if (ImGui::Button(colocandoPunto ? "Cancelar" : "+ Agregar punto")) {
            colocandoPunto = !colocandoPunto;
            avisoMapa.clear();
        }
        if (colocandoPunto) {
            ImGui::SetNextItemWidth(175);
            ImGui::InputTextWithHint("##nombrePunto", "Nombre (opcional)", nombrePunto, sizeof(nombrePunto));
            ImGui::SameLine();
            ImGui::TextUnformatted(avisoMapa.empty() ? "Haz clic en un espacio libre del mapa." : avisoMapa.c_str());
        } else {
            ImGui::TextDisabled("%s", avisoMapa.empty() ?
                "Clic: origen   ·   Clic derecho: destino o cierre   ·   Rueda: zoom   ·   Central: mover" : avisoMapa.c_str());
        }
        ImGui::End();

        ImGui::SetNextWindowPos({ANCHO_PANEL_LATERAL, altoPantalla - ALTO_EVENTOS});
        ImGui::SetNextWindowSize({anchoAreaMapa, ALTO_EVENTOS});
        ImGui::Begin("Simulación de eventos", nullptr, panelFijo);
        ImGui::TextColored({0.43f,0.83f,0.98f,1}, "Eventos de la red");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", informeEvento.c_str());
        ImGui::BeginDisabled(!meteoros.empty());
        if (ImGui::Button("Lluvia de meteoritos")) iniciarEvento(true);
        ImGui::SameLine();
        if (ImGui::Button("Trancón masivo")) iniciarEvento(false);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("Aleatorios", &automaticos)) siguienteEvento = ahora + 20;
        ImGui::SameLine();
        if (ImGui::Button("Restablecer red")) {
            meteoros.clear();
            automaticos = false;
            grafo.activarTodasLasVias();
            for (const auto& via : grafo.obtenerAristas()) grafo.establecerFactorTrafico(via.id, 1.0);
            for (const auto& via : viasIniciales) {
                grafo.establecerEstadoVia(via.id, via.activa);
                grafo.establecerFactorTrafico(via.id, via.factorTrafico);
            }
            informeEvento = "Circulación inicial restablecida";
            recalcularRuta();
        }
        const auto& vias = grafo.obtenerAristas();
        const auto cerradas = std::count_if(vias.begin(), vias.end(), [](const Arista& a){ return !a.activa; });
        const auto saturadas = std::count_if(vias.begin(), vias.end(), [](const Arista& a){ return a.activa && a.factorTrafico >= 3; });
        ImGui::TextColored({0.25f, 0.89f, 0.65f, 1}, "Ruta óptima");
        ImGui::SameLine();
        ImGui::TextColored({1, 0.57f, 0.38f, 1}, "  %d con trancón masivo", int(saturadas));
        ImGui::SameLine();
        ImGui::TextColored({1, 0.44f, 0.45f, 1}, "  %d vías cerradas", int(cerradas));
        if (automaticos) {
            ImGui::SameLine();
            ImGui::TextDisabled("  Siguiente evento en %d s", int(std::ceil(std::max(0.0, siguienteEvento - ahora))));
        }
        ImGui::End();
        ImGui::PopStyleVar();
        rlImGuiEnd();

        EndDrawing();
    }

    if (tieneFuenteMapaPropia) {
        UnloadFont(fuenteMapa);
    }
    if (tieneTexturaMapa) {
        UnloadTexture(texturaMapa);
    }
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
