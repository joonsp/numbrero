#include "engine.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <optional>

namespace {

enum class TokenKind {
    End,
    Number,
    Ident,
    CurrencySymbol,
    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    Percent,
    LParen,
    RParen,
    Comma,
    Arrow,
    Invalid
};

struct Token {
    TokenKind kind = TokenKind::End;
    QString text;
    double number = 0.0;
};

const QSet<QString> &builtinConstants() {
    static const QSet<QString> set = {"pi", "e", "tau"};
    return set;
}

const QSet<QString> &builtinAggregates() {
    static const QSet<QString> set = {
        "sum", "total", "avg", "average", "mean", "count", "prev", "ans"
    };
    return set;
}

const QSet<QString> &builtinFunctions() {
    static const QSet<QString> set = {
        "sqrt", "cbrt", "abs", "sign", "round", "floor", "ceil", "trunc",
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "deg", "rad",
        "log", "ln", "log2", "exp", "pow", "min", "max", "clamp", "sum", "avg"
    };
    return set;
}

std::optional<CurrencyId> currencyFromSymbolChar(QChar ch) {
    switch (ch.unicode()) {
    case '$': return CurrencyId::USD;
    case 0x20AC: return CurrencyId::EUR; // €
    case 0x00A3: return CurrencyId::GBP; // £
    case 0x00A5: return CurrencyId::JPY; // ¥
    default: return std::nullopt;
    }
}

std::optional<CurrencyId> lookupCurrency(const QString &token) {
    if (token.size() == 1)
        return currencyFromSymbolChar(token.at(0));
    const QString upper = token.toUpper();
    if (upper == QStringLiteral("USD")) return CurrencyId::USD;
    if (upper == QStringLiteral("EUR")) return CurrencyId::EUR;
    if (upper == QStringLiteral("GBP")) return CurrencyId::GBP;
    if (upper == QStringLiteral("JPY")) return CurrencyId::JPY;
    if (upper == QStringLiteral("KR"))  return CurrencyId::Kr;
    return std::nullopt;
}

QString currencySymbol(CurrencyId id) {
    switch (id) {
    case CurrencyId::USD: return QStringLiteral("$");
    case CurrencyId::EUR: return QStringLiteral("€");
    case CurrencyId::GBP: return QStringLiteral("£");
    case CurrencyId::JPY: return QStringLiteral("¥");
    case CurrencyId::Kr:  return QStringLiteral("kr");
    case CurrencyId::None: break;
    }
    return {};
}

std::optional<NumberBase> lookupBase(const QString &token) {
    const QString lower = token.toLower();
    if (lower == QStringLiteral("hex")) return NumberBase::Hex;
    if (lower == QStringLiteral("bin")) return NumberBase::Bin;
    if (lower == QStringLiteral("oct")) return NumberBase::Oct;
    if (lower == QStringLiteral("dec")) return NumberBase::Dec;
    return std::nullopt;
}

bool isIdentStart(QChar ch) {
    return ch.isLetter() || ch == QLatin1Char('_') || ch == QChar(0x00B0);
}

bool isIdentPart(QChar ch) {
    return ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QChar(0x00B2);
}

bool isRadixDigit(QChar ch, int radix) {
    const ushort u = ch.unicode();
    if (radix == 2)
        return u == '0' || u == '1';
    if (radix == 8)
        return u >= '0' && u <= '7';
    if (radix == 16)
        return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F');
    return false;
}

QVector<Token> tokenize(const QString &expr, QString &errorOut) {
    QVector<Token> tokens;
    int i = 0;
    const int n = expr.size();

    while (i < n) {
        const QChar ch = expr.at(i);
        if (ch.isSpace()) {
            ++i;
            continue;
        }

        // Prefixed base literals: 0x..., 0b..., 0o...
        if (ch == QLatin1Char('0') && i + 1 < n) {
            const QChar p = expr.at(i + 1).toLower();
            int radix = 0;
            if (p == QLatin1Char('x')) radix = 16;
            else if (p == QLatin1Char('b')) radix = 2;
            else if (p == QLatin1Char('o')) radix = 8;

            if (radix != 0) {
                const int start = i;
                i += 2;
                QString digits;
                while (i < n) {
                    const QChar c = expr.at(i);
                    if (isRadixDigit(c, radix)) {
                        digits.append(c);
                        ++i;
                    } else if (c == QLatin1Char('_')) {
                        ++i;
                    } else {
                        break;
                    }
                }
                if (digits.isEmpty() || (i < n && expr.at(i).isLetterOrNumber())) {
                    errorOut = QCoreApplication::translate("Engine", "Invalid integer literal");
                    return {};
                }
                bool ok = false;
                const qulonglong parsed = digits.toULongLong(&ok, radix);
                if (!ok) {
                    errorOut = QCoreApplication::translate("Engine", "Integer literal overflow");
                    return {};
                }
                tokens.push_back({TokenKind::Number, expr.mid(start, i - start), static_cast<double>(parsed)});
                continue;
            }
        }

        // Decimal number literal (supports '_' grouping and scientific notation)
        if (ch.isDigit() || (ch == QLatin1Char('.') && i + 1 < n && expr.at(i + 1).isDigit())) {
            QString cleaned;
            bool seenDot = false;
            bool seenExp = false;
            while (i < n) {
                const QChar c = expr.at(i);
                if (c.isDigit()) {
                    cleaned.append(c);
                    ++i;
                } else if (c == QLatin1Char('_')) {
                    ++i;
                } else if (c == QLatin1Char('.') && !seenDot && !seenExp) {
                    seenDot = true;
                    cleaned.append(c);
                    ++i;
                } else if ((c == QLatin1Char('e') || c == QLatin1Char('E')) && !seenExp) {
                    int look = i + 1;
                    if (look < n && (expr.at(look) == QLatin1Char('+') || expr.at(look) == QLatin1Char('-')))
                        ++look;
                    if (look < n && expr.at(look).isDigit()) {
                        seenExp = true;
                        cleaned.append(QLatin1Char('e'));
                        ++i;
                        if (i < n && (expr.at(i) == QLatin1Char('+') || expr.at(i) == QLatin1Char('-'))) {
                            cleaned.append(expr.at(i));
                            ++i;
                        }
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            bool ok = false;
            const double val = cleaned.toDouble(&ok);
            if (!ok) {
                errorOut = QCoreApplication::translate("Engine", "Invalid number");
                return {};
            }
            tokens.push_back({TokenKind::Number, cleaned, val});
            continue;
        }

        if (currencyFromSymbolChar(ch).has_value()) {
            tokens.push_back({TokenKind::CurrencySymbol, QString(ch), 0.0});
            ++i;
            continue;
        }

        if (isIdentStart(ch)) {
            const int start = i;
            ++i;
            while (i < n && isIdentPart(expr.at(i)))
                ++i;
            tokens.push_back({TokenKind::Ident, expr.mid(start, i - start), 0.0});
            continue;
        }

        if (ch == QLatin1Char('-') && i + 1 < n && expr.at(i + 1) == QLatin1Char('>')) {
            tokens.push_back({TokenKind::Arrow, "->", 0.0});
            i += 2;
            continue;
        }

        if (ch == QLatin1Char('*') && i + 1 < n && expr.at(i + 1) == QLatin1Char('*')) {
            tokens.push_back({TokenKind::Caret, "**", 0.0});
            i += 2;
            continue;
        }

        TokenKind kind = TokenKind::Invalid;
        switch (ch.unicode()) {
        case '+': kind = TokenKind::Plus; break;
        case '-': kind = TokenKind::Minus; break;
        case '*': kind = TokenKind::Star; break;
        case '/': kind = TokenKind::Slash; break;
        case '^': kind = TokenKind::Caret; break;
        case '%': kind = TokenKind::Percent; break;
        case '(': kind = TokenKind::LParen; break;
        case ')': kind = TokenKind::RParen; break;
        case ',': kind = TokenKind::Comma; break;
        default: break;
        }

        if (kind == TokenKind::Invalid) {
            errorOut = QCoreApplication::translate("Engine", "Unexpected character '%1'").arg(ch);
            return {};
        }
        tokens.push_back({kind, QString(ch), 0.0});
        ++i;
    }

    tokens.push_back({TokenKind::End, {}, 0.0});
    return tokens;
}

bool startsPrimary(const Token &tok) {
    if (tok.kind == TokenKind::Number || tok.kind == TokenKind::LParen ||
        tok.kind == TokenKind::CurrencySymbol) {
        return true;
    }
    if (tok.kind == TokenKind::Ident) {
        const QString lower = tok.text.toLower();
        return lower != "of" && lower != "off" && lower != "in" && lower != "to" && lower != "as";
    }
    return false;
}

class Parser {
public:
    Parser(const QVector<Token> &tokens,
           const QHash<QString, CalcValue> &env,
           const QVector<CalcValue> &blockValues,
           const std::optional<CalcValue> &prevValue)
        : m_tokens(tokens), m_env(env), m_blockValues(blockValues), m_prevValue(prevValue) {}

    bool parse(CalcValue &out, QString &errorOut) {
        m_error.clear();
        m_pos = 0;
        out = parseConversion();
        if (!m_error.isEmpty()) {
            errorOut = m_error;
            return false;
        }
        if (peek().kind != TokenKind::End) {
            errorOut = QCoreApplication::translate("Engine", "Unexpected token '%1'").arg(peek().text);
            return false;
        }
        if (!std::isfinite(out.number)) {
            errorOut = std::isnan(out.number)
                ? QCoreApplication::translate("Engine", "Math domain error")
                : QCoreApplication::translate("Engine", "Overflow");
            return false;
        }
        return true;
    }

private:
    const Token &peek(int offset = 0) const {
        const int idx = m_pos + offset;
        if (idx >= 0 && idx < m_tokens.size())
            return m_tokens.at(idx);
        return m_tokens.last();
    }

    const Token &advance() {
        const Token &cur = peek();
        if (m_pos + 1 < m_tokens.size())
            ++m_pos;
        return cur;
    }

    bool fail(const QString &msg) {
        if (m_error.isEmpty())
            m_error = msg;
        return false;
    }

    static bool isConversionKeyword(const Token &tok) {
        if (tok.kind == TokenKind::Arrow)
            return true;
        if (tok.kind == TokenKind::Ident) {
            const QString lower = tok.text.toLower();
            return lower == "in" || lower == "to" || lower == "as";
        }
        return false;
    }

    CalcValue parseConversion() {
        CalcValue lhs = parseAddSub();
        while (m_error.isEmpty() && isConversionKeyword(peek())) {
            advance();
            if (peek().kind == TokenKind::Percent) {
                advance();
                if (lhs.unit != UnitId::None || lhs.currency != CurrencyId::None) {
                    fail(QCoreApplication::translate("Engine", "Cannot convert to %"));
                    return {};
                }
                lhs.base = NumberBase::Dec;
                lhs.isPercent = true;
                continue;
            }
            if (peek().kind != TokenKind::Ident) {
                fail(QCoreApplication::translate("Engine", "Expected unit or base after conversion"));
                return {};
            }
            const QString targetName = advance().text;

            if (const auto targetBase = lookupBase(targetName)) {
                if (lhs.unit != UnitId::None || lhs.currency != CurrencyId::None || lhs.isPercent) {
                    fail(QCoreApplication::translate("Engine", "Base conversion requires a plain integer"));
                    return {};
                }
                if (!std::isfinite(lhs.number) || std::trunc(lhs.number) != lhs.number ||
                    std::abs(lhs.number) > 9007199254740991.0) {
                    fail(QCoreApplication::translate("Engine", "Base conversion requires an integer"));
                    return {};
                }
                lhs.base = *targetBase;
                continue;
            }

            const auto targetUnit = lookupUnit(targetName);
            if (!targetUnit.has_value()) {
                fail(QCoreApplication::translate("Engine", "Unknown unit '%1'").arg(targetName));
                return {};
            }
            if (lhs.currency != CurrencyId::None) {
                fail(QCoreApplication::translate("Engine", "Cannot convert %1 to %2")
                         .arg(currencySymbol(lhs.currency), unitSymbol(*targetUnit)));
                return {};
            }
            double converted = 0.0;
            if (!convertUnit(lhs.number, lhs.unit, *targetUnit, converted)) {
                if (lhs.unit == UnitId::None) {
                    fail(QCoreApplication::translate("Engine", "Value has no unit to convert to %1")
                             .arg(unitSymbol(*targetUnit)));
                } else {
                    fail(QCoreApplication::translate("Engine", "Incompatible units: %1 and %2")
                             .arg(unitSymbol(lhs.unit), unitSymbol(*targetUnit)));
                }
                return {};
            }
            lhs = {converted, *targetUnit, CurrencyId::None, NumberBase::Dec, false};
        }
        return lhs;
    }

    CalcValue parseAddSub() {
        CalcValue lhs = parseMulDiv();
        while (m_error.isEmpty() &&
               (peek().kind == TokenKind::Plus || peek().kind == TokenKind::Minus)) {
            const bool isAdd = advance().kind == TokenKind::Plus;
            const CalcValue rhs = parseMulDiv();
            if (!m_error.isEmpty())
                return {};

            lhs.base = NumberBase::Dec;

            if (!lhs.isPercent && rhs.isPercent) {
                const double factor = isAdd ? (1.0 + rhs.number) : (1.0 - rhs.number);
                lhs.number *= factor;
                continue;
            }
            if (lhs.isPercent && rhs.isPercent) {
                lhs.number = isAdd ? (lhs.number + rhs.number) : (lhs.number - rhs.number);
                continue;
            }
            if (lhs.isPercent && !rhs.isPercent) {
                fail(QCoreApplication::translate("Engine", "Cannot add %1 to percentage")
                         .arg(rhs.unit == UnitId::None ? QStringLiteral("number") : unitSymbol(rhs.unit)));
                return {};
            }

            // Currency addition/subtraction
            if (lhs.currency != CurrencyId::None || rhs.currency != CurrencyId::None) {
                if (lhs.currency != rhs.currency) {
                    const QString c1 = lhs.currency == CurrencyId::None ? QStringLiteral("number") : currencySymbol(lhs.currency);
                    const QString c2 = rhs.currency == CurrencyId::None ? QStringLiteral("number") : currencySymbol(rhs.currency);
                    fail(QCoreApplication::translate("Engine", "Incompatible currencies: %1 and %2").arg(c1, c2));
                    return {};
                }
                lhs.number = isAdd ? (lhs.number + rhs.number) : (lhs.number - rhs.number);
                continue;
            }

            if (lhs.unit == rhs.unit) {
                lhs.number = isAdd ? (lhs.number + rhs.number) : (lhs.number - rhs.number);
            } else if (lhs.unit != UnitId::None && rhs.unit != UnitId::None &&
                       unitDimension(lhs.unit) == unitDimension(rhs.unit)) {
                double rhsConverted = 0.0;
                convertUnit(rhs.number, rhs.unit, lhs.unit, rhsConverted);
                lhs.number = isAdd ? (lhs.number + rhsConverted) : (lhs.number - rhsConverted);
            } else {
                const QString u1 = lhs.unit == UnitId::None ? QStringLiteral("scalar") : unitSymbol(lhs.unit);
                const QString u2 = rhs.unit == UnitId::None ? QStringLiteral("scalar") : unitSymbol(rhs.unit);
                fail(QCoreApplication::translate("Engine", "Incompatible units: %1 and %2").arg(u1, u2));
                return {};
            }
        }
        return lhs;
    }

    CalcValue multiplyValues(const CalcValue &lhs, const CalcValue &rhs) {
        if (lhs.unit != UnitId::None && rhs.unit != UnitId::None) {
            fail(QCoreApplication::translate("Engine", "Cannot multiply %1 by %2")
                     .arg(unitSymbol(lhs.unit), unitSymbol(rhs.unit)));
            return {};
        }
        if (lhs.currency != CurrencyId::None && rhs.currency != CurrencyId::None) {
            fail(QCoreApplication::translate("Engine", "Cannot multiply %1 by %2")
                     .arg(currencySymbol(lhs.currency), currencySymbol(rhs.currency)));
            return {};
        }
        if ((lhs.currency != CurrencyId::None && rhs.unit != UnitId::None) ||
            (lhs.unit != UnitId::None && rhs.currency != CurrencyId::None)) {
            fail(QCoreApplication::translate("Engine", "Cannot combine currency and unit"));
            return {};
        }
        const UnitId outUnit = lhs.unit != UnitId::None ? lhs.unit : rhs.unit;
        const CurrencyId outCur = lhs.currency != CurrencyId::None ? lhs.currency : rhs.currency;
        const bool outPct = (lhs.isPercent != rhs.isPercent) && outUnit == UnitId::None && outCur == CurrencyId::None;
        return {lhs.number * rhs.number, outUnit, outCur, NumberBase::Dec, outPct};
    }

    CalcValue parseMulDiv() {
        CalcValue lhs = parseOf();
        while (m_error.isEmpty()) {
            const TokenKind kind = peek().kind;
            if (kind != TokenKind::Star && kind != TokenKind::Slash && kind != TokenKind::Percent)
                break;
            advance();
            const CalcValue rhs = parseOf();
            if (!m_error.isEmpty())
                return {};

            if (kind == TokenKind::Star) {
                lhs = multiplyValues(lhs, rhs);
            } else if (kind == TokenKind::Slash) {
                if (rhs.number == 0.0) {
                    fail(QCoreApplication::translate("Engine", "Division by zero"));
                    return {};
                }
                if (lhs.currency != CurrencyId::None || rhs.currency != CurrencyId::None) {
                    if (lhs.currency != CurrencyId::None && rhs.currency == lhs.currency) {
                        lhs = {lhs.number / rhs.number, UnitId::None, CurrencyId::None, NumberBase::Dec, false};
                    } else if (lhs.currency != CurrencyId::None && rhs.currency == CurrencyId::None && rhs.unit == UnitId::None) {
                        lhs = {lhs.number / rhs.number, UnitId::None, lhs.currency, NumberBase::Dec, false};
                    } else {
                        fail(QCoreApplication::translate("Engine", "Incompatible currencies in division"));
                        return {};
                    }
                    continue;
                }
                if (lhs.unit != UnitId::None && rhs.unit != UnitId::None) {
                    if (unitDimension(lhs.unit) != unitDimension(rhs.unit)) {
                        fail(QCoreApplication::translate("Engine", "Incompatible units: %1 and %2")
                                 .arg(unitSymbol(lhs.unit), unitSymbol(rhs.unit)));
                        return {};
                    }
                    double rhsInLhs = 0.0;
                    convertUnit(rhs.number, rhs.unit, lhs.unit, rhsInLhs);
                    if (rhsInLhs == 0.0) {
                        fail(QCoreApplication::translate("Engine", "Division by zero"));
                        return {};
                    }
                    lhs = {lhs.number / rhsInLhs, UnitId::None, CurrencyId::None, NumberBase::Dec, false};
                } else if (rhs.unit != UnitId::None) {
                    fail(QCoreApplication::translate("Engine", "Cannot divide by %1")
                             .arg(unitSymbol(rhs.unit)));
                    return {};
                } else {
                    lhs = {lhs.number / rhs.number, lhs.unit, CurrencyId::None, NumberBase::Dec,
                           lhs.isPercent && !rhs.isPercent};
                }
            } else {
                if (rhs.number == 0.0) {
                    fail(QCoreApplication::translate("Engine", "Division by zero"));
                    return {};
                }
                if (lhs.unit != rhs.unit || lhs.currency != rhs.currency) {
                    fail(QCoreApplication::translate("Engine", "Incompatible units for modulo"));
                    return {};
                }
                lhs = {std::fmod(lhs.number, rhs.number), lhs.unit, lhs.currency, NumberBase::Dec, false};
            }
        }
        return lhs;
    }

    CalcValue parseOf() {
        CalcValue lhs = parseImplicitMul();
        if (m_error.isEmpty() && peek().kind == TokenKind::Ident) {
            const QString lower = peek().text.toLower();
            if (lower == QStringLiteral("of")) {
                advance();
                const CalcValue rhs = parseOf();
                if (!m_error.isEmpty())
                    return {};
                if (lhs.unit != UnitId::None || lhs.currency != CurrencyId::None) {
                    fail(QCoreApplication::translate("Engine", "Left side of 'of' cannot have a unit"));
                    return {};
                }
                return {lhs.number * rhs.number, rhs.unit, rhs.currency, NumberBase::Dec,
                        rhs.isPercent && lhs.isPercent};
            }
            if (lower == QStringLiteral("off")) {
                advance();
                const CalcValue rhs = parseOf();
                if (!m_error.isEmpty())
                    return {};
                if (lhs.isPercent && lhs.unit == UnitId::None && lhs.currency == CurrencyId::None) {
                    return {(1.0 - lhs.number) * rhs.number, rhs.unit, rhs.currency, NumberBase::Dec, rhs.isPercent};
                }
                if (lhs.unit == rhs.unit && lhs.currency == rhs.currency && !lhs.isPercent && !rhs.isPercent) {
                    return {rhs.number - lhs.number, rhs.unit, rhs.currency, NumberBase::Dec, false};
                }
                fail(QCoreApplication::translate("Engine", "Incompatible operands for 'off'"));
                return {};
            }
        }
        return lhs;
    }

    bool canImplicitMultiply(const CalcValue &lhs) const {
        const Token &next = peek();
        if (next.kind == TokenKind::LParen)
            return true;
        if (next.kind == TokenKind::Ident) {
            const QString lower = next.text.toLower();
            if (lower == "in" || lower == "to" || lower == "as" || lower == "of" || lower == "off")
                return false;
            if (peek(1).kind == TokenKind::LParen && builtinFunctions().contains(lower))
                return true;
            if (m_env.contains(next.text))
                return true;
            if (builtinConstants().contains(lower))
                return true;
            // Allow 2 $price? Handled via CurrencySymbol in unary/primary if needed, not Ident.
        }
        Q_UNUSED(lhs);
        return false;
    }

    CalcValue parseImplicitMul() {
        CalcValue lhs = parseUnary();
        while (m_error.isEmpty() && canImplicitMultiply(lhs)) {
            const CalcValue rhs = parseUnary();
            if (!m_error.isEmpty())
                return {};
            lhs = multiplyValues(lhs, rhs);
        }
        return lhs;
    }

    CalcValue parseUnary() {
        if (peek().kind == TokenKind::Plus) {
            advance();
            return parseUnary();
        }
        if (peek().kind == TokenKind::Minus) {
            advance();
            CalcValue v = parseUnary();
            v.number = -v.number;
            return v;
        }
        return parsePower();
    }

    CalcValue parsePower() {
        CalcValue base = parsePostfix();
        if (m_error.isEmpty() && peek().kind == TokenKind::Caret) {
            advance();
            const CalcValue exp = parseUnary();
            if (!m_error.isEmpty())
                return {};
            if (exp.unit != UnitId::None || exp.currency != CurrencyId::None) {
                fail(QCoreApplication::translate("Engine", "Exponent cannot have a unit"));
                return {};
            }
            if (base.unit != UnitId::None || base.currency != CurrencyId::None) {
                fail(QCoreApplication::translate("Engine", "Cannot raise a unit or currency to a power"));
                return {};
            }
            return {std::pow(base.number, exp.number), UnitId::None, CurrencyId::None, NumberBase::Dec, false};
        }
        return base;
    }

    CalcValue parsePostfix() {
        CalcValue val = parsePrimary();
        while (m_error.isEmpty()) {
            if (peek().kind == TokenKind::Percent && !startsPrimary(peek(1))) {
                advance();
                if (val.unit != UnitId::None || val.currency != CurrencyId::None) {
                    fail(QCoreApplication::translate("Engine", "Cannot apply % to a unit or currency"));
                    return {};
                }
                val = {val.number / 100.0, UnitId::None, CurrencyId::None, NumberBase::Dec, true};
                continue;
            }
            if (peek().kind == TokenKind::CurrencySymbol &&
                val.currency == CurrencyId::None && val.unit == UnitId::None && !val.isPercent) {
                const auto cur = lookupCurrency(advance().text);
                if (cur.has_value()) {
                    val.currency = *cur;
                    val.base = NumberBase::Dec;
                    continue;
                }
            }
            if (peek().kind == TokenKind::Ident &&
                val.unit == UnitId::None && val.currency == CurrencyId::None && !val.isPercent) {
                const QString word = peek().text;
                const QString lower = word.toLower();
                if (lower == "in") {
                    if (peek(1).kind == TokenKind::Percent || lookupBase(peek(1).text).has_value())
                        break;
                    advance();
                    val.unit = UnitId::In;
                    val.base = NumberBase::Dec;
                    continue;
                }
                if (lower != "to" && lower != "as" && lower != "of" && lower != "off") {
                    if (const auto unit = lookupUnit(word)) {
                        advance();
                        val.unit = *unit;
                        val.base = NumberBase::Dec;
                        continue;
                    }
                    // Postfix currency codes/kr when not shadowed by a user variable.
                    if (!m_env.contains(word)) {
                        if (const auto cur = lookupCurrency(word)) {
                            advance();
                            val.currency = *cur;
                            val.base = NumberBase::Dec;
                            continue;
                        }
                    }
                }
            }
            break;
        }
        return val;
    }

    CalcValue parsePrimary() {
        const Token tok = peek();
        if (tok.kind == TokenKind::Number) {
            advance();
            return {tok.number, UnitId::None, CurrencyId::None, NumberBase::Dec, false};
        }
        if (tok.kind == TokenKind::CurrencySymbol) {
            const auto cur = lookupCurrency(advance().text);
            CalcValue inner = parsePrimary();
            if (!m_error.isEmpty())
                return {};
            if (inner.currency != CurrencyId::None || inner.unit != UnitId::None || inner.isPercent) {
                fail(QCoreApplication::translate("Engine", "Unexpected currency prefix"));
                return {};
            }
            inner.currency = cur.value_or(CurrencyId::None);
            inner.base = NumberBase::Dec;
            return inner;
        }
        if (tok.kind == TokenKind::LParen) {
            advance();
            const CalcValue inner = parseConversion();
            if (!m_error.isEmpty())
                return {};
            if (peek().kind != TokenKind::RParen) {
                fail(QCoreApplication::translate("Engine", "Missing closing ')'"));
                return {};
            }
            advance();
            return inner;
        }
        if (tok.kind == TokenKind::Ident) {
            advance();
            const QString name = tok.text;
            const QString lower = name.toLower();

            if (peek().kind == TokenKind::LParen &&
                (builtinFunctions().contains(lower) || !m_env.contains(name))) {
                return parseFunctionCall(lower);
            }

            const auto varIt = m_env.constFind(name);
            if (varIt != m_env.constEnd())
                return varIt.value();

            if (lower == "pi")
                return {3.14159265358979323846, UnitId::None, CurrencyId::None, NumberBase::Dec, false};
            if (lower == "e")
                return {2.71828182845904523536, UnitId::None, CurrencyId::None, NumberBase::Dec, false};
            if (lower == "tau")
                return {6.28318530717958647692, UnitId::None, CurrencyId::None, NumberBase::Dec, false};

            if (lower == "prev" || lower == "ans") {
                if (!m_prevValue.has_value()) {
                    fail(QCoreApplication::translate("Engine", "No previous value"));
                    return {};
                }
                return *m_prevValue;
            }

            if (lower == "count")
                return {static_cast<double>(m_blockValues.size()), UnitId::None, CurrencyId::None, NumberBase::Dec, false};

            if (lower == "sum" || lower == "total")
                return aggregateSum(m_blockValues);

            if (lower == "avg" || lower == "average" || lower == "mean")
                return aggregateAvg(m_blockValues);

            fail(QCoreApplication::translate("Engine", "Unknown variable '%1'").arg(name));
            return {};
        }

        fail(QCoreApplication::translate("Engine", "Expected expression"));
        return {};
    }

    CalcValue aggregateSum(const QVector<CalcValue> &items) {
        if (items.isEmpty())
            return {};
        const UnitId targetUnit = items.first().unit;
        const CurrencyId targetCurrency = items.first().currency;
        const bool allPct = items.first().isPercent;
        double total = 0.0;
        for (const CalcValue &item : items) {
            if (item.currency != targetCurrency) {
                fail(QCoreApplication::translate("Engine", "Incompatible currencies in sum"));
                return {};
            }
            if (targetUnit == UnitId::None && item.unit == UnitId::None) {
                total += item.number;
            } else if (targetUnit != UnitId::None && item.unit != UnitId::None &&
                       unitDimension(targetUnit) == unitDimension(item.unit)) {
                double converted = 0.0;
                convertUnit(item.number, item.unit, targetUnit, converted);
                total += converted;
            } else {
                fail(QCoreApplication::translate("Engine", "Incompatible units in sum"));
                return {};
            }
        }
        return {total, targetUnit, targetCurrency, NumberBase::Dec,
                allPct && targetUnit == UnitId::None && targetCurrency == CurrencyId::None};
    }

    CalcValue aggregateAvg(const QVector<CalcValue> &items) {
        if (items.isEmpty()) {
            fail(QCoreApplication::translate("Engine", "Nothing to average"));
            return {};
        }
        CalcValue s = aggregateSum(items);
        if (!m_error.isEmpty())
            return {};
        s.number /= static_cast<double>(items.size());
        return s;
    }

    CalcValue parseFunctionCall(const QString &name) {
        advance(); // consume '('
        QVector<CalcValue> args;
        if (peek().kind != TokenKind::RParen) {
            while (true) {
                args.push_back(parseConversion());
                if (!m_error.isEmpty())
                    return {};
                if (peek().kind == TokenKind::Comma) {
                    advance();
                    continue;
                }
                break;
            }
        }
        if (peek().kind != TokenKind::RParen) {
            fail(QCoreApplication::translate("Engine", "Expected ')' after arguments"));
            return {};
        }
        advance();

        auto requireCount = [&](int exact) -> bool {
            if (args.size() != exact) {
                fail(QCoreApplication::translate("Engine", "%1() expects %2 argument(s)")
                         .arg(name).arg(exact));
                return false;
            }
            return true;
        };
        auto requireMinCount = [&](int minimum) -> bool {
            if (args.size() < minimum) {
                fail(QCoreApplication::translate("Engine", "%1() expects at least %2 argument(s)")
                         .arg(name).arg(minimum));
                return false;
            }
            return true;
        };
        auto requireRange = [&](int minArgs, int maxArgs) -> bool {
            if (args.size() < minArgs || args.size() > maxArgs) {
                fail(QCoreApplication::translate("Engine", "%1() expects %2 to %3 arguments")
                         .arg(name).arg(minArgs).arg(maxArgs));
                return false;
            }
            return true;
        };

        if (name == "sum") {
            if (!requireMinCount(1)) return {};
            return aggregateSum(args);
        }
        if (name == "avg") {
            if (!requireMinCount(1)) return {};
            return aggregateAvg(args);
        }
        if (name == "min" || name == "max") {
            if (!requireMinCount(1)) return {};
            const bool isMin = (name == "min");
            const UnitId targetUnit = args.first().unit;
            const CurrencyId targetCurrency = args.first().currency;
            double best = args.first().number;
            for (int i = 1; i < args.size(); ++i) {
                if (args.at(i).currency != targetCurrency) {
                    fail(QCoreApplication::translate("Engine", "Incompatible currencies in %1()").arg(name));
                    return {};
                }
                double val = args.at(i).number;
                if (targetUnit != args.at(i).unit) {
                    if (targetUnit == UnitId::None || args.at(i).unit == UnitId::None ||
                        unitDimension(targetUnit) != unitDimension(args.at(i).unit)) {
                        fail(QCoreApplication::translate("Engine", "Incompatible units in %1()").arg(name));
                        return {};
                    }
                    convertUnit(val, args.at(i).unit, targetUnit, val);
                }
                best = isMin ? std::min(best, val) : std::max(best, val);
            }
            return {best, targetUnit, targetCurrency, NumberBase::Dec, args.first().isPercent};
        }
        if (name == "clamp") {
            if (!requireCount(3)) return {};
            return {std::clamp(args[0].number, args[1].number, args[2].number),
                    args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }
        if (name == "abs") {
            if (!requireCount(1)) return {};
            return {std::abs(args[0].number), args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }
        if (name == "sign") {
            if (!requireCount(1)) return {};
            const double v = args[0].number;
            return {v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0), UnitId::None, CurrencyId::None, NumberBase::Dec, false};
        }
        if (name == "round") {
            if (!requireRange(1, 2)) return {};
            const int decimals = args.size() == 2 ? static_cast<int>(std::llround(args[1].number)) : 0;
            const double factor = std::pow(10.0, std::clamp(decimals, 0, 10));
            return {std::round(args[0].number * factor) / factor,
                    args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }
        if (name == "floor") {
            if (!requireCount(1)) return {};
            return {std::floor(args[0].number), args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }
        if (name == "ceil") {
            if (!requireCount(1)) return {};
            return {std::ceil(args[0].number), args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }
        if (name == "trunc") {
            if (!requireCount(1)) return {};
            return {std::trunc(args[0].number), args[0].unit, args[0].currency, NumberBase::Dec, args[0].isPercent};
        }

        for (const CalcValue &a : args) {
            if (a.unit != UnitId::None || a.currency != CurrencyId::None) {
                fail(QCoreApplication::translate("Engine", "%1() requires plain numbers").arg(name));
                return {};
            }
        }

        if (name == "sqrt")  { if (!requireCount(1)) return {}; return {std::sqrt(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "cbrt")  { if (!requireCount(1)) return {}; return {std::cbrt(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "sin")   { if (!requireCount(1)) return {}; return {std::sin(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "cos")   { if (!requireCount(1)) return {}; return {std::cos(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "tan")   { if (!requireCount(1)) return {}; return {std::tan(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "asin")  { if (!requireCount(1)) return {}; return {std::asin(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "acos")  { if (!requireCount(1)) return {}; return {std::acos(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "atan")  { if (!requireCount(1)) return {}; return {std::atan(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "atan2") { if (!requireCount(2)) return {}; return {std::atan2(args[0].number, args[1].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "deg")   { if (!requireCount(1)) return {}; return {args[0].number * 180.0 / 3.14159265358979323846, UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "rad")   { if (!requireCount(1)) return {}; return {args[0].number * 3.14159265358979323846 / 180.0, UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "ln")    { if (!requireCount(1)) return {}; return {std::log(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "log2")  { if (!requireCount(1)) return {}; return {std::log2(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "exp")   { if (!requireCount(1)) return {}; return {std::exp(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "pow")   { if (!requireCount(2)) return {}; return {std::pow(args[0].number, args[1].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false}; }
        if (name == "log") {
            if (!requireRange(1, 2)) return {};
            if (args.size() == 1)
                return {std::log10(args[0].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false};
            return {std::log(args[0].number) / std::log(args[1].number), UnitId::None, CurrencyId::None, NumberBase::Dec, false};
        }

        fail(QCoreApplication::translate("Engine", "Unknown function '%1'").arg(name));
        return {};
    }

    const QVector<Token> &m_tokens;
    const QHash<QString, CalcValue> &m_env;
    const QVector<CalcValue> &m_blockValues;
    const std::optional<CalcValue> &m_prevValue;
    int m_pos = 0;
    QString m_error;
};

QString stripInlineComment(const QString &line) {
    const int slashIdx = line.indexOf(QStringLiteral("//"));
    int cut = slashIdx;
    for (int i = 1; i < line.size(); ++i) {
        if (line.at(i) == QLatin1Char('#') && line.at(i - 1).isSpace()) {
            cut = (cut < 0) ? i : std::min(cut, i);
            break;
        }
    }
    return cut >= 0 ? line.left(cut) : line;
}

bool looksLikeProseNote(const QString &trimmed, const QHash<QString, CalcValue> &env) {
    for (const QChar ch : trimmed) {
        if (ch.isDigit())
            return false;
        if (QStringLiteral("+-*/^%()=,.$€£¥").contains(ch))
            return false;
    }
    static const QRegularExpression wordRe(QStringLiteral("[A-Za-z_][A-Za-z0-9_]*"));
    auto it = wordRe.globalMatch(trimmed);
    while (it.hasNext()) {
        const QString word = it.next().captured(0);
        const QString lower = word.toLower();
        if (env.contains(word) ||
            builtinConstants().contains(lower) ||
            builtinAggregates().contains(lower) ||
            builtinFunctions().contains(lower)) {
            return false;
        }
    }
    return true;
}

} // namespace

QString Engine::formatNumber(double value, const QLocale &locale, int precision) {
    if (!std::isfinite(value))
        return QString::number(value);

    const int prec = std::clamp(precision, 0, 10);
    if (std::abs(value) < 1e-12)
        value = 0.0;

    QString num;
    const double absVal = std::abs(value);
    if (absVal != 0.0 && (absVal >= 1e12 || absVal < 1e-6)) {
        num = locale.toString(value, 'g', std::max(prec, 1));
    } else {
        num = locale.toString(value, 'f', prec);
        const QString dec = locale.decimalPoint();
        const int decIdx = num.indexOf(dec);
        if (decIdx >= 0) {
            int end = num.size();
            while (end > decIdx + 1 && num.at(end - 1) == QLatin1Char('0'))
                --end;
            if (end == decIdx + dec.size())
                end = decIdx;
            num.truncate(end);
        }
    }
    if (num == QStringLiteral("-0"))
        num = QStringLiteral("0");
    return num;
}

QString Engine::formatValue(const CalcValue &value, const QLocale &locale, int precision) {
    if (value.base != NumberBase::Dec) {
        const qint64 intVal = static_cast<qint64>(std::llround(value.number));
        const bool neg = intVal < 0;
        const quint64 absInt = neg ? static_cast<quint64>(-intVal) : static_cast<quint64>(intVal);
        QString prefix;
        int radix = 10;
        switch (value.base) {
        case NumberBase::Hex: prefix = QStringLiteral("0x"); radix = 16; break;
        case NumberBase::Bin: prefix = QStringLiteral("0b"); radix = 2; break;
        case NumberBase::Oct: prefix = QStringLiteral("0o"); radix = 8; break;
        case NumberBase::Dec: break;
        }
        return (neg ? QStringLiteral("-") : QString()) + prefix + QString::number(absInt, radix);
    }

    if (value.isPercent)
        return formatNumber(value.number * 100.0, locale, precision) + QStringLiteral("%");

    if (value.currency != CurrencyId::None) {
        const bool neg = value.number < 0.0 && std::abs(value.number) >= 1e-12;
        const QString num = formatNumber(neg ? -value.number : value.number, locale, precision);
        if (value.currency == CurrencyId::Kr)
            return (neg ? QStringLiteral("-") : QString()) + num + QStringLiteral(" kr");
        return (neg ? QStringLiteral("-") : QString()) + currencySymbol(value.currency) + num;
    }

    QString out = formatNumber(value.number, locale, precision);
    if (value.unit != UnitId::None)
        out += QLatin1Char(' ') + unitSymbol(value.unit);
    return out;
}

SheetResult Engine::evaluate(const QString &text, const QLocale &displayLocale, int precision) {
    SheetResult sheet;
    QHash<QString, CalcValue> env;
    QHash<QString, int> varOrderIndex;
    QVector<CalcValue> blockValues;
    std::optional<CalcValue> prevValue;

    QLocale cleanLocale = displayLocale;
    cleanLocale.setNumberOptions(cleanLocale.numberOptions() | QLocale::OmitGroupSeparator);
    QLocale copyLocale = QLocale::c();
    copyLocale.setNumberOptions(QLocale::OmitGroupSeparator);

    static const QRegularExpression assignRe(
        QStringLiteral(R"(^(?:let\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=)\s*(.+)$)"));

    int pos = 0;
    int lineNum = 0;
    const int totalLen = text.size();

    while (pos <= totalLen) {
        int nextNl = text.indexOf(QLatin1Char('\n'), pos);
        if (nextNl < 0)
            nextNl = totalLen;

        QString rawLine = text.mid(pos, nextNl - pos);
        if (rawLine.endsWith(QLatin1Char('\r')))
            rawLine.chop(1);

        LineResult line;
        line.lineNumber = lineNum;
        line.charOffset = pos;

        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) {
            line.kind = LineKind::Empty;
            blockValues.clear();
        } else if (trimmed.startsWith(QLatin1Char('#'))) {
            line.kind = LineKind::Heading;
            blockValues.clear();
        } else if (trimmed.startsWith(QStringLiteral("//"))) {
            line.kind = LineKind::Comment;
        } else {
            const QString body = stripInlineComment(rawLine).trimmed();
            if (body.isEmpty()) {
                line.kind = LineKind::Comment;
            } else {
                const auto assignMatch = assignRe.match(body);
                if (assignMatch.hasMatch()) {
                    const QString varName = assignMatch.captured(1);
                    const QString rhs = assignMatch.captured(2).trimmed();
                    QString err;
                    const auto tokens = tokenize(rhs, err);
                    CalcValue val;
                    if (err.isEmpty()) {
                        Parser parser(tokens, env, blockValues, prevValue);
                        parser.parse(val, err);
                    }
                    if (!err.isEmpty()) {
                        line.kind = LineKind::Error;
                        line.variableName = varName;
                        line.errorText = err;
                        sheet.hasErrors = true;
                    } else {
                        line.kind = LineKind::Assignment;
                        line.variableName = varName;
                        line.value = val;
                        line.displayText = formatValue(val, cleanLocale, precision);
                        line.copyText = formatValue(val, copyLocale, precision);

                        env.insert(varName, val);
                        prevValue = val;
                        blockValues.push_back(val);

                        const VariableEntry entry{varName, line.displayText, line.copyText, lineNum};
                        if (varOrderIndex.contains(varName)) {
                            sheet.variables[varOrderIndex.value(varName)] = entry;
                        } else {
                            varOrderIndex.insert(varName, sheet.variables.size());
                            sheet.variables.push_back(entry);
                        }
                    }
                } else if (looksLikeProseNote(body, env)) {
                    line.kind = LineKind::Note;
                } else {
                    QString err;
                    const auto tokens = tokenize(body, err);
                    CalcValue val;
                    if (err.isEmpty()) {
                        Parser parser(tokens, env, blockValues, prevValue);
                        parser.parse(val, err);
                    }
                    if (!err.isEmpty()) {
                        line.kind = LineKind::Error;
                        line.errorText = err;
                        sheet.hasErrors = true;
                    } else {
                        line.kind = LineKind::Expression;
                        line.value = val;
                        line.displayText = formatValue(val, cleanLocale, precision);
                        line.copyText = formatValue(val, copyLocale, precision);
                        prevValue = val;
                        blockValues.push_back(val);
                    }
                }
            }
        }

        sheet.lines.push_back(line);
        ++lineNum;
        if (nextNl == totalLen)
            break;
        pos = nextNl + 1;
    }

    return sheet;
}
