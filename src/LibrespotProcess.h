#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

struct IMMDeviceEnumerator;
struct IMMNotificationClient;

// Roda o librespot (https://github.com/librespot-org/librespot) como processo
// auxiliar. Ele aparece como um dispositivo Spotify Connect ("Tonal @ PC") e é
// quem de fato baixa, decodifica e toca o áudio. O Tonal só manda comandos
// pela Web API apontando para esse dispositivo.
class LibrespotProcess : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
    Q_PROPERTY(QString executablePath READ executablePath NOTIFY executablePathChanged)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY bitrateChanged)

public:
    enum State { Stopped, NotFound, Starting, NeedsLogin, Ready, Crashed };
    Q_ENUM(State)

    explicit LibrespotProcess(QObject *parent = nullptr);
    ~LibrespotProcess() override;

    State state() const { return m_state; }
    QString statusText() const { return m_statusText; }
    QString deviceName() const { return m_deviceName; }
    QString executablePath() const { return m_exe; }
    int bitrate() const { return m_bitrate; }
    void setBitrate(int kbps);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    // Apaga as credenciais salvas do librespot e refaz o login dele.
    Q_INVOKABLE void resetLogin();
    Q_INVOKABLE void setExecutablePath(const QString &path);
    Q_INVOKABLE QString recentLog() const { return m_log.join(QLatin1Char('\n')); }

signals:
    void stateChanged();
    void executablePathChanged();
    void bitrateChanged();
    // Disparado quando o librespot carrega/troca de faixa, pausa, etc.
    // O Player usa isso para atualizar o estado sem ficar fazendo polling agressivo.
    void playbackEvent();
    // A sessão com o Spotify caiu (o motor vai voltar sozinho). O que estava tocando aqui se perde.
    void sessionLost();

private:
    void setState(State s, const QString &text = {});
    void onReadyRead();
    void handleLine(const QString &line);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    QString findExecutable() const;
    QString cacheDir() const;
    bool hasCachedCredentials() const;
    bool requestGracefulExit();
    void attachToJob();
    void watchOutputDevice();
    void followOutputDevice();

    QProcess m_process;
    QTimer m_restartTimer;
    QTimer m_outputTimer;
    // Saída de som padrão do Windows: a que o motor está usando e a do último aviso.
    QString m_outputId;
    QString m_pendingOutputId;
    IMMDeviceEnumerator *m_enumerator = nullptr;
    IMMNotificationClient *m_outputNotifier = nullptr;
    QByteArray m_buffer;
    QStringList m_log;
    QString m_exe;
    QString m_deviceName;
    QString m_statusText;
    State m_state = Stopped;
    int m_bitrate = 320;
    int m_crashCount = 0;
    bool m_stopping = false;
};
