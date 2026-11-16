#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace planificador {

/**
 * @brief Elemento almacenado en el montículo binario mínimo.
 */
struct ElementoHeap {
    int nodo = -1;
    double prioridad = 0.0;
};

/**
 * @brief Cola de prioridad propia implementada mediante un Montículo Binario Mínimo (Min-Heap)
 *        indexado para soportar inserción, extracción del mínimo y disminución de clave
 *        (decrease-key) en tiempo O(log V), garantizando complejidad O((V + E) log V) en Dijkstra.
 */
class MinHeap {
public:
    explicit MinHeap(std::size_t capacidadMaximaNodos = 0);

    /**
     * @brief Inserta un nodo con su prioridad o actualiza su prioridad si ya existe y es menor.
     */
    void insertar(int nodo, double prioridad);

    /**
     * @brief Disminuye el valor de la prioridad asociada a un nodo existente en el montículo.
     *        Si el nodo no está en el montículo, lo inserta.
     */
    void disminuirPrioridad(int nodo, double nuevaPrioridad);

    /**
     * @brief Extrae y retorna el elemento con la menor prioridad (raíz del montículo).
     * @throws std::underflow_error si el montículo está vacío.
     */
    ElementoHeap extraerMinimo();

    /**
     * @brief Consulta el elemento con menor prioridad sin extraerlo.
     * @throws std::underflow_error si el montículo está vacío.
     */
    ElementoHeap obtenerMinimo() const;

    /**
     * @brief Verifica en O(1) si un nodo se encuentra actualmente dentro del montículo.
     */
    bool contiene(int nodo) const;

    /**
     * @brief Obtiene la prioridad actual de un nodo dentro del montículo.
     */
    double obtenerPrioridad(int nodo) const;

    /**
     * @brief Indica si el montículo no contiene elementos.
     */
    bool estaVacio() const noexcept;

    /**
     * @brief Retorna la cantidad actual de elementos en el montículo.
     */
    std::size_t tamano() const noexcept;

    /**
     * @brief Vacía todos los elementos del montículo.
     */
    void limpiar() noexcept;

private:
    std::vector<ElementoHeap> datos_;
    /// Mapa directo de idNodo -> índice en `datos_` (-1 si no está presente).
    std::vector<int> posicionEnHeap_;

    void asegurarCapacidadNodo(int nodo);
    void intercambiar(std::size_t i, std::size_t j);
    void flotar(std::size_t indice);
    void hundir(std::size_t indice);

    static std::size_t padre(std::size_t i) noexcept { return (i - 1) / 2; }
    static std::size_t hijoIzquierdo(std::size_t i) noexcept { return 2 * i + 1; }
    static std::size_t hijoDerecho(std::size_t i) noexcept { return 2 * i + 2; }
};

} // namespace planificador
