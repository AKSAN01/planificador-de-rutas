#include "Haversine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace planificador {

namespace {
constexpr double PI = 3.14159265358979323846;
} // namespace

double Haversine::gradosARadianes(double grados) {
    return grados * (PI / 180.0);
}

double Haversine::calcularDistanciaKm(double lat1, double lon1, double lat2, double lon2) {
    const double dLat = gradosARadianes(lat2 - lat1);
    const double dLon = gradosARadianes(lon2 - lon1);

    const double radLat1 = gradosARadianes(lat1);
    const double radLat2 = gradosARadianes(lat2);

    const double sinDLat = std::sin(dLat / 2.0);
    const double sinDLon = std::sin(dLon / 2.0);

    double a = sinDLat * sinDLat +
               std::cos(radLat1) * std::cos(radLat2) * sinDLon * sinDLon;
    a = std::clamp(a, 0.0, 1.0);

    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return RADIO_TIERRA_KM * c;
}

double Haversine::calcularTiempoMinutos(double distanciaKm, double velocidadKmh, double factorTrafico) {
    if (velocidadKmh <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    if (distanciaKm <= 0.0) {
        return 0.0;
    }
    const double factorEfectivo = (factorTrafico > 0.0) ? factorTrafico : 1.0;
    return (distanciaKm / velocidadKmh) * 60.0 * factorEfectivo;
}

} // namespace planificador
