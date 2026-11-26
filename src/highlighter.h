#pragma once

#include <QColor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit Highlighter(QTextDocument *document = nullptr);

    void setAccent(const QString &hex);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();

    QColor m_accent{"#FFD60A"};
    QTextCharFormat m_headingFmt;
    QTextCharFormat m_commentFmt;
    QTextCharFormat m_varFmt;
    QTextCharFormat m_numberFmt;
    QTextCharFormat m_operatorFmt;
    QTextCharFormat m_keywordFmt;
    QTextCharFormat m_functionFmt;
    QTextCharFormat m_unitFmt;
};
