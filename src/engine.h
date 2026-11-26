#pragma once

#include <QLocale>
#include <QString>
#include <QVector>

#include "units.h"

enum class CurrencyId {
    None,
    USD,
    EUR,
    GBP,
    JPY,
    Kr
};

enum class NumberBase {
    Dec,
    Hex,
    Bin,
    Oct
};

struct CalcValue {
    double number = 0.0;
    UnitId unit = UnitId::None;
    CurrencyId currency = CurrencyId::None;
    NumberBase base = NumberBase::Dec;
    bool isPercent = false;
};

enum class LineKind {
    Empty,
    Heading,
    Comment,
    Note,
    Assignment,
    Expression,
    Error
};

struct LineResult {
    int lineNumber = 0;
    int charOffset = 0;
    LineKind kind = LineKind::Empty;
    QString variableName;
    CalcValue value;
    QString displayText;
    QString copyText;
    QString errorText;
};

struct VariableEntry {
    QString name;
    QString displayText;
    QString copyText;
    int lineNumber = 0;
};

struct SheetResult {
    QVector<LineResult> lines;
    QVector<VariableEntry> variables;
    bool hasErrors = false;
};

class Engine {
public:
    static SheetResult evaluate(const QString &text,
                                const QLocale &displayLocale = QLocale::c(),
                                int precision = 6);
    static QString formatValue(const CalcValue &value,
                               const QLocale &locale,
                               int precision = 6);
    static QString formatNumber(double value,
                                const QLocale &locale,
                                int precision = 6);
};
