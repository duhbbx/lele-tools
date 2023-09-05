#include "markdowneditor.h"

#include <QLabel>
#include <QDesktopServices>
#include <QWebChannel>


REGISTER_DYNAMICOBJECT(MarkdownEditor);


void Document::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    emit textChanged(m_text);
}


bool PreviewPage::acceptNavigationRequest(const QUrl &url,
                                          QWebEnginePage::NavigationType /*type*/,
                                          bool /*isMainFrame*/)
{
    // Only allow qrc:/index.html.
    qDebug() << url.scheme() << ", " << url;
    if (url.scheme() == QString("qrc"))
        return true;
    QDesktopServices::openUrl(url);
    return false;
}

MarkdownEditor::MarkdownEditor() : QWidget(nullptr), DynamicObjectBase()
{
    QHBoxLayout * layout = new QHBoxLayout;
    QPlainTextEdit * textEdit = new QPlainTextEdit;
    QWebEngineView * preview = new QWebEngineView(this);

    layout->addWidget(textEdit, 1);
    layout->addWidget(preview, 1);



    this->textEdit = textEdit;
    this->preview = preview;


    this->setLayout(layout);

    this->textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->preview->setContextMenuPolicy(Qt::NoContextMenu);

    PreviewPage *page = new PreviewPage(this);
    this->preview->setPage(page);

    this->preview->setUrl(QUrl("qrc:/resources/index.html"));



    connect(this->textEdit, &QPlainTextEdit::textChanged, [this]() { m_content.setText(this->textEdit->toPlainText()); });

    QWebChannel *channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("content"), &m_content);
    page->setWebChannel(channel);

}

