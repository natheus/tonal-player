#pragma once

#include <QJSValue>
#include <QObject>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

// Tradução pelo DeepL (API oficial), usada nas legendas da letra. Cada pessoa usa a própria chave (o plano
// DeepL API Free dá 500 mil caracteres por mês), guardada no Gerenciador de Credenciais como "Tonal/DeepL": o
// código é público, então não dá para embutir uma chave no app.
class DeepL : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY apiKeyChanged)

public:
    // erro vazio = deu certo. translations e languages na ordem dos textos pedidos; languages é o idioma que o
    // DeepL detectou em cada texto ("EN", "PT", "ES"...).
    using Done = std::function<void(const QString &error, int status, const QStringList &translations,
                                    const QStringList &languages)>;

    DeepL(QNetworkAccessManager *nam, QObject *parent = nullptr);

    bool hasApiKey() const { return !m_apiKey.isEmpty(); }

    // Confere a chave (GET /v2/usage, que não gasta cota) e só guarda se o DeepL aceitar. callback(erro): vazio =
    // deu certo.
    Q_INVOKABLE void setApiKey(const QString &key, const QJSValue &callback);
    Q_INVOKABLE void forgetApiKey();

    // targetLang: "PT-BR", "EN-US"... Cada texto é traduzido sozinho (split_sentences = 0), o que mantém uma
    // tradução por linha da letra.
    void translate(const QStringList &texts, const QString &targetLang, Done done);

    static QString errorText(int status, QNetworkReply *reply);

signals:
    void apiKeyChanged();

private:
    QNetworkReply *send(const QString &path, const QString &key, const QByteArray &body);
    void storeApiKey(const QString &key);

    QNetworkAccessManager *m_nam;
    QString m_apiKey;
};
