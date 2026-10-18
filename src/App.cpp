#include "App.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QQmlEngine>
#include <QColor>
#include <QImage>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QLocale>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>

App::App(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_imageNam(new QNetworkAccessManager(this))
    , m_auth(new AuthManager(m_nam, this))
    , m_api(new SpotifyApi(m_nam, m_auth, this))
    , m_librespot(new LibrespotProcess(this))
    , m_player(new Player(m_api, m_librespot, m_auth, this))
    , m_deepl(new DeepL(m_nam, this))
    , m_lyrics(new Lyrics(m_nam, m_player, m_deepl, this))
    , m_setlistFm(new SetlistFm(m_nam, this))
{
    auto *cache = new QNetworkDiskCache(m_imageNam);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/colors"));
    cache->setMaximumCacheSize(30LL * 1024 * 1024);
    m_imageNam->setCache(cache);

    // O librespot só sobe depois do login da Web API, para não abrir
    // duas abas de login ao mesmo tempo no primeiro uso.
    connect(m_auth, &AuthManager::authenticatedChanged, this, [this] {
        if (m_auth->isAuthenticated())
            m_librespot->start();
        else
            m_librespot->stop();
    });
    if (m_auth->isAuthenticated())
        m_librespot->start();
}

App::~App()
{
    m_librespot->stop();
}

App *App::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    s_instance->m_api->setJsEngine(jsEngine);
    s_instance->m_setlistFm->setJsEngine(jsEngine);
    return s_instance;
}

QString App::version() const
{
    return QCoreApplication::applicationVersion();
}

QString App::language() const
{
    return QLocale().language() == QLocale::English ? QStringLiteral("en") : QStringLiteral("pt");
}

QString App::startupLanguage()
{
    const QString saved = QSettings().value(QStringLiteral("ui/language")).toString();
    if (saved == QLatin1String("pt") || saved == QLatin1String("en"))
        return saved;
    const QStringList system = QLocale::system().uiLanguages();
    return !system.isEmpty() && system.first().startsWith(QLatin1String("pt")) ? QStringLiteral("pt")
                                                                               : QStringLiteral("en");
}

void App::setLanguage(const QString &language)
{
    if (language == QLatin1String("pt") || language == QLatin1String("en"))
        QSettings().setValue(QStringLiteral("ui/language"), language);
}

void App::restart()
{
    s_restartRequested = true;
    QCoreApplication::quit();
}

void App::copyToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

QString App::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

void App::dominantColor(const QString &imageUrl, const QJSValue &callback)
{
    if (imageUrl.isEmpty() || !callback.isCallable())
        return;
    const auto cached = m_colorCache.constFind(imageUrl);
    if (cached != m_colorCache.constEnd()) {
        QJSValue cb = callback;
        cb.call({ QJSValue(*cached) });
        return;
    }

    QNetworkRequest req { QUrl(imageUrl) };
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply *reply = m_imageNam->get(req);
    QJSValue cb = callback;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, imageUrl]() mutable {
        reply->deleteLater();
        QImage img;
        if (reply->error() != QNetworkReply::NoError || !img.loadFromData(reply->readAll()))
            return;

        // Média de cores num thumbnail pequeno, ignorando pixels quase pretos/brancos.
        const QImage small = img.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_RGB32);
        double r = 0, g = 0, b = 0, weight = 0;
        for (int y = 0; y < small.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
            for (int x = 0; x < small.width(); ++x) {
                const QColor c(line[x]);
                const double s = c.hsvSaturationF();
                const double v = c.valueF();
                if (v < 0.12 || (v > 0.95 && s < 0.08))
                    continue;
                const double w = 0.2 + s; // cores saturadas pesam mais
                r += c.redF() * w;
                g += c.greenF() * w;
                b += c.blueF() * w;
                weight += w;
            }
        }
        QColor color = weight > 0 ? QColor::fromRgbF(float(r / weight), float(g / weight), float(b / weight))
                                  : QColor(83, 83, 83);
        float h, s, l, a;
        color.getHslF(&h, &s, &l, &a);
        color = QColor::fromHslF(h < 0 ? 0 : h, qBound(0.0f, s, 0.75f), qBound(0.30f, l, 0.45f));

        const QString name = color.name();
        m_colorCache.insert(imageUrl, name);
        cb.call({ QJSValue(name) });
    });
}
