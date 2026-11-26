#include "highlighter.h"

#include <QRegularExpression>
#include <QSet>

#include "units.h"

Highlighter::Highlighter(QTextDocument *document) : QSyntaxHighlighter(document) {
    rebuildFormats();
}

void Highlighter::setAccent(const QString &hex) {
    const QColor next(hex);
    if (!next.isValid() || next == m_accent)
        return;
    m_accent = next;
    rebuildFormats();
    rehighlight();
}

void Highlighter::rebuildFormats() {
    m_headingFmt = QTextCharFormat();
    m_headingFmt.setForeground(QColor(QStringLiteral("#F4F4F6")));
    m_headingFmt.setFontWeight(QFont::DemiBold);

    m_commentFmt = QTextCharFormat();
    m_commentFmt.setForeground(QColor(QStringLiteral("#6E707A")));
    m_commentFmt.setFontItalic(true);

    m_varFmt = QTextCharFormat();
    m_varFmt.setForeground(m_accent);
    m_varFmt.setFontWeight(QFont::Medium);

    m_numberFmt = QTextCharFormat();
    m_numberFmt.setForeground(QColor(QStringLiteral("#F4F4F6")));

    m_operatorFmt = QTextCharFormat();
    m_operatorFmt.setForeground(QColor(QStringLiteral("#8E92A0")));

    m_keywordFmt = QTextCharFormat();
    m_keywordFmt.setForeground(QColor(QStringLiteral("#9CA3AF")));

    m_functionFmt = QTextCharFormat();
    m_functionFmt.setForeground(QColor(QStringLiteral("#7DD3FC")));

    m_unitFmt = QTextCharFormat();
    m_unitFmt.setForeground(QColor(QStringLiteral("#A5B4FC")));
}

void Highlighter::highlightBlock(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    if (trimmed.startsWith(QLatin1Char('#'))) {
        setFormat(0, text.size(), m_headingFmt);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("//"))) {
        setFormat(0, text.size(), m_commentFmt);
        return;
    }

    // Determine where an inline comment starts so we only highlight code before it.
    int codeEnd = text.indexOf(QStringLiteral("//"));
    for (int i = 1; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('#') && text.at(i - 1).isSpace()) {
            codeEnd = (codeEnd < 0) ? i : qMin(codeEnd, i);
            break;
        }
    }
    if (codeEnd >= 0) {
        setFormat(codeEnd, text.size() - codeEnd, m_commentFmt);
    } else {
        codeEnd = text.size();
    }

    const QString code = text.left(codeEnd);

    // Highlight assignment LHS variable in the Omarchy accent color.
    static const QRegularExpression assignRe(
        QStringLiteral(R"(^\s*(?:let\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=))"));
    const auto assignMatch = assignRe.match(code);
    if (assignMatch.hasMatch())
        setFormat(assignMatch.capturedStart(1), assignMatch.capturedLength(1), m_varFmt);

    // Numbers, base literals (0x/0b/0o), currency symbols & percentage signs.
    static const QRegularExpression numRe(
        QStringLiteral(R"(\b0[xX][0-9a-fA-F_]+|\b0[bB][01_]+|\b0[oO][0-7_]+|\b\d[\d_]*(?:\.[\d_]+)?(?:[eE][+-]?\d+)?%?|\.\d+(?:[eE][+-]?\d+)?%?|\$|€|£|¥)"));
    auto numberIt = numRe.globalMatch(code);
    while (numberIt.hasNext()) {
        const auto m = numberIt.next();
        setFormat(m.capturedStart(0), m.capturedLength(0), m_numberFmt);
    }

    // Operators.
    static const QRegularExpression opRe(QStringLiteral(R"(->|\*\*|[+\-*/^=%])"));
    auto opIt = opRe.globalMatch(code);
    while (opIt.hasNext()) {
        const auto m = opIt.next();
        setFormat(m.capturedStart(0), m.capturedLength(0), m_operatorFmt);
    }

    // Identifiers: functions, keywords, currencies, and units.
    static const QSet<QString> keywords = {
        QStringLiteral("in"), QStringLiteral("to"), QStringLiteral("as"),
        QStringLiteral("of"), QStringLiteral("off"), QStringLiteral("let"),
        QStringLiteral("sum"), QStringLiteral("total"), QStringLiteral("avg"),
        QStringLiteral("average"), QStringLiteral("mean"), QStringLiteral("count"),
        QStringLiteral("prev"), QStringLiteral("ans"), QStringLiteral("pi"),
        QStringLiteral("e"), QStringLiteral("tau"),
        QStringLiteral("hex"), QStringLiteral("bin"), QStringLiteral("oct"),
        QStringLiteral("dec")
    };
    static const QSet<QString> currencyWords = {
        QStringLiteral("USD"), QStringLiteral("EUR"), QStringLiteral("GBP"),
        QStringLiteral("JPY"), QStringLiteral("KR"), QStringLiteral("kr")
    };

    static const QRegularExpression wordRe(QStringLiteral(R"([A-Za-z_°][A-Za-z0-9_²]*)"));
    auto wordIt = wordRe.globalMatch(code);
    while (wordIt.hasNext()) {
        const auto m = wordIt.next();
        const int start = m.capturedStart(0);
        const int len = m.capturedLength(0);
        const QString word = m.captured(0);
        const QString lower = word.toLower();

        if (assignMatch.hasMatch() && start == assignMatch.capturedStart(1))
            continue;

        // Function call if immediately followed by '('.
        int after = start + len;
        while (after < code.size() && code.at(after).isSpace())
            ++after;
        if (after < code.size() && code.at(after) == QLatin1Char('(')) {
            setFormat(start, len, m_functionFmt);
            continue;
        }

        if (keywords.contains(lower)) {
            setFormat(start, len, m_keywordFmt);
            continue;
        }

        if (lookupUnit(word).has_value() || currencyWords.contains(word) ||
            currencyWords.contains(word.toUpper())) {
            setFormat(start, len, m_unitFmt);
        }
    }
}
