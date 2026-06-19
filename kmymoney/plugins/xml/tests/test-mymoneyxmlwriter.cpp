/*
    SPDX-FileCopyrightText: 2024 Ralf Habacker <ralf.habacker@freenet.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test-mymoneyxmlwriter.h"

#include "mymoneypayee.h"
#include "mymoneysecurity.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QBuffer>
#include <QRegularExpression>
#include <QTest>

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// std Includes

#include <iostream>

// ----------------------------------------------------------------------------
// Project Includes

#include "../mymoneyxmlwriter.h"
#include "mymoneyfile.h"
#include "mymoneyutils.h"
#include "onlinetasks/sepa/sepaonlinetransferimpl.h"
#include "onlinetasks/sepa/sepastandingorderimpl.h"
#include "payeeidentifier/ibanbic/ibanbic.h"

#include <QXmlStreamWriter>

#define KMMCOMPARE(actual, expected, _file, _line)                                                                                                             \
    do {                                                                                                                                                       \
        if (!QTest::qCompare(actual, expected, #actual, #expected, _file, _line)) {                                                                            \
            compareContent(actual, expected);                                                                                                                  \
            return;                                                                                                                                            \
        }                                                                                                                                                      \
    } while (false)

QTEST_GUILESS_MAIN(MyMoneyXmlWriterTest)

void MyMoneyXmlWriterTest::init()
{
    m_file = MyMoneyFile::instance();
    m_file->unload();
}

bool compareContent(const QByteArray& data1, const QByteArray& data2)
{
    QRegularExpression exp("\r?\n");
    QStringList a = QString(data1).split(exp);
    QStringList b = QString(data2).split(exp);
    int max = std::min(a.size(), b.size());
    bool result = a.size() == b.size();
    for (int i = 0; i < max; i++) {
        if (a.at(i) != b.at(i)) {
            result = false;
            std::cerr << "- " << a.at(i).toStdString() << std::endl;
            std::cerr << "+ " << b.at(i).toStdString() << std::endl;
        }
    }
    return result;
}

QByteArray replaceContent(const QByteArray& data, const QString& key, const QString& newContent)
{
    QStringList result;
    for (const auto& line : QString(data).split('\n')) {
        if (!line.contains(key))
            result.append(line);
        else if (line.contains("value")) {
            QRegularExpression exp("value=\".*\"");
            result.append(QString(line).replace(exp, QString("value=\"%1\"").arg(newContent)));
        } else if (line.contains("date=")) {
            QRegularExpression exp("date=\".*\"");
            result.append(QString(line).replace(exp, QString("date=\"%1\"").arg(newContent)));
        }
    }
    return result.join("\n").toLocal8Bit();
}

void _writeAndCompare(MyMoneyFile* file, const QString& filename, const char* _file, int _line)
{
    // read reference file
    const QString srcFile = QLatin1String(CMAKE_CURRENT_SOURCE_DIR) + "/" + filename;
    QString dstFile = QLatin1String(CMAKE_CURRENT_BINARY_DIR) + "/" + filename;

    QFile refFile(srcFile);
    QVERIFY(refFile.open(QIODevice::ReadOnly));
    QByteArray refData = refFile.readAll();

    MyMoneyXmlWriter writer;
    writer.setFile(file);
    QByteArray data;
    QBuffer buffer;
    buffer.setBuffer(&data);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(writer.write(&buffer));
    buffer.close();
#if defined(Q_OS_WIN32)
    data.replace("\r\n", "\n");
    refData.replace("\r\n", "\n");
#endif
    QFile outFile(dstFile);
    QVERIFY(outFile.open(QIODevice::WriteOnly));
    outFile.write(data);
    outFile.close();

    // patch lines containing dynamic data
    QStringList keys = QStringList() << "LastModificationDate"
                                     << "LAST_MODIFIED_DATE"
                                     << "APPVERSION"
                                     << "kmm-id";
    QByteArray data1 = data;
    QByteArray refData1 = refData;
    for (const auto& key : keys) {
        data1 = replaceContent(data1, key, QLatin1String("*"));
        refData1 = replaceContent(refData1, key, QLatin1String("*"));
    }

    KMMCOMPARE(refData1, data1, _file, _line);
}

#define writeAndCompare(a, b) _writeAndCompare(a, b, __FILE__, __LINE__)

void MyMoneyXmlWriterTest::testWriteFileInfo()
{
    MyMoneyFileTransaction ft;

    // store the user info
    m_file->setUser(MyMoneyPayee());

    // create and setup base currency
    m_file->addCurrency(MyMoneySecurity("EUR"));
    m_file->setBaseCurrency(MyMoneySecurity("EUR"));

    ft.commit();

    writeAndCompare(m_file, QLatin1String("testfile1.xml"));

    MyMoneyFileTransaction ft1;
    m_file->setFileFixVersion(1);
    ft1.commit();

    writeAndCompare(m_file, QLatin1String("testfile2.xml"));
}

// Helper: serialize a sepa task's <onlineTask> element to a string.
static QString writeSepaTaskToXml(const sepaOnlineTransferImpl& task)
{
    QString out;
    QXmlStreamWriter writer(&out);
    writer.writeStartElement(QStringLiteral("onlineTask"));
    task.writeXML(&writer);
    writer.writeEndElement();
    return out;
}

void MyMoneyXmlWriterTest::testWriteSepaTransferType()
{
    // LH-F-20 byte gate: a default (Standard) task must NOT emit the new
    // transferType/executionDate attributes, so files that never used the
    // feature stay byte-identical to ones written by an older version.
    {
        sepaOnlineTransferImpl task;
        const QString xml = writeSepaTaskToXml(task);
        QVERIFY(!xml.contains(QLatin1String("transferType")));
        QVERIFY(!xml.contains(QLatin1String("executionDate")));
    }

    // An instant task carries only transferType (no date).
    {
        sepaOnlineTransferImpl task;
        task.setTransferType(sepaOnlineTransfer::TransferType::Instant);
        const QString xml = writeSepaTaskToXml(task);
        QVERIFY(xml.contains(QLatin1String("transferType=\"1\"")));
        QVERIFY(!xml.contains(QLatin1String("executionDate")));
    }

    // A dated task carries both attributes.
    {
        sepaOnlineTransferImpl task;
        task.setTransferType(sepaOnlineTransfer::TransferType::Dated);
        task.setExecutionDate(QDate(2026, 7, 1));
        const QString xml = writeSepaTaskToXml(task);
        QVERIFY(xml.contains(QLatin1String("transferType=\"2\"")));
        QVERIFY(xml.contains(QLatin1String("executionDate=\"2026-07-01\"")));

        // The copy constructor (used by clone()) must preserve both fields, else
        // clone() would silently lose them.
        const sepaOnlineTransferImpl copy(task);
        QVERIFY(copy.transferType() == sepaOnlineTransfer::TransferType::Dated);
        QCOMPARE(copy.executionDate(), QDate(2026, 7, 1));

        // isValid() rejects a dated transfer without an execution date.
        sepaOnlineTransferImpl noDate;
        noDate.setTransferType(sepaOnlineTransfer::TransferType::Dated);
        QVERIFY(!noDate.isValid());
    }
}

static QString writeStandingOrderToXml(const sepaStandingOrderImpl& task)
{
    QString out;
    QXmlStreamWriter writer(&out);
    writer.writeStartElement(QStringLiteral("onlineTask"));
    task.writeXML(&writer);
    writer.writeEndElement();
    return out;
}

void MyMoneyXmlWriterTest::testWriteSepaStandingOrder()
{
    // LH-F-21: a monthly create order writes its recurrence block, omits the
    // action attribute (Create is the default), and omits the optional
    // last/next/bankOrderId fields.
    {
        sepaStandingOrderImpl task;
        task.setPeriod(sepaStandingOrder::Period::Monthly);
        task.setCycle(1);
        task.setExecutionDay(1);
        task.setFirstExecutionDate(QDate(2026, 8, 1));
        const QString xml = writeStandingOrderToXml(task);
        QVERIFY(xml.contains(QLatin1String("period=\"0\"")));
        QVERIFY(xml.contains(QLatin1String("cycle=\"1\"")));
        QVERIFY(xml.contains(QLatin1String("executionDay=\"1\"")));
        QVERIFY(xml.contains(QLatin1String("firstExecutionDate=\"2026-08-01\"")));
        QVERIFY(!xml.contains(QLatin1String("action=")));
        QVERIFY(!xml.contains(QLatin1String("lastExecutionDate")));
        QVERIFY(!xml.contains(QLatin1String("bankOrderId")));
    }

    // A modify order writes action + bankOrderId + nextExecutionDate.
    {
        sepaStandingOrderImpl task;
        task.setAction(sepaStandingOrder::Action::Modify);
        task.setExecutionDay(15);
        task.setFirstExecutionDate(QDate(2026, 8, 1));
        task.setBankOrderId(QStringLiteral("ORDER-42"));
        task.setNextExecutionDate(QDate(2026, 9, 1));
        const QString xml = writeStandingOrderToXml(task);
        QVERIFY(xml.contains(QLatin1String("action=\"1\"")));
        QVERIFY(xml.contains(QLatin1String("bankOrderId=\"ORDER-42\"")));
        QVERIFY(xml.contains(QLatin1String("nextExecutionDate=\"2026-09-01\"")));

        // The copy constructor (used by clone()) preserves every field.
        const sepaStandingOrderImpl copy(task);
        QVERIFY(copy.action() == sepaStandingOrder::Action::Modify);
        QCOMPARE(copy.executionDay(), 15);
        QCOMPARE(copy.bankOrderId(), QStringLiteral("ORDER-42"));
        QCOMPARE(copy.nextExecutionDate(), QDate(2026, 9, 1));
    }

    // isValid() — deterministic schedule + payment checks.
    {
        payeeIdentifiers::ibanBic beneficiary;
        beneficiary.setIban(QStringLiteral("DE89370400440532013000")); // checksum-valid test IBAN

        sepaStandingOrderImpl task;
        task.setBeneficiary(beneficiary);
        task.setValue(MyMoneyMoney(50000, 100));
        task.setPeriod(sepaStandingOrder::Period::Monthly);
        task.setCycle(1);
        // No executionDay / firstExecutionDate yet -> invalid.
        QVERIFY(!task.isValid());

        task.setExecutionDay(1);
        task.setFirstExecutionDate(QDate(2026, 8, 1));
        QVERIFY(task.isValid());

        // Switching to Modify without a bank order id -> invalid again.
        task.setAction(sepaStandingOrder::Action::Modify);
        QVERIFY(!task.isValid());
        task.setBankOrderId(QStringLiteral("ORDER-42"));
        task.setNextExecutionDate(QDate(2026, 9, 1));
        QVERIFY(task.isValid());
    }
}
