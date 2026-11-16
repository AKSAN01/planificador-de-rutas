#include "Grafo.hpp"
#include "Haversine.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace planificador {

const std::vector<Arista> Grafo::kListaVacia_{};

void Grafo::limpiar() {
    lugares_.clear();
    indiceLugarPorId_.clear();
    aristas_.clear();
    indiceAristaPorId_.clear();
    listaAdyacencia_.clear();
    boundingBox_ = BoundingBoxGeo{};
}

void Grafo::agregarLugar(const Lugar& lugar) {
    if (lugar.id < 0) {
        return;
    }
    const auto capRequerida = static_cast<std::size_t>(lugar.id) + 1;
    if (capRequerida > listaAdyacencia_.size()) {
        listaAdyacencia_.resize(capRequerida);
    }

    auto it = indiceLugarPorId_.find(lugar.id);
    if (it != indiceLugarPorId_.end()) {
        lugares_[it->second] = lugar;
    } else {
        indiceLugarPorId_[lugar.id] = lugares_.size();
        lugares_.push_back(lugar);
    }
}

int Grafo::agregarArista(const Arista& aristaEntrada) {
    if (!existeLugar(aristaEntrada.origen) || !existeLugar(aristaEntrada.destino)) {
        return -1;
    }

    Arista nueva = aristaEntrada;
    if (nueva.id < 0) {
        nueva.id = static_cast<int>(aristas_.size());
    }

    if (nueva.distanciaKm <= 0.0) {
        const Lugar* lOrigen = obtenerLugar(nueva.origen);
        const Lugar* lDestino = obtenerLugar(nueva.destino);
        if (lOrigen && lDestino) {
            // Factor de sinuosidad urbana (1.18x sobre línea recta ortodrómica)
            nueva.distanciaKm = Haversine::calcularDistanciaKm(
                lOrigen->latitud, lOrigen->longitud,
                lDestino->latitud, lDestino->longitud) * 1.18;
        }
    }

    if (nueva.velocidadPromedioKmh <= 0.0) {
        nueva.velocidadPromedioKmh = 35.0;
    }
    if (nueva.factorTrafico <= 0.0) {
        nueva.factorTrafico = 1.0;
    }

    indiceAristaPorId_[nueva.id] = aristas_.size();
    aristas_.push_back(nueva);

    listaAdyacencia_[static_cast<std::size_t>(nueva.origen)].push_back(nueva);

    if (nueva.bidireccional && nueva.origen != nueva.destino) {
        Arista inversa = nueva;
        std::swap(inversa.origen, inversa.destino);
        listaAdyacencia_[static_cast<std::size_t>(inversa.origen)].push_back(inversa);
    }

    return nueva.id;
}

bool Grafo::establecerEstadoVia(int idArista, bool activa) {
    auto it = indiceAristaPorId_.find(idArista);
    if (it == indiceAristaPorId_.end()) {
        return false;
    }

    aristas_[it->second].activa = activa;

    for (auto& lista : listaAdyacencia_) {
        for (auto& arista : lista) {
            if (arista.id == idArista) {
                arista.activa = activa;
            }
        }
    }
    return true;
}

bool Grafo::alternarEstadoVia(int idArista) {
    const Arista* arista = obtenerArista(idArista);
    if (!arista) {
        return false;
    }
    return establecerEstadoVia(idArista, !arista->activa);
}

bool Grafo::establecerFactorTrafico(int idArista, double factorTrafico) {
    auto it = indiceAristaPorId_.find(idArista);
    if (it == indiceAristaPorId_.end()) {
        return false;
    }

    const double factorSeguro = std::max(0.2, factorTrafico);
    aristas_[it->second].factorTrafico = factorSeguro;

    for (auto& lista : listaAdyacencia_) {
        for (auto& arista : lista) {
            if (arista.id == idArista) {
                arista.factorTrafico = factorSeguro;
            }
        }
    }
    return true;
}

void Grafo::activarTodasLasVias() {
    for (auto& arista : aristas_) {
        arista.activa = true;
    }
    for (auto& lista : listaAdyacencia_) {
        for (auto& arista : lista) {
            arista.activa = true;
        }
    }
}

const std::vector<Arista>& Grafo::obtenerAdyacentes(int idNodo) const {
    if (idNodo < 0 || static_cast<std::size_t>(idNodo) >= listaAdyacencia_.size()) {
        return kListaVacia_;
    }
    return listaAdyacencia_[static_cast<std::size_t>(idNodo)];
}

bool Grafo::existeLugar(int idNodo) const {
    return indiceLugarPorId_.find(idNodo) != indiceLugarPorId_.end();
}

const Lugar* Grafo::obtenerLugar(int idNodo) const {
    auto it = indiceLugarPorId_.find(idNodo);
    if (it == indiceLugarPorId_.end()) {
        return nullptr;
    }
    return &lugares_[it->second];
}

const Arista* Grafo::obtenerArista(int idArista) const {
    auto it = indiceAristaPorId_.find(idArista);
    if (it == indiceAristaPorId_.end()) {
        return nullptr;
    }
    return &aristas_[it->second];
}

bool Grafo::cargarDesdeJSON(const std::string& rutaArchivo) {
    std::ifstream archivo(rutaArchivo);
    if (!archivo.is_open()) {
        std::cerr << "[Grafo] No se pudo abrir el archivo JSON: " << rutaArchivo << "\n";
        return false;
    }

    try {
        nlohmann::json j;
        archivo >> j;

        limpiar();

        if (j.contains("bounding_box") && j["bounding_box"].is_object()) {
            const auto& bb = j["bounding_box"];
            boundingBox_.latMin = bb.value("lat_min", 4.555);
            boundingBox_.latMax = bb.value("lat_max", 4.770);
            boundingBox_.lonMin = bb.value("lon_min", -74.165);
            boundingBox_.lonMax = bb.value("lon_max", -74.015);
        }

        if (!j.contains("lugares") || !j["lugares"].is_array()) {
            std::cerr << "[Grafo] El JSON no contiene el arreglo 'lugares'.\n";
            return false;
        }

        for (const auto& item : j["lugares"]) {
            Lugar lugar;
            lugar.id = item.at("id").get<int>();
            lugar.nombre = item.at("nombre").get<std::string>();
            lugar.localidad = item.value("localidad", "Bogotá D.C.");
            lugar.latitud = item.at("latitud").get<double>();
            lugar.longitud = item.at("longitud").get<double>();
            agregarLugar(lugar);
        }

        const std::string claveAristas = j.contains("vias") ? "vias" : "aristas";
        if (j.contains(claveAristas) && j[claveAristas].is_array()) {
            int idSecuencial = 0;
            for (const auto& item : j[claveAristas]) {
                Arista arista;
                arista.id = item.value("id", idSecuencial++);
                arista.origen = item.at("origen").get<int>();
                arista.destino = item.at("destino").get<int>();
                arista.nombreVia = item.value("nombre_via", item.value("nombre", "Vía Urbana"));
                arista.distanciaKm = item.value("distancia_km", 0.0);
                arista.velocidadPromedioKmh = item.value("velocidad_kmh", 38.0);
                arista.factorTrafico = item.value("factor_trafico", 1.0);
                arista.activa = item.value("activa", true);
                arista.bidireccional = item.value("bidireccional", true);
                agregarArista(arista);
            }
        }

        return !lugares_.empty();
    } catch (const std::exception& e) {
        std::cerr << "[Grafo] Error al parsear JSON (" << rutaArchivo << "): " << e.what() << "\n";
        return false;
    }
}

} // namespace planificador
