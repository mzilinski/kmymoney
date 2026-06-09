/*
    SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import org.kde.quickcharts.controls as ChartsControls

// Experimental read-only Kirigami dashboard for the Home view (PoC). Loaded into a
// bare QQuickWidget; ScrollablePage was confirmed to render correctly there without
// an ApplicationWindow by the MR-B1 step-0 spike. Context properties provided by C++:
//   homeBridge     - HomeViewBridge (net-worth text, fileOpen, navigation, enter/skip schedule)
//   accountsModel  - DashboardAccountsModel (flat asset/liability accounts)
//   schedulesModel - SchedulesDueModel (overdue + upcoming scheduled payments)
Kirigami.ScrollablePage {
    id: root
    title: i18n("Home")

    ColumnLayout {
        // Full-content root: C++ (KHomeViewPrivate::printQml) resolves this by objectName
        // and grabToImage()s it for printing — its implicitHeight spans all sections, so
        // print captures the whole dashboard, not just the visible viewport.
        objectName: "homeContent"
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

        // ---- Net-worth header (text) ----
        // Gated by the classic "Assets & Liabilities" home-page item (ItemList code 8);
        // the classic net-worth graph (code 6) has no QML counterpart so it is not mapped.
        Kirigami.AbstractCard {
            visible: homeBridge.fileOpen && homeBridge.showAssetsLiabilities
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

        // ---- Net-worth pie (assets vs. liabilities, magnitude-based) ----
        Kirigami.AbstractCard {
            visible: homeBridge.fileOpen && homeBridge.showAssetsLiabilities
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { level: 3; text: i18n("Assets vs. Liabilities") }

                Charts.PieChart {
                    id: netWorthPie
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                    valueSources: Charts.ArraySource {
                        array: [homeBridge.assetsValue, homeBridge.liabilitiesValue]
                    }
                    nameSource: Charts.ArraySource {
                        array: [i18n("Assets"), i18n("Liabilities")]
                    }
                    // Theme-aware, accessible colours; green assets / red liabilities.
                    colorSource: Charts.ArraySource {
                        array: [Kirigami.Theme.positiveTextColor, Kirigami.Theme.negativeTextColor]
                    }
                }

                ChartsControls.Legend {
                    Layout.fillWidth: true
                    chart: netWorthPie
                    formatValue: (value) => homeBridge.formatValue(value)
                }
            }
        }

        // ---- Per-account allocation pie ("where is my money") ----
        // count is a real property (rowCount is a method); hide the card when nothing
        // has a non-zero magnitude to draw. QML-only section: it has no classic home-page
        // ItemList code, so it is always shown (subject only to its own count self-gate).
        Kirigami.AbstractCard {
            visible: homeBridge.fileOpen && accountAllocationModel.count > 0
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { level: 3; text: i18n("Account Allocation") }

                Charts.PieChart {
                    id: allocationPie
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                    valueSources: Charts.ModelSource {
                        model: accountAllocationModel
                        roleName: "allocationValue"
                    }
                    nameSource: Charts.ModelSource {
                        model: accountAllocationModel
                        roleName: "accountName"
                    }
                    // Theme-aware gradient of one colour per account; itemCount tracks the
                    // model so the palette grows/shrinks with the number of wedges.
                    colorSource: Charts.ColorGradientSource {
                        baseColor: Kirigami.Theme.highlightColor
                        itemCount: accountAllocationModel.count
                    }
                }

                ChartsControls.Legend {
                    Layout.fillWidth: true
                    chart: allocationPie
                    formatValue: (value) => homeBridge.formatValue(value)
                }
            }
        }

        // ---- Account cards (asset/liability, non-closed, deduplicated) ----
        // accountsModel is already filtered to real accounts, so a flat Repeater is correct.
        // Gated by the classic "Preferred/Payment Accounts" home-page items (codes 2|3).
        Kirigami.CardsLayout {
            visible: homeBridge.fileOpen && homeBridge.showAccounts
            Layout.fillWidth: true

            Repeater {
                model: accountsModel
                delegate: Kirigami.Card {
                    // The card navigates on click, so show hover/press affordance
                    // (showClickFeedback also enables hoverEnabled on AbstractCard).
                    showClickFeedback: true
                    // Name + balance live in the contentItem rather than banner.title:
                    // a bare-Label contentItem under a banner collapsed to zero height,
                    // hiding the balance. A ColumnLayout contentItem (as the other cards
                    // use) lays both out reliably. ?? guards the transient [undefined] a
                    // Repeater delegate sees while the model is still being populated.
                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Heading {
                            level: 4
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: model.accountName ?? ""
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: model.balanceText ?? ""
                            color: model.isNegative ? Kirigami.Theme.negativeTextColor
                                                    : Kirigami.Theme.textColor
                        }
                    }
                    onClicked: homeBridge.openAccountLedger(model.accountId ?? "")
                }
            }
        }

        // ---- Scheduled payments due (overdue + within one month) ----
        // schedulesModel.count is a real property (QAbstractItemModel::rowCount is a
        // method, not bindable), so the whole card hides when nothing is due.
        // Gated by the classic "Scheduled Payments" home-page item (ItemList code 1).
        Kirigami.AbstractCard {
            visible: homeBridge.fileOpen && schedulesModel.count > 0 && homeBridge.showScheduledPayments
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { level: 3; text: i18n("Scheduled Payments Due") }

                Repeater {
                    model: schedulesModel
                    // Each schedule is a small column: the main row plus, for a transfer,
                    // a second row for the counter account. Columns are only loosely
                    // aligned via fillWidth (a dashboard card, not a strict table).
                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        // ---- main account row ----
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: (model.scheduleName ?? "")
                                      + ((model.overdueCountText ?? "") !== "" ? " " + model.overdueCountText : "")
                                // overdue schedules flagged on the name; the amount carries its own sign colour
                                color: model.isOverdue ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                            }
                            QQC2.Label { text: model.accountName ?? ""; opacity: 0.7 }
                            QQC2.Label { text: model.dueDateText ?? "" }
                            QQC2.Label {
                                text: model.amountText ?? ""
                                color: model.isNegativeAmount ? Kirigami.Theme.negativeTextColor
                                                              : Kirigami.Theme.textColor
                            }
                            // projected balance after the payment, de-emphasised; the "→"
                            // reads "results in" and disambiguates it from the amount.
                            QQC2.Label {
                                text: (model.balanceAfterText ?? "") !== "" ? "→ " + model.balanceAfterText : ""
                                opacity: 0.7
                                color: model.isNegativeBalanceAfter ? Kirigami.Theme.negativeTextColor
                                                                    : Kirigami.Theme.textColor
                            }
                            QQC2.Button {
                                text: i18n("Enter")
                                icon.name: "go-next"
                                onClicked: homeBridge.enterSchedule(model.scheduleId ?? "")
                            }
                            QQC2.Button {
                                text: i18n("Skip")
                                icon.name: "media-skip-forward"
                                onClicked: homeBridge.skipSchedule(model.scheduleId ?? "")
                            }
                        }

                        // ---- counter account row (transfers only) ----
                        RowLayout {
                            visible: model.isTransfer ?? false
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: model.counterAccountName ?? ""
                                opacity: 0.7
                            }
                            QQC2.Label {
                                text: model.counterAmountText ?? ""
                                color: model.counterIsNegativeAmount ? Kirigami.Theme.negativeTextColor
                                                                     : Kirigami.Theme.textColor
                            }
                            QQC2.Label {
                                text: (model.counterBalanceAfterText ?? "") !== "" ? "→ " + model.counterBalanceAfterText : ""
                                opacity: 0.7
                                color: model.counterIsNegativeBalanceAfter ? Kirigami.Theme.negativeTextColor
                                                                           : Kirigami.Theme.textColor
                            }
                        }
                    }
                }
            }
        }
    }
}
