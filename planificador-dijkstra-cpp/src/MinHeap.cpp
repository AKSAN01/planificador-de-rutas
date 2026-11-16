#include "MinHeap.hpp"

#include <limits>
#include <stdexcept>

namespace planificador {

MinHeap::MinHeap(std::size_t capacidadMaximaNodos) {
    datos_.reserve(capacidadMaximaNodos);
    posicionEnHeap_.assign(capacidadMaximaNodos, -1);
}

void MinHeap::asegurarCapacidadNodo(int nodo) {
    if (nodo < 0) {
        throw std::invalid_argument("El identificador del nodo en MinHeap no puede ser negativo.");
    }
    const auto requerido = static_cast<std::size_t>(nodo) + 1;
    if (requerido > posicionEnHeap_.size()) {
        posicionEnHeap_.resize(requerido, -1);
    }
}

void MinHeap::intercambiar(std::size_t i, std::size_t j) {
    if (i == j) {
        return;
    }
    posicionEnHeap_[static_cast<std::size_t>(datos_[i].nodo)] = static_cast<int>(j);
    posicionEnHeap_[static_cast<std::size_t>(datos_[j].nodo)] = static_cast<int>(i);
    std::swap(datos_[i], datos_[j]);
}

void MinHeap::flotar(std::size_t indice) {
    while (indice > 0) {
        const std::size_t idxPadre = padre(indice);
        if (datos_[indice].prioridad < datos_[idxPadre].prioridad ||
            (datos_[indice].prioridad == datos_[idxPadre].prioridad &&
             datos_[indice].nodo < datos_[idxPadre].nodo)) {
            intercambiar(indice, idxPadre);
            indice = idxPadre;
        } else {
            break;
        }
    }
}

void MinHeap::hundir(std::size_t indice) {
    const std::size_t n = datos_.size();
    while (true) {
        const std::size_t izq = hijoIzquierdo(indice);
        const std::size_t der = hijoDerecho(indice);
        std::size_t menor = indice;

        auto esMenor = [this](std::size_t a, std::size_t b) {
            if (datos_[a].prioridad != datos_[b].prioridad) {
                return datos_[a].prioridad < datos_[b].prioridad;
            }
            return datos_[a].nodo < datos_[b].nodo;
        };

        if (izq < n && esMenor(izq, menor)) {
            menor = izq;
        }
        if (der < n && esMenor(der, menor)) {
            menor = der;
        }

        if (menor != indice) {
            intercambiar(indice, menor);
            indice = menor;
        } else {
            break;
        }
    }
}

void MinHeap::insertar(int nodo, double prioridad) {
    asegurarCapacidadNodo(nodo);
    const int posExistente = posicionEnHeap_[static_cast<std::size_t>(nodo)];
    if (posExistente != -1) {
        disminuirPrioridad(nodo, prioridad);
        return;
    }

    const std::size_t nuevoIndice = datos_.size();
    datos_.push_back(ElementoHeap{nodo, prioridad});
    posicionEnHeap_[static_cast<std::size_t>(nodo)] = static_cast<int>(nuevoIndice);
    flotar(nuevoIndice);
}

void MinHeap::disminuirPrioridad(int nodo, double nuevaPrioridad) {
    asegurarCapacidadNodo(nodo);
    const int pos = posicionEnHeap_[static_cast<std::size_t>(nodo)];
    if (pos == -1) {
        insertar(nodo, nuevaPrioridad);
        return;
    }

    const auto idx = static_cast<std::size_t>(pos);
    const double prioridadAnterior = datos_[idx].prioridad;
    if (nuevaPrioridad <= prioridadAnterior) {
        datos_[idx].prioridad = nuevaPrioridad;
        flotar(idx);
    }
}

ElementoHeap MinHeap::extraerMinimo() {
    if (datos_.empty()) {
        throw std::underflow_error("No se puede extraer el minimo de un MinHeap vacio.");
    }

    const ElementoHeap minimo = datos_.front();
    posicionEnHeap_[static_cast<std::size_t>(minimo.nodo)] = -1;

    if (datos_.size() == 1) {
        datos_.pop_back();
        return minimo;
    }

    datos_[0] = datos_.back();
    posicionEnHeap_[static_cast<std::size_t>(datos_[0].nodo)] = 0;
    datos_.pop_back();

    hundir(0);
    return minimo;
}

ElementoHeap MinHeap::obtenerMinimo() const {
    if (datos_.empty()) {
        throw std::underflow_error("El MinHeap esta vacio.");
    }
    return datos_.front();
}

bool MinHeap::contiene(int nodo) const {
    if (nodo < 0 || static_cast<std::size_t>(nodo) >= posicionEnHeap_.size()) {
        return false;
    }
    return posicionEnHeap_[static_cast<std::size_t>(nodo)] != -1;
}

double MinHeap::obtenerPrioridad(int nodo) const {
    if (!contiene(nodo)) {
        return std::numeric_limits<double>::infinity();
    }
    const int pos = posicionEnHeap_[static_cast<std::size_t>(nodo)];
    return datos_[static_cast<std::size_t>(pos)].prioridad;
}

bool MinHeap::estaVacio() const noexcept {
    return datos_.empty();
}

std::size_t MinHeap::tamano() const noexcept {
    return datos_.size();
}

void MinHeap::limpiar() noexcept {
    datos_.clear();
    std::fill(posicionEnHeap_.begin(), posicionEnHeap_.end(), -1);
}

} // namespace planificador
