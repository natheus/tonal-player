#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class DeepL;
class GoogleTranslate;
class Player;
class QNetworkAccessManager;
class QNetworkReply;

// Letra da faixa atual, do LRCLIB (lrclib.net, acervo aberto de letras sincronizadas: a Web API do Spotify não
// tem letras). Só busca enquanto a tela de letra está aberta (active), para não mandar ao LRCLIB o nome de
// cada música tocada. Guarda as últimas letras em memória.
//
// Legendas: com showTranslation ligado, cada linha ganha a tradução para o idioma do app (pt-BR ou inglês).
// Traduz pelo DeepL quando há chave e, sem ela (ou com a cota do DeepL acabada), pelo Google sem chave. As
// linhas vão uma vez só (refrão repetido não gasta cota) e as que já estão no idioma do app ficam sem legenda.
// As traduções ficam num arquivo por música no cache, para não pedir de novo.
class Lyrics : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(Status status READ status NOTIFY changed)
    // Linhas como { time (ms, -1 na letra sem sincronia), text }. Texto vazio = trecho instrumental.
    Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)
    Q_PROPERTY(bool showTranslation READ showTranslation WRITE setShowTranslation NOTIFY showTranslationChanged)
    Q_PROPERTY(TranslationStatus translationStatus READ translationStatus NOTIFY translationChanged)
    Q_PROPERTY(QString translationError READ translationError NOTIFY translationChanged)
    // Quem traduziu a letra na tela: "Google" ou "DeepL" (para a atribuição).
    Q_PROPERTY(QString translationProvider READ translationProvider NOTIFY translationChanged)
    // Muda a cada tradução: os delegates leem translationAt(i) junto com ela, para atualizar.
    Q_PROPERTY(int translationRevision READ translationRevision NOTIFY translationChanged)

public:
    enum Status { Idle, Loading, Synced, Plain, Instrumental, NotFound, Error };
    Q_ENUM(Status)
    // AlreadyInLanguage: a letra inteira já está no idioma do app, não há o que legendar. NeedsKey: o DeepL
    // recusou a chave salva (sem chave, o Tonal usa o Google e não pede nada). TranslationRemoved: a pessoa apagou
    // a tradução desta música; a legenda volta com retranslate() ou na próxima vez que a música abrir.
    enum TranslationStatus { TranslationOff, Translating, Translated, AlreadyInLanguage, NeedsKey, TranslationFailed,
                             TranslationRemoved };
    Q_ENUM(TranslationStatus)

    Lyrics(QNetworkAccessManager *nam, Player *player, DeepL *deepl, QObject *parent = nullptr);

    bool active() const { return m_active; }
    void setActive(bool active);
    Status status() const { return m_status; }
    QVariantList lines() const { return m_lines; }

    // Índice da linha que está tocando em positionMs (-1 antes da primeira).
    Q_INVOKABLE int lineAt(int positionMs) const;
    // Início da linha em ms (-1 se não houver).
    Q_INVOKABLE int lineTime(int index) const;
    Q_INVOKABLE void retry();

    bool showTranslation() const { return m_showTranslation; }
    void setShowTranslation(bool show);
    TranslationStatus translationStatus() const { return m_translationStatus; }
    QString translationError() const { return m_translationError; }
    QString translationProvider() const { return m_translationProvider; }
    int translationRevision() const { return m_translationRevision; }
    // Legenda da linha (vazia se não houver).
    Q_INVOKABLE QString translationAt(int index) const;
    Q_INVOKABLE void retryTranslation();
    // Apaga a tradução salva desta música e traduz de novo. provider "Google" ou "DeepL" escolhe a fonte; vazio
    // usa a de sempre (DeepL com chave, senão Google).
    Q_INVOKABLE void retranslate(const QString &provider = QString());
    // Apaga só a tradução salva desta música e tira as legendas dela da tela, sem buscar de novo.
    Q_INVOKABLE void forgetTranslation();
    // Traduções salvas no disco (uma por música e idioma).
    Q_INVOKABLE int savedTranslationCount() const;
    // Apaga todas e devolve quantas eram. A legenda na tela continua até trocar de música.
    Q_INVOKABLE int clearSavedTranslations();

signals:
    void activeChanged();
    void changed();
    void showTranslationChanged();
    void translationChanged();

private:
    struct Entry
    {
        Status status = NotFound;
        QList<int> times;
        QStringList texts;
    };

    void load();
    void request(const QString &path, const QList<std::pair<QString, QString>> &query, bool isSearch);
    void handleReply(QNetworkReply *reply, bool isSearch, int serial);
    Entry entryFrom(const QJsonObject &obj) const;
    QJsonObject bestMatch(const QJsonArray &results) const;
    void show(const Entry &entry);
    void updateTranslation();
    void applyTranslation(const QStringList &sources, const QStringList &translations,
                          const QStringList &languages, const QString &target);
    void setTranslationStatus(TranslationStatus status);
    static QString translationCacheDir();
    QString translationCacheFile(const QString &target) const;
    void requestTranslation(const QStringList &sources, const QString &target, bool useDeepL);
    void finishTranslation(const QStringList &sources, const QStringList &translated, const QStringList &languages,
                           const QString &target, const QString &provider);

    QNetworkAccessManager *m_nam;
    Player *m_player;
    DeepL *m_deepl;
    GoogleTranslate *m_google;
    bool m_active = false;
    int m_serial = 0;

    // O que foi pedido ao LRCLIB: a busca de reserva usa os mesmos dados.
    QString m_queryArtist;
    QString m_queryTrack;
    int m_queryDurationMs = 0;

    QString m_trackUri;   // faixa das linhas mostradas (ou da busca em andamento)
    Status m_status = Idle;
    QVariantList m_lines;
    QList<int> m_times;
    QStringList m_texts;

    bool m_showTranslation = false;
    TranslationStatus m_translationStatus = TranslationOff;
    QString m_translationError;
    QString m_translationProvider;
    QString m_forcedProvider;     // fonte escolhida em retranslate(), vale para um pedido só
    QStringList m_translations;   // alinhada com m_texts
    int m_translationRevision = 0;
    int m_translationSerial = 0;

    QHash<QString, Entry> m_cache;
    QStringList m_cacheOrder;
};
