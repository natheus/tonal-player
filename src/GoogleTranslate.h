#pragma once

#include <QObject>
#include <QStringList>

#include "DeepL.h"

class QNetworkAccessManager;
class QNetworkReply;

// Tradução pelo Google sem chave, para as legendas da letra quando não há chave do DeepL. Usa o endereço da
// extensão oficial do Google Tradutor para Chrome (translate_a/t, client=dict-chrome-ex): não é uma API
// documentada, então pode mudar ou parar sem aviso. Em set/2026 o Google passou a recusar com 429 o
// client=gtx, o identificador mais usado por projetos assim; o dict-chrome-ex seguiu respondendo.
class GoogleTranslate : public QObject
{
    Q_OBJECT

public:
    explicit GoogleTranslate(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Mesma assinatura do DeepL: traduções e idiomas detectados ("en", "pt"...) na ordem dos textos. Todas as
    // linhas vão num pedido só.
    void translate(const QStringList &texts, const QString &targetLang, DeepL::Done done);

private:
    QNetworkAccessManager *m_nam;
};
