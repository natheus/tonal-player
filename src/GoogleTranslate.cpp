#include "GoogleTranslate.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcGoogle, "tonal.google", QtInfoMsg)

GoogleTranslate::GoogleTranslate(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam)
{
}

void GoogleTranslate::translate(const QStringList &texts, const QString &targetLang, DeepL::Done done)
{
    QUrl url(QStringLiteral("https://translate.googleapis.com/translate_a/t"));
    // "PT-BR" (formato do DeepL) → "pt": o português do Google já é o do Brasil.
    url.setQuery(QStringLiteral("client=dict-chrome-ex&sl=auto&tl=") + targetLang.section(QLatin1Char('-'), 0, 0).toLower());

    // Uma linha por "q", no corpo: pela URL, uma letra longa passaria do tamanho máximo.
    QByteArray body;
    for (const QString &text : texts) {
        if (!body.isEmpty())
            body += '&';
        body += "q=" + QUrl::toPercentEncoding(text);
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Tonal/%1 (https://github.com/natheus/tonal-player)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done, count = texts.size()] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCDebug(lcGoogle) << "POST translate_a/t" << count << "textos ->" << status;
        // Bloqueio por excesso: 429 com uma página HTML ("Sorry... automated queries"), às vezes com 200.
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (status == 429 || (status == 200 && !doc.isArray())) {
            qCWarning(lcGoogle) << "Google recusou a tradução:" << status;
            done(tr("O Google bloqueou as traduções por um tempo. Tente de novo mais tarde ou use uma chave do DeepL."),
                 429, {}, {});
            return;
        }
        if (status != 200) {
            qCWarning(lcGoogle) << "Google:" << status << reply->errorString();
            done(status == 0 ? tr("Não consegui falar com o Google Tradutor (%1).").arg(reply->errorString())
                             : tr("O Google Tradutor respondeu com erro %1.").arg(status),
                 status, {}, {});
            return;
        }
        // [[tradução, idioma], ...], um par por "q", inclusive com uma linha só.
        const QJsonArray items = doc.array();
        if (items.size() != count) {
            done(tr("O Google Tradutor respondeu com %1 traduções para %2 linhas.").arg(items.size()).arg(count),
                 status, {}, {});
            return;
        }
        QStringList translations;
        QStringList languages;
        for (const QJsonValue &item : items) {
            const QJsonArray pair = item.toArray();
            translations << (pair.isEmpty() ? item.toString() : pair.at(0).toString());
            languages << pair.at(1).toString();
        }
        done(QString(), status, translations, languages);
    });
}
