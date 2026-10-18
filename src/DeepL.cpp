#include "DeepL.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QUrl>

#include "TokenStore.h"

Q_LOGGING_CATEGORY(lcDeepL, "tonal.deepl", QtInfoMsg)

namespace {

const QString kSecretName = QStringLiteral("DeepL");
const QString kSettingsKey = QStringLiteral("deepl/apiKey"); // só sem o cofre do Windows

} // namespace

DeepL::DeepL(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam)
{
    if (const auto stored = TokenStore::readSecret(kSecretName))
        m_apiKey = QString::fromUtf8(*stored);
    else
        m_apiKey = QSettings().value(kSettingsKey).toString();
}

QNetworkReply *DeepL::send(const QString &path, const QString &key, const QByteArray &body)
{
    // Chaves do plano grátis terminam em ":fx" e têm endereço próprio.
    const QString host = key.endsWith(QLatin1String(":fx")) ? QStringLiteral("https://api-free.deepl.com")
                                                             : QStringLiteral("https://api.deepl.com");
    QNetworkRequest req(QUrl(host + path));
    req.setRawHeader("Authorization", "DeepL-Auth-Key " + key.toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Tonal/%1 (https://github.com/natheus/tonal-player)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(20000);
    if (body.isEmpty())
        return m_nam->get(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_nam->post(req, body);
}

QString DeepL::errorText(int status, QNetworkReply *reply)
{
    switch (status) {
    case 401:
    case 403:
        return tr("O DeepL recusou a chave. Confira se copiou a chave inteira.");
    case 456:
        return tr("A cota grátis do DeepL deste mês acabou.");
    case 429:
        return tr("O DeepL recebeu pedidos demais. Tente de novo em alguns segundos.");
    case 0:
        return tr("Não consegui falar com o DeepL (%1).").arg(reply->errorString());
    default:
        return tr("O DeepL respondeu com erro %1.").arg(status);
    }
}

void DeepL::setApiKey(const QString &key, const QJSValue &callback)
{
    const QString trimmed = key.trimmed();
    QJSValue cb = callback;
    if (trimmed.isEmpty()) {
        cb.call({ QJSValue(tr("Cole a chave do DeepL.")) });
        return;
    }
    QNetworkReply *reply = send(QStringLiteral("/v2/usage"), trimmed, {});
    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmed, cb]() mutable {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCDebug(lcDeepL) << "conferência da chave ->" << status;
        if (status != 200) {
            cb.call({ QJSValue(errorText(status, reply)) });
            return;
        }
        storeApiKey(trimmed);
        cb.call({ QJSValue(QString()) });
    });
}

void DeepL::forgetApiKey()
{
    storeApiKey(QString());
}

void DeepL::storeApiKey(const QString &key)
{
    QSettings settings;
    const bool inVault = key.isEmpty()
        ? TokenStore::deleteSecret(kSecretName)
        : TokenStore::writeSecret(kSecretName, key.toUtf8(), QStringLiteral("DeepL API"),
                                  QStringLiteral("Chave do DeepL usada pelo Tonal nas legendas da letra"));
    if (inVault || key.isEmpty())
        settings.remove(kSettingsKey);
    else
        settings.setValue(kSettingsKey, key); // sem o cofre: fica no QSettings, como os tokens
    if (key == m_apiKey)
        return;
    m_apiKey = key;
    emit apiKeyChanged();
}

void DeepL::translate(const QStringList &texts, const QString &targetLang, Done done)
{
    if (m_apiKey.isEmpty()) {
        done(tr("Falta a chave do DeepL."), 403, {}, {});
        return;
    }
    const QJsonObject body {
        { QStringLiteral("text"), QJsonArray::fromStringList(texts) },
        { QStringLiteral("target_lang"), targetLang },
        { QStringLiteral("split_sentences"), QStringLiteral("0") }, // uma linha da letra = uma frase
        { QStringLiteral("preserve_formatting"), true },
    };
    QNetworkReply *reply = send(QStringLiteral("/v2/translate"), m_apiKey,
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done, count = texts.size()] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCDebug(lcDeepL) << "POST /v2/translate" << count << "textos ->" << status;
        if (status != 200) {
            qCWarning(lcDeepL) << "DeepL:" << status << reply->errorString();
            done(errorText(status, reply), status, {}, {});
            return;
        }
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll()).object()
                                     .value(QStringLiteral("translations")).toArray();
        if (items.size() != count) {
            done(tr("O DeepL respondeu com %1 traduções para %2 linhas.").arg(items.size()).arg(count), status, {}, {});
            return;
        }
        QStringList translations;
        QStringList languages;
        for (const QJsonValue &item : items) {
            translations << item.toObject().value(QStringLiteral("text")).toString();
            languages << item.toObject().value(QStringLiteral("detected_source_language")).toString();
        }
        done(QString(), status, translations, languages);
    });
}
