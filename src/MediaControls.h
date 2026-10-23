#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class Player;
class QWindow;

// Controle de mídia do sistema. No Windows: o SMTC (System Media Transport Controls), em que o painel de mídia
// mostra a faixa, a capa e a posição e as teclas de mídia do teclado chegam mesmo com a janela minimizada ou sem
// foco; e os botões anterior, tocar/pausar e próxima na miniatura do Tonal na barra de tarefas. Só espelha o
// estado do Player e chama os mesmos comandos da barra do player.
class MediaControls : public QObject
{
    Q_OBJECT

public:
    explicit MediaControls(Player *player, QObject *parent = nullptr);
    ~MediaControls() override;

    // O SMTC e os botões da miniatura pertencem a uma janela: chame depois que a janela principal existir.
    void attach(QWindow *window);

    // Id fixo do app no Windows (AppUserModelID). O painel de mídia tira o nome e o ícone do atalho do Menu
    // Iniciar que tem o mesmo id (o instalador cria). Sem ele aparece "Aplicativo desconhecido".
    // Chame antes de criar a primeira janela.
    static void setAppUserModelId();

private:
    void sync();
    void handleButton(int button);
    void handleSeek(qint64 ms);

    struct Native;
    std::unique_ptr<Native> d;

    Player *m_player;
    QTimer m_syncTimer;

    // O que já foi mandado para o sistema, para só atualizar o que mudou.
    QString m_shownTrack;
    int m_shownStatus = -1;
    int m_shownPosition = 0;
    QElapsedTimer m_shownClock;
};
