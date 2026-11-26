#pragma once

#include <QString>
#include <optional>

enum class UnitDim {
    None,
    Length,
    Mass,
    Temperature,
    Time,
    Data,
    Area,
    Volume
};

enum class UnitId {
    None,
    // Length (base: meter)
    Mm, Cm, M, Km, In, Ft, Yd, Mi, Nmi,
    // Mass (base: gram)
    Mg, G, Kg, Tonne, Oz, Lb,
    // Temperature (base: Celsius)
    Celsius, Fahrenheit, Kelvin,
    // Time (base: second)
    Ms, Sec, Min, Hour, Day, Week, Year,
    // Data (base: byte)
    Bit, Byte, KB, MB, GB, TB, KiB, MiB, GiB, TiB,
    // Area (base: square meter)
    Mm2, Cm2, M2, Km2, Ha, SqFt, Acre,
    // Volume (base: liter)
    Ml, Cl, Dl, Liter, Gal, Qt, Pt, Cup, FlOz, Tbsp, Tsp
};

std::optional<UnitId> lookupUnit(const QString &token);
UnitDim unitDimension(UnitId id);
QString unitSymbol(UnitId id);
double toBase(double value, UnitId id);
double fromBase(double baseValue, UnitId id);
bool convertUnit(double value, UnitId from, UnitId to, double &out);
