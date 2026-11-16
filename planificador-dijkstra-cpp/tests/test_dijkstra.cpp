#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Dijkstra.hpp"
#include "Grafo.hpp"
#include "Haversine.hpp"
#include "MinHeap.hpp"
#include "Modelos.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace planificador;
using Catch::Approx;

namespace {
std::string buscarBogotaJson() {
    const std::vector<std::string> rutas = {
        "data/bogota.json",
        "../data/bogota.json",
        "../../data/bogota.json",
        "planificador-dijkstra-cpp/data/bogota.json"
    };
    for (const auto& r : rutas) {
        if (std::filesystem::exists(r)) {
            return r;
        }
    }
    return "data/bogota.json";
}
} // namespace

TEST_CASE("Haversine calcula distancias geograficas y tiempos correctamente", "[haversine]") {
    SECTION("Distancia entre un mismo punto es cero") {
        const double d = Haversine::calcularDistanciaKm(4.5981, -74.0758, 4.5981, -74.0758);
        REQUIRE(d == Approx(0.0).margin(1e-9));
    }

    SECTION("Simetria en la distancia ortodromica") {
        const double d1 = Haversine::calcularDistanciaKm(4.5981, -74.0758, 4.7016, -74.1469);
        const double d2 = Haversine::calcularDistanciaKm(4.7016, -74.1469, 4.5981, -74.0758);
        REQUIRE(d1 == Approx(d2).epsilon(1e-9));
        // Plaza de Bolivar -> Aeropuerto El Dorado (~13.9 km en linea recta)
        REQUIRE(d1 > 12.5);
        REQUIRE(d1 < 15.5);
    }

    SECTION("Calculo de tiempo estimado en minutos con factor de trafico") {
        // 20 km a 40 km/h con trafico 1.0x = 30 minutos
        REQUIRE(Haversine::calcularTiempoMinutos(20.0, 40.0, 1.0) == Approx(30.0));
        // Con congestion 1.5x = 45 minutos
        REQUIRE(Haversine::calcularTiempoMinutos(20.0, 40.0, 1.5) == Approx(45.0));
    }
}

TEST_CASE("MinHeap mantiene la propiedad de monticulo binario minimo y soporta decreaseKey", "[minheap]") {
    MinHeap heap(10);
    REQUIRE(heap.estaVacio());
    REQUIRE(heap.tamano() == 0);

    SECTION("Insercion y extraccion en orden ascendente de prioridad") {
        heap.insertar(4, 15.5);
        heap.insertar(1, 3.2);
        heap.insertar(7, 9.0);
        heap.insertar(2, 1.1);
        heap.insertar(5, 7.4);

        REQUIRE(heap.tamano() == 5);
        REQUIRE(heap.contiene(7));
        REQUIRE_FALSE(heap.contiene(9));

        REQUIRE(heap.extraerMinimo().nodo == 2);
        REQUIRE(heap.extraerMinimo().nodo == 1);
        REQUIRE(heap.extraerMinimo().nodo == 5);
        REQUIRE(heap.extraerMinimo().nodo == 7);
        REQUIRE(heap.extraerMinimo().nodo == 4);
        REQUIRE(heap.estaVacio());
    }

    SECTION("Disminuir prioridad (decrease-key) actualiza posicion en O(log V)") {
        heap.insertar(0, 20.0);
        heap.insertar(1, 10.0);
        heap.insertar(2, 30.0);

        REQUIRE(heap.obtenerMinimo().nodo == 1);

        heap.disminuirPrioridad(2, 4.5);
        REQUIRE(heap.obtenerMinimo().nodo == 2);
        REQUIRE(heap.obtenerPrioridad(2) == Approx(4.5));

        const ElementoHeap minElem = heap.extraerMinimo();
        REQUIRE(minElem.nodo == 2);
        REQUIRE(minElem.prioridad == Approx(4.5));
        REQUIRE_FALSE(heap.contiene(2));
    }

    SECTION("Extraer de un MinHeap vacio lanza std::underflow_error") {
        REQUIRE_THROWS_AS(heap.extraerMinimo(), std::underflow_error);
    }
}

TEST_CASE("Algoritmo de Dijkstra calcula rutas optimas por distancia, tiempo y cierres viales", "[dijkstra]") {
    Grafo g;
    g.agregarLugar({0, "Origen A", "Centro", 4.60, -74.07});
    g.agregarLugar({1, "Intermedio B", "Chapinero", 4.63, -74.06});
    g.agregarLugar({2, "Intermedio C", "Teusaquillo", 4.64, -74.09});
    g.agregarLugar({3, "Destino D", "Suba", 4.71, -74.08});
    g.agregarLugar({4, "Aislado E", "Usme", 4.50, -74.10});

    // Ruta por B: corta (2 + 2 = 4 km) pero lenta (15 km/h con trafico 2.0x)
    const int viaAB = g.agregarArista({0, 0, 1, "Via Corta Congestionada 1", 2.0, 15.0, 2.0, true, true});
    const int viaBD = g.agregarArista({1, 1, 3, "Via Corta Congestionada 2", 2.0, 15.0, 2.0, true, true});

    // Ruta por C: mas larga (3.5 + 3.5 = 7 km) pero autopista rapida (60 km/h, trafico 1.0x)
    g.agregarArista({2, 0, 2, "Autopista Rapida 1", 3.5, 60.0, 1.0, true, true});
    g.agregarArista({3, 2, 3, "Autopista Rapida 2", 3.5, 60.0, 1.0, true, true});

    SECTION("Optimizacion por DISTANCIA_KM elige el camino mas corto en kilometros (A -> B -> D)") {
        const ResultadoRuta res = Dijkstra::calcularRuta(g, 0, 3, CriterioPeso::DISTANCIA_KM, true);
        REQUIRE(res.encontrada);
        REQUIRE(res.distanciaTotalKm == Approx(4.0));
        REQUIRE(res.caminoNodos == std::vector<int>{0, 1, 3});
        REQUIRE_FALSE(res.pasos.empty());
    }

    SECTION("Optimizacion por TIEMPO_MINUTOS elige la autopista mas rapida (A -> C -> D)") {
        const ResultadoRuta res = Dijkstra::calcularRuta(g, 0, 3, CriterioPeso::TIEMPO_MINUTOS, false);
        REQUIRE(res.encontrada);
        REQUIRE(res.caminoNodos == std::vector<int>{0, 2, 3});
        REQUIRE(res.tiempoTotalMin == Approx(7.0)); // 7 km a 60 km/h = 7 minutos
        REQUIRE(res.distanciaTotalKm == Approx(7.0));
    }

    SECTION("Cierre de via obliga a recalcular por ruta alterna") {
        REQUIRE(g.establecerEstadoVia(viaBD, false));
        const ResultadoRuta res = Dijkstra::calcularRuta(g, 0, 3, CriterioPeso::DISTANCIA_KM, false);
        REQUIRE(res.encontrada);
        REQUIRE(res.caminoNodos == std::vector<int>{0, 2, 3});
        REQUIRE(res.distanciaTotalKm == Approx(7.0));

        // Si cerramos tambien la autopista, el destino queda inalcanzable
        REQUIRE(g.establecerEstadoVia(viaAB, false));
        REQUIRE(g.establecerEstadoVia(2, false));
        const ResultadoRuta sinRuta = Dijkstra::calcularRuta(g, 0, 3, CriterioPeso::DISTANCIA_KM, false);
        REQUIRE_FALSE(sinRuta.encontrada);
    }

    SECTION("Origen igual a destino retorna costo cero y un solo nodo") {
        const ResultadoRuta res = Dijkstra::calcularRuta(g, 1, 1, CriterioPeso::DISTANCIA_KM, true);
        REQUIRE(res.encontrada);
        REQUIRE(res.costoTotal == Approx(0.0));
        REQUIRE(res.caminoNodos == std::vector<int>{1});
        REQUIRE(res.caminoAristas.empty());
    }

    SECTION("Nodo desconectado retorna encontrada = false") {
        const ResultadoRuta res = Dijkstra::calcularRuta(g, 0, 4, CriterioPeso::DISTANCIA_KM, false);
        REQUIRE_FALSE(res.encontrada);
        REQUIRE(res.caminoNodos.empty());
    }
}

TEST_CASE("Carga de dataset real data/bogota.json y calculo de rutas en Bogota", "[json][bogota]") {
    Grafo bogota;
    const std::string ruta = buscarBogotaJson();
    REQUIRE(bogota.cargarDesdeJSON(ruta));
    REQUIRE(bogota.numeroLugares() >= 20);
    REQUIRE(bogota.numeroAristas() >= 35);

    // Ruta entre UDistrital Sede Macarena (3) y UDistrital Facultad Tecnologica (21)
    const ResultadoRuta rutaUD = Dijkstra::calcularRuta(bogota, 3, 21, CriterioPeso::DISTANCIA_KM, true);
    REQUIRE(rutaUD.encontrada);
    REQUIRE(rutaUD.caminoNodos.front() == 3);
    REQUIRE(rutaUD.caminoNodos.back() == 21);
    REQUIRE(rutaUD.distanciaTotalKm > 10.0);
    REQUIRE(rutaUD.tiempoTotalMin > 15.0);
    REQUIRE(rutaUD.pasos.size() >= 3);
}
