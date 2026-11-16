#include "Dijkstra.hpp"
#include "Haversine.hpp"
#include "MinHeap.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace planificador {

namespace {
std::string formatearDecimal(double valor, int decimales = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimales) << valor;
    return oss.str();
}
} // namespace

ResultadoRuta Dijkstra::calcularRuta(
    const Grafo& grafo,
    int idOrigen,
    int idDestino,
    CriterioPeso criterio,
    bool grabarPasos) {

    ResultadoRuta resultado;
    resultado.origen = idOrigen;
    resultado.destino = idDestino;
    resultado.criterio = criterio;

    if (!grafo.existeLugar(idOrigen) || !grafo.existeLugar(idDestino)) {
        return resultado;
    }

    const std::size_t nCapacidad = grafo.capacidadNodos();
    const double INF = std::numeric_limits<double>::infinity();

    std::vector<double> distancias(nCapacidad, INF);
    std::vector<int> predecesores(nCapacidad, -1);
    std::vector<int> aristaPredecesora(nCapacidad, -1);
    std::vector<bool> visitados(nCapacidad, false);

    MinHeap colaPrioridad(nCapacidad);

    distancias[static_cast<std::size_t>(idOrigen)] = 0.0;
    colaPrioridad.insertar(idOrigen, 0.0);

    const std::string unidad = (criterio == CriterioPeso::DISTANCIA_KM) ? " km" : " min";
    int contadorIteraciones = 0;

    auto registrarPaso = [&](int nodoActual, int idArista, int nodoVecino, bool relajado, const std::string& desc) {
        if (!grabarPasos) {
            return;
        }
        PasoDijkstra paso;
        paso.iteracion = contadorIteraciones++;
        paso.nodoActual = nodoActual;
        paso.aristaEvaluadaId = idArista;
        paso.nodoVecino = nodoVecino;
        paso.huboRelajacion = relajado;
        paso.descripcion = desc;
        paso.distancias = distancias;
        paso.predecesores = predecesores;
        paso.aristaPredecesora = aristaPredecesora;
        paso.visitados = visitados;
        resultado.pasos.push_back(std::move(paso));
    };

    const Lugar* lugarOrigen = grafo.obtenerLugar(idOrigen);
    registrarPaso(
        idOrigen, -1, -1, false,
        "Inicialización: origen en '" + (lugarOrigen ? lugarOrigen->nombre : std::to_string(idOrigen)) +
            "' con distancia 0.00" + unidad + ".");

    while (!colaPrioridad.estaVacio()) {
        const ElementoHeap actual = colaPrioridad.extraerMinimo();
        const int u = actual.nodo;
        const auto idxU = static_cast<std::size_t>(u);

        if (visitados[idxU]) {
            continue;
        }

        visitados[idxU] = true;
        resultado.nodosExplorados++;

        const Lugar* lugarU = grafo.obtenerLugar(u);
        const std::string nombreU = lugarU ? lugarU->nombre : ("Nodo " + std::to_string(u));

        registrarPaso(
            u, -1, -1, false,
            "Extraído del MinHeap: '" + nombreU + "' (acumulado = " +
                formatearDecimal(distancias[idxU]) + unidad + ").");

        if (u == idDestino) {
            registrarPaso(
                u, -1, -1, false,
                "¡Destino alcanzado! Se garantiza el camino óptimo hacia '" + nombreU + "'.");
            break;
        }

        const auto& adyacentes = grafo.obtenerAdyacentes(u);
        for (const Arista& arista : adyacentes) {
            if (!arista.activa) {
                continue; // Vía cerrada
            }

            const int v = arista.destino;
            if (!grafo.existeLugar(v)) {
                continue;
            }
            const auto idxV = static_cast<std::size_t>(v);
            if (visitados[idxV]) {
                continue;
            }

            const double pesoArista = arista.obtenerPeso(criterio);
            if (pesoArista < 0.0 || !std::isfinite(pesoArista)) {
                continue;
            }

            const double nuevaDistancia = distancias[idxU] + pesoArista;
            const Lugar* lugarV = grafo.obtenerLugar(v);
            const std::string nombreV = lugarV ? lugarV->nombre : ("Nodo " + std::to_string(v));

            if (nuevaDistancia < distancias[idxV]) {
                const double anterior = distancias[idxV];
                distancias[idxV] = nuevaDistancia;
                predecesores[idxV] = u;
                aristaPredecesora[idxV] = arista.id;

                if (colaPrioridad.contiene(v)) {
                    colaPrioridad.disminuirPrioridad(v, nuevaDistancia);
                } else {
                    colaPrioridad.insertar(v, nuevaDistancia);
                }

                const std::string strAnt = std::isfinite(anterior) ? (formatearDecimal(anterior) + unidad) : "INF";
                registrarPaso(
                    u, arista.id, v, true,
                    "Relajación exitosa por '" + arista.nombreVia + "' hacia '" + nombreV +
                        "': " + strAnt + " -> " + formatearDecimal(nuevaDistancia) + unidad + ".");
            } else {
                registrarPaso(
                    u, arista.id, v, false,
                    "Sin mejora por '" + arista.nombreVia + "' hacia '" + nombreV +
                        "' (actual " + formatearDecimal(distancias[idxV]) + unidad +
                        " <= candidato " + formatearDecimal(nuevaDistancia) + unidad + ").");
            }
        }
    }

    const auto idxDestino = static_cast<std::size_t>(idDestino);
    if (!std::isfinite(distancias[idxDestino])) {
        return resultado;
    }

    // Reconstrucción del camino óptimo desde el destino hasta el origen
    resultado.encontrada = true;
    resultado.costoTotal = distancias[idxDestino];

    int cursor = idDestino;
    while (cursor != -1) {
        resultado.caminoNodos.push_back(cursor);
        const int idArista = aristaPredecesora[static_cast<std::size_t>(cursor)];
        if (idArista != -1) {
            resultado.caminoAristas.push_back(idArista);
            const Arista* aristaPtr = grafo.obtenerArista(idArista);
            if (aristaPtr) {
                resultado.distanciaTotalKm += aristaPtr->distanciaKm;
                resultado.tiempoTotalMin += Haversine::calcularTiempoMinutos(
                    aristaPtr->distanciaKm, aristaPtr->velocidadPromedioKmh, aristaPtr->factorTrafico);
            }
        }
        if (cursor == idOrigen) {
            break;
        }
        cursor = predecesores[static_cast<std::size_t>(cursor)];
    }

    std::reverse(resultado.caminoNodos.begin(), resultado.caminoNodos.end());
    std::reverse(resultado.caminoAristas.begin(), resultado.caminoAristas.end());

    return resultado;
}

} // namespace planificador
