#pragma once

#include <limits>
#include <string>
#include <vector>

namespace planificador {

/**
 * @brief Criterio de optimización para el cálculo de la ruta óptima.
 */
enum class CriterioPeso {
    DISTANCIA_KM,    ///< Minimizar la distancia geográfica/vial en kilómetros.
    TIEMPO_MINUTOS   ///< Minimizar el tiempo estimado de viaje en minutos.
};

/**
 * @brief Caja delimitadora geográfica (Bounding Box) del mapa de Bogotá.
 */
struct BoundingBoxGeo {
    double latMin = 4.555;
    double latMax = 4.770;
    double lonMin = -74.165;
    double lonMax = -74.015;
};

/**
 * @brief Representa un lugar emblemático o nodo dentro de la malla vial de Bogotá.
 */
struct Lugar {
    int id = -1;
    std::string nombre;
    std::string localidad;
    double latitud = 0.0;
    double longitud = 0.0;
};

/**
 * @brief Representa un corredor vial o arista dirigida entre dos lugares.
 */
struct Arista {
    int id = -1;
    int origen = -1;
    int destino = -1;
    std::string nombreVia;
    double distanciaKm = 0.0;
    double velocidadPromedioKmh = 40.0;
    double factorTrafico = 1.0; ///< Multiplicador de tiempo (1.0 = flujo normal, >1.0 = congestión)
    bool activa = true;         ///< false si la vía se encuentra cerrada
    bool bidireccional = true;

    /**
     * @brief Obtiene el peso no negativo de la arista según el criterio elegido.
     */
    double obtenerPeso(CriterioPeso criterio) const {
        if (criterio == CriterioPeso::DISTANCIA_KM) {
            return distanciaKm;
        }
        if (velocidadPromedioKmh <= 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        return (distanciaKm / velocidadPromedioKmh) * 60.0 * factorTrafico;
    }
};

/**
 * @brief Instantánea (snapshot) de una iteración del algoritmo de Dijkstra
 *        para su visualización paso a paso en la interfaz gráfica.
 */
struct PasoDijkstra {
    int iteracion = 0;
    int nodoActual = -1;
    int aristaEvaluadaId = -1;
    int nodoVecino = -1;
    bool huboRelajacion = false;
    std::string descripcion;
    std::vector<double> distancias;
    std::vector<int> predecesores;
    std::vector<int> aristaPredecesora;
    std::vector<bool> visitados;
};

/**
 * @brief Resultado completo de la ejecución del algoritmo de Dijkstra.
 */
struct ResultadoRuta {
    bool encontrada = false;
    int origen = -1;
    int destino = -1;
    CriterioPeso criterio = CriterioPeso::DISTANCIA_KM;
    std::vector<int> caminoNodos;
    std::vector<int> caminoAristas;
    double costoTotal = 0.0;
    double distanciaTotalKm = 0.0;
    double tiempoTotalMin = 0.0;
    int nodosExplorados = 0;
    std::vector<PasoDijkstra> pasos;
};

} // namespace planificador
