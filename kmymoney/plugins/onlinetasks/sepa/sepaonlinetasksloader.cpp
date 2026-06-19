/*
    SPDX-FileCopyrightText: 2013-2015 Christian Dávid <christian-david@web.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sepaonlinetasksloader.h"

#include <KPluginFactory>

#include "mymoney/onlinejobadministration.h"
#include "onlinetasks/sepa/sepaonlinetransferimpl.h"
#include "onlinetasks/sepa/sepastandingorderimpl.h"
#include "ui/sepacredittransferedit.h"

// NOTE: the standing-order EDITOR lives in its own plugin
// (konlinetasks_sepa_standingorder); a single KPluginFactory may register only
// one IonlineJobEdit, since the transfer form instantiates editors via
// create<IonlineJobEdit>() which cannot disambiguate two of them. The
// standing-order TASK is still created here (createOnlineTask below).
K_PLUGIN_FACTORY_WITH_JSON(konlinetasks_sepa_factory, "kmymoney-sepaorders.json", registerPlugin<sepaOnlineTasksLoader>();
                           registerPlugin<sepaCreditTransferEdit>();)

sepaOnlineTasksLoader::sepaOnlineTasksLoader(QObject* parent, const QVariantList& options)
    : onlineTaskFactory(parent, options)
{
}

onlineTask* sepaOnlineTasksLoader::createOnlineTask(const QString& taskId) const
{
    if (taskId == sepaOnlineTransferImpl::name())
        return new sepaOnlineTransferImpl;

    if (taskId == sepaStandingOrderImpl::name())
        return new sepaStandingOrderImpl;

    return nullptr;
}

// Needed for K_PLUGIN_FACTORY
#include "sepaonlinetasksloader.moc"
