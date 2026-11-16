#pragma once

#include "Modelos.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace planificador {

/**
 * @brief Representa la malla vial de Bogotá como un grafo ponderado mediante lista de adyacencia.
 *        Permite cargar nodos y vías desde un archivo JSON, así como habilitar/deshabilitar vías
 *        y ajustar factores de tráfico en tiempo real.
 */
class Grafo {
public:
    Grafo() = default;

    /**
     * @brief Carga lugares, vías y caja delimitadora desde un archivo JSON.
     * @param rutaArchivo Ruta al archivo JSON (ej. "data/bogota.json").
     * @return true si la carga fue exitosa, false en caso de error.
     */
    bool cargarDesdeJSON(const std::string& rutaArchivo);

    /**
     * @brief Agrega un lugar (vértice) al grafo.
     */
    void agregarLugar(const Lugar& lugar);

    /**
     * @brief Agrega una vía (arista dirigida o bidireccional) al grafo.
     *        Si distanciaKm <= 0, se calcula automáticamente usando Haversine.
     */
    int agregarArista(const Arista& arista);

    /**
     * @brief Activa o desactiva (cierra) una vía por su ID.
     */
    bool establecerEstadoVia(int idArista, bool activa);

    /**
     * @brief Invierte el estado activo/cerrado de una vía.
     */
    bool alternarEstadoVia(int idArista);

    /**
     * @brief Modifica el factor multiplicador de tráfico de una vía (>= 0.5).
     */
    bool establecerFactorTrafico(int idArista, double factorTrafico);

    /**
     * @brief Restaura todas las vías a estado activo y factor de tráfico original (1.0).
     */
    void activarTodasLasVias();

    /**
     * @brief Limpia por completo el grafo.
     */
    void limpiar();

    /**
     * @brief Retorna la lista de aristas salientes desde un nodo dado.
     */
    const std::vector<Arista>& obtenerAdyacentes(int idNodo) const;

    /**
     * @brief Verifica si existe un nodo con el identificador dado.
     */
    bool existeLugar(int idNodo) const;

    /**
     * @brief Obtiene un puntero constante al lugar con el ID especificado (o nullptr si no existe).
     */
    const Lugar* obtenerLugar(int idNodo) const;

    /**
     * @brief Obtiene un puntero constante a la arista maestra por su ID (o nullptr si no existe).
     */
    const Arista* obtenerArista(int idArista) const;

    /**
     * @brief Retorna todos los lugares registrados en el grafo.
     */
    const std::vector<Lugar>& obtenerLugares() const noexcept { return lugares_; }

    /**
     * @brief Retorna el catálogo de vías (aristas lógicas sin duplicar sentido inverso).
     */
    const std::vector<Arista>& obtenerAristas() const noexcept { return aristas_; }

    /**
     * @brief Retorna el número total de lugares (vértices).
     */
    std::size_t numeroLugares() const noexcept { return lugares_.size(); }

    /**
     * @brief Retorna el número total de vías registradas.
     */
    std::size_t numeroAristas() const noexcept { return aristas_.size(); }

    /**
     * @brief Retorna el valor de ID de nodo máximo + 1 (útil para dimensionar arreglos indexados por ID).
     */
    std::size_t capacidadNodos() const noexcept { return listaAdyacencia_.size(); }

    /**
     * @brief Obtiene la caja delimitadora geográfica del mapa.
     */
    const BoundingBoxGeo& obtenerBoundingBox() const noexcept { return boundingBox_; }

    /**
     * @brief Define la caja delimitadora geográfica del mapa.
     */
    void establecerBoundingBox(const BoundingBoxGeo& box) noexcept { boundingBox_ = box; }

private:
    std::vector<Lugar> lugares_;
    std::unordered_map<int, std::size_t> indiceLugarPorId_;

    std::vector<Arista> aristas_;
    std::unordered_map<int, std::size_t> indiceAristaPorId_;

    std::vector<std::vector<Arista>> listaAdyacencia_;
    BoundingBoxGeo boundingBox_{};

    static const std::vector<Arista> kListaVacia_;
};

} // namespace planificador
