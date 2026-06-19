/*
    SPDX-FileCopyrightText: 2013 Christian Dávid <christian-david@web.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KONLINEJOBOUTBOXVIEW_H
#define KONLINEJOBOUTBOXVIEW_H

// ----------------------------------------------------------------------------
// Qt Headers

// ----------------------------------------------------------------------------
// KDE Headers

class KXMLGUIClient;
class KXMLGUIFactory;

// ----------------------------------------------------------------------------
// Project Headers

#include "kmymoneyviewbase.h"
#include "onlinejob.h"

class QModelIndex;

namespace KMyMoneyPlugin {
class OnlinePlugin;
}

namespace eMenu {
enum class OnlineAction {
    LogOnlineJob,
    AccountCreditTransfer,
    DeleteOnlineJob,
    EditOnlineJob,
    SendOnlineJobs,
    RetrieveStandingOrders, //!< LH-F-21 T2: retrieve standing orders from the bank
    ModifyStandingOrder, //!< LH-F-21 T3: modify a retrieved standing order
    DeleteStandingOrder, //!< LH-F-21 T3: delete a retrieved standing order
};
inline qHashSeedType qHash(const OnlineAction key, qHashSeedType seed)
{
    return ::qHash(static_cast<uint>(key), seed);
}
};

class KOnlineJobOutboxViewPrivate;
class SelectedObjects;
class KOnlineJobOutboxView : public KMyMoneyViewBase
{
    Q_OBJECT

public:
    explicit KOnlineJobOutboxView(QWidget* parent = nullptr);
    ~KOnlineJobOutboxView() override;

    void executeAction(eMenu::Action action, const SelectedObjects& selections) override;

    void createActions(KXMLGUIFactory* guiFactory, KXMLGUIClient* guiClient);
    void removeActions();

    QStringList selectedOnlineJobs() const;

public Q_SLOTS:
    void updateActions(const SelectedObjects& selections) override;

    virtual void setOnlinePlugins(QMap<QString, KMyMoneyPlugin::OnlinePlugin*>* plugins) override;

Q_SIGNALS:
    void sendJobs(QList<onlineJob>);
    void editJob(QString);
    void newCreditTransfer();

protected:
    void showEvent(QShowEvent* event) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    Q_DECLARE_PRIVATE(KOnlineJobOutboxView)

private Q_SLOTS:
    void updateSelection();

    void slotRemoveJob();

    /** @brief If any job is selected, send it. Send all valid jobs otherwise. */
    void slotSendJobs();

    /** @brief Send all sendable online jobs */
    void slotSendAllSendableJobs();

    /** @brief Send only the selected jobs */
    void slotSendSelectedJobs();

    void slotEditJob();
    void slotEditJob(const QModelIndex&);

    /**
     * Saves the @a job to the engine and returns the id of the job
     */
    QString slotOnlineJobSave(onlineJob job);
    void slotOnlineJobSend(onlineJob job);
    void slotOnlineJobSend(QList<onlineJob> jobs);

    void slotOnlineJobLog();
    void slotOnlineJobLog(const QStringList& onlineJobIds);
    void slotNewCreditTransfer();

    /** @brief LH-F-21 T2: trigger a bank retrieval of standing orders (SimpleMode). */
    void slotRetrieveStandingOrders();
    /** @brief LH-F-21 T3: open a Modify order seeded from the selected retrieved row. */
    void slotModifyStandingOrder();
    /** @brief LH-F-21 T3: confirm + send a Delete for the selected retrieved row. */
    void slotDeleteStandingOrder();
};

#endif // KONLINEJOBOUTBOXVIEW_H
