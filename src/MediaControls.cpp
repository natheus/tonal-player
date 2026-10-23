#ifdef _WIN32
// As interfaces do Windows entram antes do Qt, para as macros do Qt (signals, slots, emit) não chegarem
// aos cabeçalhos delas. São as interfaces COM do Windows SDK (WRL), não o C++/WinRT: o C++/WinRT exige
// corrotinas, que no MSVC atual só existem em C++20.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <SystemMediaTransportControlsInterop.h>
#include <roapi.h>
#include <shobjidl_core.h>
#include <windows.foundation.h>
#include <windows.media.h>
#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>
#endif

#include "MediaControls.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSettings>
#include <QWindow>

#include "Player.h"

Q_LOGGING_CATEGORY(lcMedia, "tonal.media", QtInfoMsg)

#ifdef _WIN32

namespace wrl = Microsoft::WRL;
namespace wf = ABI::Windows::Foundation;
namespace media = ABI::Windows::Media;
namespace streams = ABI::Windows::Storage::Streams;

namespace {

// Os eventos do SMTC chegam numa thread do sistema. Eles só postam o pedido para a thread principal, que
// confere se a instância ainda existe: a thread do sistema nunca toca no objeto.
MediaControls *s_instance = nullptr;

using ButtonHandler = wf::ITypedEventHandler<media::SystemMediaTransportControls *,
                                             media::SystemMediaTransportControlsButtonPressedEventArgs *>;
using SeekHandler = wf::ITypedEventHandler<media::SystemMediaTransportControls *,
                                           media::PlaybackPositionChangeRequestedEventArgs *>;

bool ok(HRESULT hr, const char *what)
{
    if (SUCCEEDED(hr))
        return true;
    qCWarning(lcMedia).noquote() << what << QString::number(ulong(hr), 16);
    return false;
}

// O QString termina em zero, como o Windows exige de uma referência de HSTRING. A referência vale até o fim
// da expressão em que é criada, e o Windows copia o texto se precisar guardá-lo.
wrl::Wrappers::HStringReference hstring(const QString &text)
{
    return wrl::Wrappers::HStringReference(reinterpret_cast<const wchar_t *>(text.utf16()), UINT32(text.size()));
}

wf::TimeSpan timeSpan(qint64 ms)
{
    return wf::TimeSpan { ms * 10000 }; // unidades de 100 ns
}

wrl::ComPtr<streams::IRandomAccessStreamReference> thumbnailFromUrl(const QString &url)
{
    wrl::ComPtr<wf::IUriRuntimeClassFactory> uriFactory;
    wrl::ComPtr<wf::IUriRuntimeClass> uri;
    wrl::ComPtr<streams::IRandomAccessStreamReferenceStatics> references;
    wrl::ComPtr<streams::IRandomAccessStreamReference> thumbnail;
    if (ok(RoGetActivationFactory(wrl::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                                  IID_PPV_ARGS(&uriFactory)), "Uri:")
        && ok(uriFactory->CreateUri(hstring(url).Get(), &uri), "CreateUri:")
        && ok(RoGetActivationFactory(
                  wrl::Wrappers::HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(),
                  IID_PPV_ARGS(&references)), "RandomAccessStreamReference:")
        && ok(references->CreateFromUri(uri.Get(), &thumbnail), "CreateFromUri:"))
        return thumbnail;
    return {};
}

// Botões da miniatura na barra de tarefas (ao passar o mouse sobre o Tonal): anterior, tocar/pausar, próxima.
enum ThumbButton : UINT { ThumbPrevious = 1, ThumbPlayPause, ThumbNext };

// Os mesmos desenhos de Icons.qml (grade 24x24, só M, L e Z): prevF, playF, pauseF, nextF.
const char *const kThumbIcons[4] = {
    "M19 5 L19 19 L9 12 Z M5 5 L7.5 5 L7.5 19 L5 19 Z",
    "M7 4.5 L19.5 12 L7 19.5 Z",
    "M6 4 L10 4 L10 20 L6 20 Z M14 4 L18 4 L18 20 L14 20 Z",
    "M5 5 L15 12 L5 19 Z M16.5 5 L19 5 L19 19 L16.5 19 Z",
};

HICON makeIcon(const char *svgPath, int size, const QColor &color)
{
    static const QRegularExpression token(QStringLiteral(R"([MLZ]|-?\d*\.?\d+)"));
    QPainterPath path;
    QChar command;
    QList<qreal> numbers;
    for (auto it = token.globalMatch(QLatin1String(svgPath)); it.hasNext();) {
        const QString t = it.next().captured();
        if (t == QLatin1String("Z")) {
            path.closeSubpath();
        } else if (t == QLatin1String("M") || t == QLatin1String("L")) {
            command = t.at(0);
        } else {
            numbers << t.toDouble();
            if (numbers.size() < 2)
                continue;
            const QPointF point(numbers.at(0), numbers.at(1));
            numbers.clear();
            if (command == QLatin1Char('M'))
                path.moveTo(point);
            else
                path.lineTo(point);
        }
    }
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);
    painter.fillPath(path, color);
    painter.end();
    return image.toHICON();
}

} // namespace

struct MediaControls::Native : QAbstractNativeEventFilter
{
    MediaControls *owner = nullptr;
    HWND hwnd = nullptr;

    // SMTC: nulo se o Windows recusou (os botões da miniatura funcionam mesmo assim).
    wrl::ComPtr<media::ISystemMediaTransportControls> smtc;
    wrl::ComPtr<media::ISystemMediaTransportControls2> smtc2;
    EventRegistrationToken buttonToken {};
    EventRegistrationToken seekToken {};

    // Miniatura na barra de tarefas
    wrl::ComPtr<ITaskbarList3> taskbar;
    UINT taskbarButtonCreated = 0;
    HICON icons[4] {};      // anterior, tocar, pausar, próxima
    int thumbState = -1;    // bit 0: tem faixa; bit 1: tocando (-1 = botões ainda não estão na barra)

    ~Native() override
    {
        if (QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(this);
        for (HICON icon : icons) {
            if (icon)
                DestroyIcon(icon);
        }
    }

    bool attachSmtc();
    void attachThumbBar();
    void addThumbButtons();
    void updateThumbButtons(bool hasTrack, bool playing);
    void fillThumbButtons(THUMBBUTTON *buttons, bool hasTrack, bool playing) const;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};

bool MediaControls::Native::attachSmtc()
{
    wrl::ComPtr<ISystemMediaTransportControlsInterop> interop;
    if (!ok(RoGetActivationFactory(
                wrl::Wrappers::HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(),
                IID_PPV_ARGS(&interop)), "Controle de mídia do Windows indisponível:")
        || !ok(interop->GetForWindow(hwnd, IID_PPV_ARGS(&smtc)),
               "Controle de mídia do Windows indisponível (GetForWindow):")
        || !ok(smtc.As(&smtc2), "Controle de mídia do Windows sem a linha do tempo:"))
        return false;

    smtc->put_IsPlayEnabled(true);
    smtc->put_IsPauseEnabled(true);
    smtc->put_IsStopEnabled(true);
    smtc->put_IsNextEnabled(true);
    smtc->put_IsPreviousEnabled(true);
    smtc2->put_PlaybackRate(1.0);

    auto onButton = wrl::Callback<ButtonHandler>(
        [](media::ISystemMediaTransportControls *, media::ISystemMediaTransportControlsButtonPressedEventArgs *args) {
            media::SystemMediaTransportControlsButton button {};
            if (SUCCEEDED(args->get_Button(&button))) {
                QMetaObject::invokeMethod(QCoreApplication::instance(), [button] {
                    if (s_instance)
                        s_instance->handleButton(int(button));
                }, Qt::QueuedConnection);
            }
            return S_OK;
        });
    auto onSeek = wrl::Callback<SeekHandler>(
        [](media::ISystemMediaTransportControls *, media::IPlaybackPositionChangeRequestedEventArgs *args) {
            wf::TimeSpan position {};
            if (SUCCEEDED(args->get_RequestedPlaybackPosition(&position))) {
                const qint64 ms = position.Duration / 10000;
                QMetaObject::invokeMethod(QCoreApplication::instance(), [ms] {
                    if (s_instance)
                        s_instance->handleSeek(ms);
                }, Qt::QueuedConnection);
            }
            return S_OK;
        });
    if (!ok(smtc->add_ButtonPressed(onButton.Get(), &buttonToken), "add_ButtonPressed:"))
        return false;
    if (!ok(smtc2->add_PlaybackPositionChangeRequested(onSeek.Get(), &seekToken),
            "add_PlaybackPositionChangeRequested:")) {
        smtc->remove_ButtonPressed(buttonToken);
        return false;
    }
    return true;
}

void MediaControls::Native::attachThumbBar()
{
    if (!ok(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar)),
            "Barra de tarefas indisponível:")
        || !ok(taskbar->HrInit(), "Barra de tarefas indisponível (HrInit):")) {
        taskbar.Reset();
        return;
    }
    // Ícones no tamanho pequeno do DPI da janela, na cor que contrasta com o tema da barra de tarefas.
    const int size = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForWindow(hwnd));
    const bool lightTaskbar = QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                                        QSettings::NativeFormat).value(QStringLiteral("SystemUsesLightTheme"), 0).toInt() == 1;
    const QColor color = lightTaskbar ? QColor(0, 0, 0) : QColor(255, 255, 255);
    for (int i = 0; i < 4; ++i)
        icons[i] = makeIcon(kThumbIcons[i], size > 0 ? size : 16, color);

    // Os botões só podem entrar depois que o Windows cria o botão da janela na barra, e de novo se o Explorer
    // reiniciar: as duas vezes chega a mensagem "TaskbarButtonCreated".
    taskbarButtonCreated = RegisterWindowMessageW(L"TaskbarButtonCreated");
    QCoreApplication::instance()->installNativeEventFilter(this);
    addThumbButtons(); // se o botão já existir (a mensagem pode ter chegado antes do filtro)
}

void MediaControls::Native::fillThumbButtons(THUMBBUTTON *buttons, bool hasTrack, bool playing) const
{
    const struct { UINT id; HICON icon; QString tip; } spec[3] = {
        { ThumbPrevious, icons[0], MediaControls::tr("Anterior") },
        { ThumbPlayPause, playing ? icons[2] : icons[1], playing ? MediaControls::tr("Pausar") : MediaControls::tr("Tocar") },
        { ThumbNext, icons[3], MediaControls::tr("Próxima") },
    };
    for (int i = 0; i < 3; ++i) {
        buttons[i] = {};
        buttons[i].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[i].iId = spec[i].id;
        buttons[i].hIcon = spec[i].icon;
        buttons[i].dwFlags = hasTrack ? THBF_ENABLED : THBF_DISABLED;
        wcsncpy_s(buttons[i].szTip, reinterpret_cast<const wchar_t *>(spec[i].tip.utf16()), _TRUNCATE);
    }
}

void MediaControls::Native::addThumbButtons()
{
    if (!taskbar)
        return;
    const Player *p = owner->m_player;
    THUMBBUTTON buttons[3];
    fillThumbButtons(buttons, p->hasTrack(), p->isPlaying());
    // Falha se os botões já estão lá (só podem entrar uma vez por botão da barra): não é erro.
    if (SUCCEEDED(taskbar->ThumbBarAddButtons(hwnd, 3, buttons)))
        thumbState = (p->hasTrack() ? 1 : 0) | (p->isPlaying() ? 2 : 0);
}

void MediaControls::Native::updateThumbButtons(bool hasTrack, bool playing)
{
    const int state = (hasTrack ? 1 : 0) | (playing ? 2 : 0);
    if (!taskbar || thumbState < 0 || state == thumbState)
        return;
    THUMBBUTTON buttons[3];
    fillThumbButtons(buttons, hasTrack, playing);
    if (ok(taskbar->ThumbBarUpdateButtons(hwnd, 3, buttons), "ThumbBarUpdateButtons:"))
        thumbState = state;
}

bool MediaControls::Native::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "windows_generic_MSG")
        return false;
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->hwnd != hwnd)
        return false;
    if (msg->message == taskbarButtonCreated) {
        thumbState = -1; // Explorer reiniciado: a barra nova não tem os botões
        addThumbButtons();
        return false;
    }
    if (msg->message != WM_COMMAND || HIWORD(msg->wParam) != THBN_CLICKED)
        return false;
    // Os mesmos comandos dos botões da barra do player.
    Player *player = owner->m_player;
    switch (LOWORD(msg->wParam)) {
    case ThumbPrevious:
        player->previous();
        break;
    case ThumbPlayPause:
        player->togglePlay();
        break;
    case ThumbNext:
        player->next();
        break;
    default:
        return false;
    }
    // No laço de eventos o Qt chama os filtros com result nulo (só o da janela tem onde escrever).
    if (result)
        *result = 0;
    return true;
}

MediaControls::MediaControls(Player *player, QObject *parent)
    : QObject(parent), m_player(player)
{
    // O Player emite trackChanged, stateChanged e positionChanged em sequência a cada consulta: junta tudo
    // numa atualização só.
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(0);
    connect(&m_syncTimer, &QTimer::timeout, this, &MediaControls::sync);
}

MediaControls::~MediaControls()
{
    if (!d)
        return;
    s_instance = nullptr;
    if (d->smtc) {
        d->smtc->remove_ButtonPressed(d->buttonToken);
        d->smtc2->remove_PlaybackPositionChangeRequested(d->seekToken);
        d->smtc->put_IsEnabled(false);
    }
}

void MediaControls::setAppUserModelId()
{
    // O mesmo id vai no AppUserModelID dos atalhos em packaging/tonal.iss. Não mude: o painel de mídia e a
    // barra de tarefas deixam de achar o atalho.
    ok(SetCurrentProcessExplicitAppUserModelID(L"Tonal.Player"), "AppUserModelID:");
}

void MediaControls::attach(QWindow *window)
{
    if (!window || d)
        return;
    d = std::make_unique<Native>();
    d->owner = this;
    d->hwnd = reinterpret_cast<HWND>(window->winId());
    s_instance = this;
    if (!d->attachSmtc()) {
        d->smtc.Reset();
        d->smtc2.Reset();
    }
    d->attachThumbBar();

    for (auto signal : { &Player::trackChanged, &Player::stateChanged, &Player::positionChanged })
        connect(m_player, signal, &m_syncTimer, qOverload<>(&QTimer::start));
    sync();
}

void MediaControls::sync()
{
    const Player *p = m_player;
    d->updateThumbButtons(p->hasTrack(), p->isPlaying());
    if (!d->smtc)
        return;
    if (p->trackUri() != m_shownTrack) {
        m_shownTrack = p->trackUri();
        m_shownStatus = -1; // faixa nova: manda o estado e a linha do tempo de novo
        wrl::ComPtr<media::ISystemMediaTransportControlsDisplayUpdater> updater;
        wrl::ComPtr<media::IMusicDisplayProperties> music;
        wrl::ComPtr<media::IMusicDisplayProperties2> music2;
        if (ok(d->smtc->get_DisplayUpdater(&updater), "DisplayUpdater:")) {
            updater->ClearAll();
            if (!m_shownTrack.isEmpty() && ok(updater->put_Type(media::MediaPlaybackType_Music), "put_Type:")
                && ok(updater->get_MusicProperties(&music), "MusicProperties:")) {
                music->put_Title(hstring(p->trackName()).Get());
                music->put_Artist(hstring(p->artistsText()).Get());
                if (SUCCEEDED(music.As(&music2)))
                    music2->put_AlbumTitle(hstring(p->albumName()).Get());
                // O Windows baixa a capa sozinho a partir da URL.
                if (!p->coverUrlLarge().isEmpty()) {
                    if (auto thumbnail = thumbnailFromUrl(p->coverUrlLarge()))
                        updater->put_Thumbnail(thumbnail.Get());
                }
            }
            ok(updater->Update(), "Update:");
        }
    }

    const auto status = !p->hasTrack() ? media::MediaPlaybackStatus_Closed
                        : p->isPlaying() ? media::MediaPlaybackStatus_Playing
                                         : media::MediaPlaybackStatus_Paused;
    const bool statusChanged = int(status) != m_shownStatus;
    if (statusChanged) {
        m_shownStatus = int(status);
        d->smtc->put_IsEnabled(p->hasTrack());
        ok(d->smtc->put_PlaybackStatus(status), "put_PlaybackStatus:");
    }
    if (!p->hasTrack())
        return;

    // Enquanto toca, o painel do Windows anda com a posição sozinho. Só manda de novo quando o estado muda
    // ou a posição pula (seek, ou a consulta ao Spotify corrigiu a interpolação).
    const int position = p->positionMs();
    if (!statusChanged) {
        const qint64 expected = m_shownPosition + (p->isPlaying() ? m_shownClock.elapsed() : 0);
        if (qAbs(position - expected) <= 1500)
            return;
    }
    wrl::ComPtr<IInspectable> instance;
    wrl::ComPtr<media::ISystemMediaTransportControlsTimelineProperties> timeline;
    if (ok(RoActivateInstance(wrl::Wrappers::HStringReference(
                                  RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(),
                              &instance), "TimelineProperties:")
        && ok(instance.As(&timeline), "TimelineProperties:")) {
        timeline->put_StartTime(timeSpan(0));
        timeline->put_EndTime(timeSpan(p->durationMs()));
        timeline->put_MinSeekTime(timeSpan(0));
        timeline->put_MaxSeekTime(timeSpan(p->durationMs()));
        timeline->put_Position(timeSpan(position));
        ok(d->smtc2->UpdateTimelineProperties(timeline.Get()), "UpdateTimelineProperties:");
    }
    m_shownPosition = position;
    m_shownClock.start();
}

void MediaControls::handleButton(int button)
{
    switch (media::SystemMediaTransportControlsButton(button)) {
    case media::SystemMediaTransportControlsButton_Play:
        if (!m_player->isPlaying())
            m_player->togglePlay();
        break;
    case media::SystemMediaTransportControlsButton_Pause:
    case media::SystemMediaTransportControlsButton_Stop:
        if (m_player->isPlaying())
            m_player->togglePlay();
        break;
    case media::SystemMediaTransportControlsButton_Next:
        m_player->next();
        break;
    case media::SystemMediaTransportControlsButton_Previous:
        m_player->previous();
        break;
    default:
        break;
    }
}

void MediaControls::handleSeek(qint64 ms)
{
    m_player->seek(int(qBound(qint64(0), ms, qint64(m_player->durationMs()))));
}

#else // Ainda sem equivalente nos outros sistemas (no Linux seria o MPRIS).

struct MediaControls::Native
{
};

MediaControls::MediaControls(Player *player, QObject *parent)
    : QObject(parent), m_player(player)
{
}

MediaControls::~MediaControls() = default;

void MediaControls::attach(QWindow *window)
{
    Q_UNUSED(window)
}

void MediaControls::setAppUserModelId()
{
}

#endif
