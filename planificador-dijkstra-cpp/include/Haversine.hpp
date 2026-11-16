#pragma once

namespace planificador {

/**
 * @brief Utilidades geográficas para el cálculo de distancias sobre la esfera terrestre
 *        mediante la fórmula de Haversine y estimación de tiempos de desplazamiento.
 */
class Haversine {
public:
    /// Radio medio de la Tierra en kilómetros (WGS84 aproximado).
    static constexpr double RADIO_TIERRA_KM = 6371.0;

    /**
     * @brief Convierte un ángulo expresado en grados decimales a radianes.
     */
    static double gradosARadianes(double grados);

    /**
     * @brief Calcula la distancia ortodrómica (en línea recta sobre la curvatura terrestre)
     *        entre dos coordenadas geográficas (latitud, longitud) en kilómetros.
     */
    static double calcularDistanciaKm(double lat1, double lon1, double lat2, double lon2);

    /**
     * @brief Calcula el tiempo estimado de viaje en minutos dada la distancia (km),
     *        velocidad promedio (km/h) y un factor de tráfico multiplicador (>= 1.0).
     */
    static double calcularTiempoMinutos(double distanciaKm, double velocidadKmh, double factorTrafico = 1.0);
};

} // namespace planificador
