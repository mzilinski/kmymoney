/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Experimental read-only Kirigami dashboard for the Home view (PoC). Loaded into a
// bare QQuickWidget; ScrollablePage was confirmed to render correctly there without
// an ApplicationWindow by the MR-B1 step-0 spike. Context properties provided by C++:
//   homeBridge    - HomeViewBridge (net-worth text, fileOpen, navigation)
//   accountsModel - DashboardAccountsModel (flat asset/liability accounts)
Kirigami.ScrollablePage {
    id: root
    title: i18n("Home")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // ---- Welcome / empty state ----
        Kirigami.PlaceholderMessage {
            visible: !homeBridge.fileOpen
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            icon.name: "go-home"
            text: i18n("No file open")
            explanation: i18n("Open or create a file to see your financial overview.")
        }

        // ---- Net-worth header (text only; charts deferred to a later MR) ----
        Kirigami.AbstractCard {
            visible: homeBridge.fileOpen
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                Kirigami.Heading {
                    level: 2
                    text: i18n("Net Worth: %1", homeBridge.netWorthText)
                }
                QQC2.Label { text: i18n("Total Assets: %1", homeBridge.assetsText) }
                QQC2.Label { text: i18n("Total Liabilities: %1", homeBridge.liabilitiesText) }
            }
        }

        // ---- Account cards (asset/liability, non-closed, deduplicated) ----
        // accountsModel is already filtered to real accounts, so a flat Repeater is correct.
        Kirigami.CardsLayout {
            visible: homeBridge.fileOpen
            Layout.fillWidth: true

            Repeater {
                model: accountsModel
                delegate: Kirigami.Card {
                    // The card navigates on click, so show hover/press affordance
                    // (showClickFeedback also enables hoverEnabled on AbstractCard).
                    showClickFeedback: true
                    // ?? guards against the transient [undefined] a Repeater delegate sees
                    // while the model is still being populated (roles not yet mapped).
                    banner.title: model.accountName ?? ""
                    contentItem: QQC2.Label {
                        text: model.balanceText ?? ""
                        color: model.isNegative ? Kirigami.Theme.negativeTextColor
                                                : Kirigami.Theme.textColor
                    }
                    onClicked: homeBridge.openAccountLedger(model.accountId ?? "")
                }
            }
        }
    }
}
