#include "units.h"

#include <QHash>

namespace {

struct UnitInfo {
    UnitId id;
    UnitDim dim;
    const char *symbol;
    double scaleToBase;
};

constexpr UnitInfo kUnits[] = {
    {UnitId::None, UnitDim::None, "", 1.0},

    // Length (base: m)
    {UnitId::Mm,  UnitDim::Length, "mm",  0.001},
    {UnitId::Cm,  UnitDim::Length, "cm",  0.01},
    {UnitId::M,   UnitDim::Length, "m",   1.0},
    {UnitId::Km,  UnitDim::Length, "km",  1000.0},
    {UnitId::In,  UnitDim::Length, "in",  0.0254},
    {UnitId::Ft,  UnitDim::Length, "ft",  0.3048},
    {UnitId::Yd,  UnitDim::Length, "yd",  0.9144},
    {UnitId::Mi,  UnitDim::Length, "mi",  1609.344},
    {UnitId::Nmi, UnitDim::Length, "nmi", 1852.0},

    // Mass (base: g)
    {UnitId::Mg,    UnitDim::Mass, "mg", 0.001},
    {UnitId::G,     UnitDim::Mass, "g",  1.0},
    {UnitId::Kg,    UnitDim::Mass, "kg", 1000.0},
    {UnitId::Tonne, UnitDim::Mass, "t",  1000000.0},
    {UnitId::Oz,    UnitDim::Mass, "oz", 28.349523125},
    {UnitId::Lb,    UnitDim::Mass, "lb", 453.59237},

    // Temperature (base: Celsius; affine conversions handled explicitly)
    {UnitId::Celsius,    UnitDim::Temperature, "°C", 1.0},
    {UnitId::Fahrenheit, UnitDim::Temperature, "°F", 1.0},
    {UnitId::Kelvin,     UnitDim::Temperature, "K",  1.0},

    // Time (base: s)
    {UnitId::Ms,   UnitDim::Time, "ms",   0.001},
    {UnitId::Sec,  UnitDim::Time, "s",    1.0},
    {UnitId::Min,  UnitDim::Time, "min",  60.0},
    {UnitId::Hour, UnitDim::Time, "h",    3600.0},
    {UnitId::Day,  UnitDim::Time, "d",    86400.0},
    {UnitId::Week, UnitDim::Time, "wk",   604800.0},
    {UnitId::Year, UnitDim::Time, "yr",   31557600.0},

    // Data (base: byte)
    {UnitId::Bit,  UnitDim::Data, "bit",  0.125},
    {UnitId::Byte, UnitDim::Data, "B",    1.0},
    {UnitId::KB,   UnitDim::Data, "KB",   1000.0},
    {UnitId::MB,   UnitDim::Data, "MB",   1000.0 * 1000.0},
    {UnitId::GB,   UnitDim::Data, "GB",   1000.0 * 1000.0 * 1000.0},
    {UnitId::TB,   UnitDim::Data, "TB",   1000.0 * 1000.0 * 1000.0 * 1000.0},
    {UnitId::KiB,  UnitDim::Data, "KiB",  1024.0},
    {UnitId::MiB,  UnitDim::Data, "MiB",  1024.0 * 1024.0},
    {UnitId::GiB,  UnitDim::Data, "GiB",  1024.0 * 1024.0 * 1024.0},
    {UnitId::TiB,  UnitDim::Data, "TiB",  1024.0 * 1024.0 * 1024.0 * 1024.0},

    // Area (base: m2)
    {UnitId::Mm2,  UnitDim::Area, "mm²",  1e-6},
    {UnitId::Cm2,  UnitDim::Area, "cm²",  1e-4},
    {UnitId::M2,   UnitDim::Area, "m²",   1.0},
    {UnitId::Km2,  UnitDim::Area, "km²",  1e6},
    {UnitId::Ha,   UnitDim::Area, "ha",   10000.0},
    {UnitId::SqFt, UnitDim::Area, "sqft", 0.09290304},
    {UnitId::Acre, UnitDim::Area, "acre", 4046.8564224},

    // Volume (base: liter)
    {UnitId::Ml,    UnitDim::Volume, "mL",   0.001},
    {UnitId::Cl,    UnitDim::Volume, "cL",   0.01},
    {UnitId::Dl,    UnitDim::Volume, "dL",   0.1},
    {UnitId::Liter, UnitDim::Volume, "L",    1.0},
    {UnitId::Gal,   UnitDim::Volume, "gal",  3.785411784},
    {UnitId::Qt,    UnitDim::Volume, "qt",   0.946352946},
    {UnitId::Pt,    UnitDim::Volume, "pt",   0.473176473},
    {UnitId::Cup,   UnitDim::Volume, "cup",  0.2365882365},
    {UnitId::FlOz,  UnitDim::Volume, "floz", 0.0295735295625},
    {UnitId::Tbsp,  UnitDim::Volume, "tbsp", 0.01478676478125},
    {UnitId::Tsp,   UnitDim::Volume, "tsp",  0.00492892159375},
};

const UnitInfo &infoFor(UnitId id) {
    const int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(std::size(kUnits)))
        return kUnits[0];
    return kUnits[idx];
}

const QHash<QString, UnitId> &exactMap() {
    static const QHash<QString, UnitId> map = {
        {"B", UnitId::Byte},
        {"b", UnitId::Bit},
        {"C", UnitId::Celsius},
        {"°C", UnitId::Celsius},
        {"F", UnitId::Fahrenheit},
        {"°F", UnitId::Fahrenheit},
        {"K", UnitId::Kelvin},
        {"L", UnitId::Liter},
        {"l", UnitId::Liter},
        {"m", UnitId::M},
        {"g", UnitId::G},
        {"t", UnitId::Tonne},
        {"s", UnitId::Sec},
        {"h", UnitId::Hour},
        {"d", UnitId::Day},
        {"KB", UnitId::KB},
        {"MB", UnitId::MB},
        {"GB", UnitId::GB},
        {"TB", UnitId::TB},
        {"KiB", UnitId::KiB},
        {"MiB", UnitId::MiB},
        {"GiB", UnitId::GiB},
        {"TiB", UnitId::TiB},
        {"mL", UnitId::Ml},
        {"cL", UnitId::Cl},
        {"dL", UnitId::Dl},
    };
    return map;
}

const QHash<QString, UnitId> &lowerMap() {
    static const QHash<QString, UnitId> map = {
        // Length
        {"mm", UnitId::Mm}, {"millimeter", UnitId::Mm}, {"millimeters", UnitId::Mm},
        {"millimetre", UnitId::Mm}, {"millimetres", UnitId::Mm},
        {"cm", UnitId::Cm}, {"centimeter", UnitId::Cm}, {"centimeters", UnitId::Cm},
        {"centimetre", UnitId::Cm}, {"centimetres", UnitId::Cm},
        {"meter", UnitId::M}, {"meters", UnitId::M}, {"metre", UnitId::M}, {"metres", UnitId::M},
        {"km", UnitId::Km}, {"kilometer", UnitId::Km}, {"kilometers", UnitId::Km},
        {"kilometre", UnitId::Km}, {"kilometres", UnitId::Km},
        {"in", UnitId::In}, {"inch", UnitId::In}, {"inches", UnitId::In},
        {"ft", UnitId::Ft}, {"foot", UnitId::Ft}, {"feet", UnitId::Ft},
        {"yd", UnitId::Yd}, {"yard", UnitId::Yd}, {"yards", UnitId::Yd},
        {"mi", UnitId::Mi}, {"mile", UnitId::Mi}, {"miles", UnitId::Mi},
        {"nmi", UnitId::Nmi},

        // Mass
        {"mg", UnitId::Mg}, {"milligram", UnitId::Mg}, {"milligrams", UnitId::Mg},
        {"gram", UnitId::G}, {"grams", UnitId::G},
        {"kg", UnitId::Kg}, {"kilogram", UnitId::Kg}, {"kilograms", UnitId::Kg},
        {"tonne", UnitId::Tonne}, {"tonnes", UnitId::Tonne},
        {"oz", UnitId::Oz}, {"ounce", UnitId::Oz}, {"ounces", UnitId::Oz},
        {"lb", UnitId::Lb}, {"lbs", UnitId::Lb}, {"pound", UnitId::Lb}, {"pounds", UnitId::Lb},

        // Temperature
        {"°c", UnitId::Celsius}, {"celsius", UnitId::Celsius},
        {"°f", UnitId::Fahrenheit}, {"fahrenheit", UnitId::Fahrenheit},
        {"kelvin", UnitId::Kelvin},

        // Time
        {"ms", UnitId::Ms}, {"millisecond", UnitId::Ms}, {"milliseconds", UnitId::Ms},
        {"sec", UnitId::Sec}, {"secs", UnitId::Sec}, {"second", UnitId::Sec}, {"seconds", UnitId::Sec},
        {"min", UnitId::Min}, {"mins", UnitId::Min}, {"minute", UnitId::Min}, {"minutes", UnitId::Min},
        {"hr", UnitId::Hour}, {"hrs", UnitId::Hour}, {"hour", UnitId::Hour}, {"hours", UnitId::Hour},
        {"day", UnitId::Day}, {"days", UnitId::Day},
        {"wk", UnitId::Week}, {"wks", UnitId::Week}, {"week", UnitId::Week}, {"weeks", UnitId::Week},
        {"yr", UnitId::Year}, {"yrs", UnitId::Year}, {"year", UnitId::Year}, {"years", UnitId::Year},

        // Data
        {"bit", UnitId::Bit}, {"bits", UnitId::Bit},
        {"byte", UnitId::Byte}, {"bytes", UnitId::Byte},
        {"kb", UnitId::KB}, {"mb", UnitId::MB}, {"gb", UnitId::GB}, {"tb", UnitId::TB},
        {"kib", UnitId::KiB}, {"mib", UnitId::MiB}, {"gib", UnitId::GiB}, {"tib", UnitId::TiB},

        // Area
        {"mm2", UnitId::Mm2}, {"mm²", UnitId::Mm2},
        {"cm2", UnitId::Cm2}, {"cm²", UnitId::Cm2},
        {"m2", UnitId::M2}, {"m²", UnitId::M2}, {"sqm", UnitId::M2},
        {"km2", UnitId::Km2}, {"km²", UnitId::Km2},
        {"ha", UnitId::Ha}, {"hectare", UnitId::Ha}, {"hectares", UnitId::Ha},
        {"sqft", UnitId::SqFt}, {"ft2", UnitId::SqFt}, {"ft²", UnitId::SqFt},
        {"acre", UnitId::Acre}, {"acres", UnitId::Acre},

        // Volume
        {"ml", UnitId::Ml}, {"milliliter", UnitId::Ml}, {"milliliters", UnitId::Ml},
        {"millilitre", UnitId::Ml}, {"millilitres", UnitId::Ml},
        {"cl", UnitId::Cl}, {"dl", UnitId::Dl},
        {"liter", UnitId::Liter}, {"liters", UnitId::Liter},
        {"litre", UnitId::Liter}, {"litres", UnitId::Liter},
        {"gal", UnitId::Gal}, {"gallon", UnitId::Gal}, {"gallons", UnitId::Gal},
        {"qt", UnitId::Qt}, {"quart", UnitId::Qt}, {"quarts", UnitId::Qt},
        {"pt", UnitId::Pt}, {"pint", UnitId::Pt}, {"pints", UnitId::Pt},
        {"cup", UnitId::Cup}, {"cups", UnitId::Cup},
        {"floz", UnitId::FlOz},
        {"tbsp", UnitId::Tbsp}, {"tsp", UnitId::Tsp},
    };
    return map;
}

} // namespace

std::optional<UnitId> lookupUnit(const QString &token) {
    if (token.isEmpty())
        return std::nullopt;
    const auto exactIt = exactMap().constFind(token);
    if (exactIt != exactMap().constEnd())
        return exactIt.value();
    const auto lowerIt = lowerMap().constFind(token.toLower());
    if (lowerIt != lowerMap().constEnd())
        return lowerIt.value();
    return std::nullopt;
}

UnitDim unitDimension(UnitId id) {
    return infoFor(id).dim;
}

QString unitSymbol(UnitId id) {
    return QString::fromUtf8(infoFor(id).symbol);
}

double toBase(double value, UnitId id) {
    switch (id) {
    case UnitId::Celsius:
        return value;
    case UnitId::Fahrenheit:
        return (value - 32.0) * 5.0 / 9.0;
    case UnitId::Kelvin:
        return value - 273.15;
    default:
        return value * infoFor(id).scaleToBase;
    }
}

double fromBase(double baseValue, UnitId id) {
    switch (id) {
    case UnitId::Celsius:
        return baseValue;
    case UnitId::Fahrenheit:
        return baseValue * 9.0 / 5.0 + 32.0;
    case UnitId::Kelvin:
        return baseValue + 273.15;
    default:
        return baseValue / infoFor(id).scaleToBase;
    }
}

bool convertUnit(double value, UnitId from, UnitId to, double &out) {
    if (from == UnitId::None || to == UnitId::None)
        return false;
    if (unitDimension(from) != unitDimension(to))
        return false;
    if (from == to) {
        out = value;
        return true;
    }
    out = fromBase(toBase(value, from), to);
    return true;
}
