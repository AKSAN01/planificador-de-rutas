#pragma once

#include "Grafo.hpp"
#include "Modelos.hpp"

namespace planificador {

/**
 * @brief Implementación del algoritmo de Dijkstra desde cero utilizando lista de adyacencia
 *        y montículo binario mínimo propio (MinHeap) con grabación de instantáneas (snapshots)
 *        para visualización interactiva paso a paso.
 */
class Dijkstra {
public:
    /**
     * @brief Calcula la ruta óptima entre `idOrigen` e `idDestino` sobre el grafo dado.
     * @param grafo Grafo ponderado con la malla vial.
     * @param idOrigen Identificador del nodo de partida.
     * @param idDestino Identificador del nodo de llegada.
     * @param criterio Criterio de optimización (DISTANCIA_KM o TIEMPO_MINUTOS).
     * @param grabarPasos Si es true, almacena cada paso de extracción/relajación en `ResultadoRuta::pasos`.
     * @return Estructura `ResultadoRuta` con el camino óptimo, métricas y pasos del algoritmo.
     */
    static ResultadoRuta calcularRuta(
        const Grafo& grafo,
        int idOrigen,
        int idDestino,
        CriterioPeso criterio = CriterioPeso::DISTANCIA_KM,
        bool grabarPasos = true);
};

} // namespace planificador
