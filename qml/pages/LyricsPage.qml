import QtQuick
import QtQuick.Controls

// Letra da faixa que está tocando (App.lyrics, do LRCLIB). Na letra sincronizada a linha atual fica em
// destaque, a lista acompanha a música e um clique numa linha pula para ela.
Item {
    id: root

    property var route              // a letra segue a faixa que toca, não a rota
    property color tint: Theme.elevated
    property int current: -1
    property bool follow: true      // acompanha a música; para por alguns segundos quando a pessoa rola
    readonly property int status: App.lyrics.status
    readonly property bool synced: status === Lyrics.Synced
    readonly property bool hasLines: synced || status === Lyrics.Plain
    readonly property int translationStatus: App.lyrics.translationStatus
    property bool checkingKey: false
    property string keyError: ""
    property bool editingKey: false     // cartão da chave do DeepL aberto pelo link (sem chave, traduz pelo Google)

    function saveKey() {
        if (checkingKey) return
        checkingKey = true
        keyError = ""
        App.deepl.setApiKey(keyField.text, function(error) {
            root.checkingKey = false
            root.keyError = error
            if (error === "") {
                keyField.text = ""
                root.editingKey = false
            }
        })
    }

    // Destaca a linha um pouco antes: a posição do Spotify chega com o atraso da rede.
    readonly property int lead: 250

    Component.onCompleted: {
        App.lyrics.active = true
        root.updateTint()
        root.sync()
    }
    Component.onDestruction: App.lyrics.active = false

    function updateTint() {
        const url = App.player.coverUrl
        if (!url) {
            root.tint = Theme.elevated
            return
        }
        App.dominantColor(url, function(c) { if (url === App.player.coverUrl) root.tint = c })
    }

    // Acha a linha atual e agenda o timer para a próxima troca de linha (em vez de conferir sem parar).
    function sync() {
        if (!root.synced) {
            root.current = -1
            tick.stop()
            return
        }
        const pos = App.player.positionMs + root.lead
        const i = App.lyrics.lineAt(pos)
        root.current = i
        const next = App.lyrics.lineTime(i + 1)
        if (App.player.isPlaying && next >= 0) {
            tick.interval = Math.max(30, next - pos + 5)
            tick.restart()
        } else {
            tick.stop()
        }
    }

    Timer { id: tick; onTriggered: root.sync() }
    Timer { id: resumeFollow; interval: 4000; onTriggered: root.follow = true }

    Connections {
        target: App.player
        function onPositionChanged() { root.sync() }
        function onStateChanged() { root.sync() }
        function onTrackChanged() { root.updateTint() }
    }
    Connections {
        target: App.lyrics
        function onChanged() {
            root.follow = true
            list.positionViewAtBeginning()
            root.sync()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.darker(root.tint, 1.7)
        Behavior on color { ColorAnimation { duration: 400 } }
    }

    // ------------------------------------------------------------ legendas: botão e status
    Item {
        id: translationBar
        visible: root.hasLines
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 56 : 0

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: text !== ""
                color: root.translationStatus === Lyrics.TranslationFailed ? Theme.danger : Theme.textSecondary
                font.pixelSize: 13
                text: {
                    switch (root.translationStatus) {
                    case Lyrics.Translating: return qsTr("Traduzindo…")
                    case Lyrics.Translated:
                        return App.lyrics.translationProvider === "DeepL" ? qsTr("Tradução automática do DeepL")
                                                                          : qsTr("Tradução automática do Google")
                    case Lyrics.AlreadyInLanguage:
                        return App.language === "en" ? qsTr("A letra já está em inglês") : qsTr("A letra já está em português")
                    case Lyrics.TranslationFailed: return App.lyrics.translationError
                    case Lyrics.TranslationRemoved: return qsTr("Tradução desta música apagada")
                    default: return ""
                    }
                }
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.translationStatus === Lyrics.TranslationFailed
                text: qsTr("Tentar de novo")
                color: retryMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: 13
                font.underline: retryMouse.containsMouse
                MouseArea {
                    id: retryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.lyrics.retryTranslation()
                }
            }
            IconButton {
                id: translationMenuButton
                anchors.verticalCenter: parent.verticalCenter
                visible: App.lyrics.showTranslation
                iconSize: 20
                fillPath: Icons.dotsF
                tooltip: qsTr("Opções da tradução")
                onClicked: translationMenu.popup(translationMenuButton, 0, translationMenuButton.height)
            }
            Chip {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Tradução")
                checked: App.lyrics.showTranslation
                onClicked: {
                    App.lyrics.showTranslation = !App.lyrics.showTranslation
                    root.editingKey = false
                }
            }
        }
    }

    // Menu ⋯ da tradução: buscar de novo em outra fonte, chave do DeepL e limpar o que ficou salvo.
    DarkMenu {
        id: translationMenu
        property int savedCount: 0
        onAboutToShow: savedCount = App.lyrics.savedTranslationCount()

        // Apagam a tradução salva desta música e buscam de novo na fonte escolhida.
        DarkMenuItem {
            text: qsTr("Traduzir de novo pelo Google")
            onTriggered: App.lyrics.retranslate("Google")
        }
        DarkMenuItem {
            text: qsTr("Traduzir de novo pelo DeepL")
            visible: App.deepl.hasApiKey
            height: visible ? implicitHeight : 0
            onTriggered: App.lyrics.retranslate("DeepL")
        }
        DarkMenuItem {
            text: qsTr("Apagar a tradução desta música")
            enabled: root.translationStatus === Lyrics.Translated || root.translationStatus === Lyrics.AlreadyInLanguage
            onTriggered: App.lyrics.forgetTranslation()
        }
        MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: Theme.divider }
        }
        DarkMenuItem {
            text: App.deepl.hasApiKey ? qsTr("Trocar a chave do DeepL") : qsTr("Usar o DeepL")
            onTriggered: root.editingKey = true
        }
        MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: Theme.divider }
        }
        DarkMenuItem {
            text: qsTr("Apagar as traduções salvas (%1)").arg(translationMenu.savedCount)
            enabled: translationMenu.savedCount > 0
            onTriggered: {
                const n = App.lyrics.clearSavedTranslations()
                Nav.toast(n === 1 ? qsTr("1 tradução salva apagada") : qsTr("%1 traduções salvas apagadas").arg(n))
            }
        }
    }

    // Chave do DeepL: aberta pelo menu ⋯ ("Usar o DeepL"), ou sozinha quando o DeepL recusa a chave salva
    Rectangle {
        id: keyCard
        visible: root.hasLines && App.lyrics.showTranslation
                 && (root.editingKey || root.translationStatus === Lyrics.NeedsKey)
        anchors.top: translationBar.bottom
        anchors.left: parent.left
        anchors.leftMargin: 48
        width: Math.min(parent.width - 72, 720)
        height: visible ? keyColumn.height + 40 : 0
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.35)

        Column {
            id: keyColumn
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 12

            Label {
                text: qsTr("Traduzir com o DeepL")
                color: Theme.textPrimary
                font.pixelSize: 18
                font.weight: Font.Bold
            }
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                font.pixelSize: 14
                text: qsTr("Sem chave, o Tonal traduz pelo Google Tradutor, sem cadastro. Com uma chave do DeepL (plano grátis de 500 mil caracteres por mês, umas 200 letras), ele passa a usar o DeepL, que costuma traduzir melhor e é uma API oficial. A chave fica guardada no Gerenciador de Credenciais do Windows.")
            }
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.textPrimary
                font.pixelSize: 14
                lineHeight: 1.3
                text: qsTr("1. Crie uma conta no plano DeepL API Free (o cadastro pede um cartão só para confirmar a identidade).\n2. Na sua conta do DeepL, abra Chaves de API e copie a chave.\n3. Cole a chave aqui.")
            }
            PillButton {
                primary: false
                text: qsTr("Abrir o DeepL")
                onClicked: Qt.openUrlExternally("https://www.deepl.com/pro-api")
            }
            Row {
                width: parent.width
                spacing: 12
                TextField {
                    id: keyField
                    width: parent.width - saveKeyButton.width - 12
                           - (cancelKeyButton.visible ? cancelKeyButton.width + 12 : 0)
                           - (removeKeyButton.visible ? removeKeyButton.width + 12 : 0)
                    height: 40
                    leftPadding: 12
                    placeholderText: qsTr("Chave do DeepL")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.family: "Consolas"
                    selectByMouse: true
                    enabled: !root.checkingKey
                    onAccepted: root.saveKey()
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: keyField.activeFocus ? 1 : 0
                        border.color: Theme.textMuted
                    }
                }
                PillButton {
                    id: saveKeyButton
                    text: root.checkingKey ? qsTr("Conferindo…") : qsTr("Salvar")
                    enabled: keyField.text.trim() !== "" && !root.checkingKey
                    onClicked: root.saveKey()
                }
                PillButton {
                    id: cancelKeyButton
                    visible: root.translationStatus !== Lyrics.NeedsKey
                    primary: false
                    text: qsTr("Cancelar")
                    onClicked: {
                        root.editingKey = false
                        root.keyError = ""
                        keyField.text = ""
                    }
                }
                // Volta para o Google (também a saída quando o DeepL recusa a chave).
                PillButton {
                    id: removeKeyButton
                    visible: App.deepl.hasApiKey
                    primary: false
                    text: qsTr("Remover a chave")
                    onClicked: {
                        App.deepl.forgetApiKey()
                        root.editingKey = false
                        root.keyError = ""
                    }
                }
            }
            Label {
                // Erro ao salvar ou, depois de uma recusa, o motivo que o DeepL deu
                readonly property string message: root.keyError !== "" ? root.keyError : App.lyrics.translationError
                visible: message !== ""
                width: parent.width
                wrapMode: Text.WordWrap
                text: message
                color: Theme.danger
                font.pixelSize: 13
            }
        }
    }

    ListView {
        id: list
        anchors.top: keyCard.visible ? keyCard.bottom : translationBar.bottom
        anchors.topMargin: keyCard.visible ? 16 : 0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 48
        anchors.rightMargin: 24
        visible: root.hasLines
        model: visible ? App.lyrics.lines : []
        reuseItems: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        topMargin: 16
        bottomMargin: height * 0.5          // deixa as últimas linhas subirem até a altura da linha atual
        ScrollBar.vertical: ThinScrollBar { }

        // Um highlight vazio faz a lista rolar com animação até a linha atual, mantendo-a a 30% da altura.
        currentIndex: root.current
        highlight: Item { }
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 450
        highlightMoveVelocity: -1
        highlightRangeMode: root.follow && root.synced ? ListView.ApplyRange : ListView.NoHighlightRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.3 + 48

        onMovementStarted: {
            root.follow = false
            resumeFollow.stop()
        }
        onMovementEnded: resumeFollow.restart()

        // A altura fica no Item: no próprio Label, height ligada ao implicitHeight dava "Binding loop".
        header: Item {
            width: ListView.view.width
            height: plainNote.visible ? plainNote.implicitHeight + 24 : 0
            Label {
                id: plainNote
                width: parent.width
                visible: root.status === Lyrics.Plain
                text: qsTr("Esta letra não está sincronizada com a música.")
                color: Theme.textSecondary
                font.pixelSize: 14
            }
        }

        delegate: Item {
            id: line
            required property var modelData
            required property int index
            // translationRevision muda a cada tradução: é o que faz a legenda aparecer quando chega.
            readonly property string translation: App.lyrics.showTranslation && App.lyrics.translationRevision >= 0
                                                   ? App.lyrics.translationAt(index) : ""

            width: ListView.view.width
            height: lineTexts.height
            opacity: !root.synced || hover.containsMouse || index === root.current ? 1
                     : index < root.current ? 0.7 : 0.45
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Column {
                id: lineTexts
                width: parent.width
                topPadding: 6
                bottomPadding: 6
                spacing: 2

                Text {
                    width: parent.width
                    rightPadding: 24
                    text: line.modelData.text !== "" ? line.modelData.text : (root.synced ? "♪" : " ")
                    wrapMode: Text.Wrap
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                // Legenda: menor e mais clara, logo abaixo da linha original
                Text {
                    visible: line.translation !== ""
                    width: parent.width
                    rightPadding: 24
                    text: line.translation
                    wrapMode: Text.Wrap
                    font.pixelSize: 20
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    opacity: 0.75
                }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                enabled: root.synced
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    App.player.seek(line.modelData.time)
                    root.follow = true
                }
            }
        }

        footer: Label {
            width: ListView.view.width
            topPadding: 32
            bottomPadding: 16
            text: qsTr("Letras do LRCLIB (lrclib.net), um acervo aberto mantido por voluntários.")
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    // Sem letra para mostrar: carregando, não achou, instrumental, erro ou nada tocando.
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 96, 520)
        spacing: 12
        visible: !list.visible

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.textPrimary
            font.pixelSize: 24
            font.weight: Font.Bold
            text: {
                switch (root.status) {
                case Lyrics.Loading: return qsTr("Procurando a letra…")
                case Lyrics.NotFound: return qsTr("Ainda não temos a letra desta música")
                case Lyrics.Instrumental: return qsTr("♪ Esta música é instrumental")
                case Lyrics.Error: return qsTr("Não consegui buscar a letra")
                default: return qsTr("Toque uma música para ver a letra")
                }
            }
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 14
            visible: text !== ""
            text: root.status === Lyrics.NotFound
                  ? qsTr("As letras vêm do LRCLIB (lrclib.net), um acervo aberto mantido por voluntários.")
                  : root.status === Lyrics.Error ? qsTr("Confira a conexão e tente de novo.") : ""
        }
        Chip {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.status === Lyrics.Error
            text: qsTr("Tentar de novo")
            onClicked: App.lyrics.retry()
        }
    }
}
