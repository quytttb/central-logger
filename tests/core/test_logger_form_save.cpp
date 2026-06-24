// Regression and contract tests for LoggerFormController::saveLoggerFromForm.
//
// The production flow triggered by the UI "Save" button:
//   1. Requires a prior probe (GET /config) to set m_probedRevision.
//   2. Inserts / updates the logger row in a DB transaction.
//   3. If the config changed, POSTs the patch to the edge device.
//   4. Commits the transaction regardless of the REST outcome
//      (configApplyFailed is emitted as a non-blocking warning when REST fails).
//
// FakeEdgeServer provides a minimal QTcpServer that speaks just enough
// HTTP/1.1 to satisfy QNetworkAccessManager — no third-party dep required.

#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "network/rest/RestConfigService.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;
using namespace CentralLogger::Network;

namespace {

// ---------------------------------------------------------------------------
// Minimal HTTP/1.1 edge stub
// ---------------------------------------------------------------------------

class FakeEdgeServer : public QObject
{
    Q_OBJECT

public:
    enum class PostBehavior { Succeed, CloseImmediately };

    explicit FakeEdgeServer(QObject *parent = nullptr) : QObject(parent)
    {
        m_server.listen(QHostAddress::LocalHost, 0 /* random free port */);
        connect(&m_server, &QTcpServer::newConnection,
                this,      &FakeEdgeServer::onNewConnection);
    }

    bool    isListening()  const { return m_server.isListening(); }
    quint16 port()         const { return m_server.serverPort(); }
    int     postCount()    const { return m_postCount; }

    void setPostBehavior(PostBehavior b) { m_postBehavior = b; }

private slots:
    void onNewConnection()
    {
        QTcpSocket *sock = m_server.nextPendingConnection();
        sock->setParent(this);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray req = sock->readAll();
            if (req.startsWith("POST")) {
                ++m_postCount;
                if (m_postBehavior == PostBehavior::CloseImmediately) {
                    sock->close();
                    return;
                }
                static const QByteArray body =
                    R"({"ok":true,"applied_revision":2})";
                httpReply(sock, 200, body);
            } else {
                // GET /api/v1/config
                static const QByteArray body =
                    R"({"revision":1,)"
                    R"("config":{)"
                    R"("station_code":"TST-01",)"
                    R"("station_name":"EdgeName",)"
                    R"("poll_interval":2,)"
                    R"("modbus_tcp_unit_id":1},)"
                    R"("sensors":[]})";
                httpReply(sock, 200, body);
            }
        });
    }

private:
    static void httpReply(QTcpSocket *sock, int code, const QByteArray &body)
    {
        const QByteArray status = (code == 200) ? "200 OK" : "500 Server Error";
        QByteArray resp;
        resp += "HTTP/1.1 " + status + "\r\n";
        resp += "Content-Type: application/json\r\n";
        resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        resp += "Connection: close\r\n\r\n";
        resp += body;
        sock->write(resp);
        sock->flush();
        sock->disconnectFromHost();
    }

    QTcpServer   m_server;
    PostBehavior m_postBehavior = PostBehavior::Succeed;
    int          m_postCount    = 0;
};

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------

QString uniqueConn(const char *tag)
{
    static int s = 0;
    return QStringLiteral("save_%1_%2").arg(tag).arg(++s);
}

struct Harness {
    Database            db;
    DashboardController dash{nullptr};
    RestConfigService   rest;
    LoggerFormController form{nullptr};
    FakeEdgeServer      edge;

    explicit Harness(const char *tag)
    {
        const bool opened = db.open(uniqueConn(tag), QStringLiteral(":memory:"));
        if (!opened)
            qFatal("Harness: Database::open() failed (conn=%s)",
                   qPrintable(uniqueConn(tag)));
        dash.setDatabase(&db);
        rest.setDatabase(&db);
        form.setDatabase(&db);
        form.setRestConfigService(&rest);
        form.setDashboardController(&dash);
        dash.reloadLoggers();
    }

    // Simulate the UI "Connect" button. Spins the event loop until
    // probeConfigResult fires (or times out). Returns probe success.
    bool probe(int timeoutMs = 5000)
    {
        bool ok = false;
        bool got = false;
        const QMetaObject::Connection c = QObject::connect(
            &form, &LoggerFormController::probeConfigResult,
            [&ok, &got](bool success, const QString &) {
                ok  = success;
                got = true;
            });

        form.probeConfig(QStringLiteral("127.0.0.1"),
                         edge.port(),
                         QStringLiteral("test-token"));

        // Give the event loop time to process the network round-trip.
        const int step = 50;
        for (int waited = 0; !got && waited < timeoutMs; waited += step)
            QTest::qWait(step);

        QObject::disconnect(c);
        return ok;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestLoggerFormSave : public QObject
{
    Q_OBJECT

private slots:
    void requiresProbeBeforeSave();
    void successPath_dbCommitted();
    void restFail_dbStillCommitted();
    void noRestCall_whenConfigUnchanged();
};

// ---- No probe → save must fail, DB empty ---------------------------------

void TestLoggerFormSave::requiresProbeBeforeSave()
{
    Harness h("noprobe");
    QVERIFY(h.edge.isListening());

    bool saveOk = true;
    bool gotSignal = false;
    QObject::connect(&h.form, &LoggerFormController::formSaveFinished,
                     [&](bool ok, qint64, const QString &) {
                         saveOk = ok;
                         gotSignal = true;
                     });

    // No probe → m_probedRevision == -1 → saveLoggerFromForm returns early.
    h.form.saveLoggerFromForm(true, -1,
                              QStringLiteral("My Logger"),
                              QStringLiteral("127.0.0.1"),
                              5020, h.edge.port(),
                              QStringLiteral("tok"),
                              1, 2, 5);

    QVERIFY2(gotSignal, "formSaveFinished not emitted");
    QVERIFY2(!saveOk, "save should fail without prior probe");

    LoggerRepository repo(h.db.connection());
    QVERIFY(repo.findAll().isEmpty());
}

// ---- Happy path: probe ok + REST succeeds → DB committed -----------------

void TestLoggerFormSave::successPath_dbCommitted()
{
    Harness h("success");
    QVERIFY(h.edge.isListening());
    h.edge.setPostBehavior(FakeEdgeServer::PostBehavior::Succeed);
    QVERIFY2(h.probe(), "Probe failed — fake server may not have started");

    bool saveOk  = false;
    bool restFailed = false;
    QString saveErr;
    QObject::connect(&h.form, &LoggerFormController::formSaveFinished,
                     [&](bool ok, qint64, const QString &msg) { saveOk = ok; saveErr = msg; });
    QObject::connect(&h.form, &LoggerFormController::configApplyFailed,
                     [&](qint64, const QString &) { restFailed = true; });

    QVERIFY2(h.db.isOpen(), "Harness DB must be open before save");

    // "New Name" ≠ "EdgeName" from probe → a POST patch is required.
    h.form.saveLoggerFromForm(true, -1,
                              QStringLiteral("New Name"),
                              QStringLiteral("127.0.0.1"),
                              5020, h.edge.port(),
                              QStringLiteral("tok"),
                              1, 2, 5);

    QVERIFY2(saveOk, qPrintable(QStringLiteral("DB commit failed: %1").arg(saveErr)));
    QVERIFY2(!restFailed, "configApplyFailed must NOT fire on REST success");

    LoggerRepository repo(h.db.connection());
    const auto all = repo.findAll();
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().name, QStringLiteral("New Name"));

    QCOMPARE(h.edge.postCount(), 1);
}

// ---- Regression: REST POST fails → DB MUST still be committed -----------
//
// Previously the code did conn.rollback() on REST failure, so the logger
// was silently lost from the DB. This test pins the corrected behaviour:
// the local record is always committed; configApplyFailed warns the UI.

void TestLoggerFormSave::restFail_dbStillCommitted()
{
    Harness h("restfail");
    QVERIFY(h.edge.isListening());
    h.edge.setPostBehavior(FakeEdgeServer::PostBehavior::CloseImmediately);
    QVERIFY2(h.probe(), "Probe failed");

    bool saveOk  = false;
    bool restFailed = false;
    QString restErr;
    QObject::connect(&h.form, &LoggerFormController::formSaveFinished,
                     [&](bool ok, qint64, const QString &) { saveOk = ok; });
    QObject::connect(&h.form, &LoggerFormController::configApplyFailed,
                     [&](qint64, const QString &msg) {
                         restFailed = true;
                         restErr = msg;
                     });

    h.form.saveLoggerFromForm(true, -1,
                              QStringLiteral("New Name"),
                              QStringLiteral("127.0.0.1"),
                              5020, h.edge.port(),
                              QStringLiteral("tok"),
                              1, 2, 5);

    QVERIFY2(saveOk,
             "DB must be committed even when REST POST fails (regression test)");

    QVERIFY2(restFailed,
             "configApplyFailed must be emitted so the UI can warn the user");
    QVERIFY2(!restErr.isEmpty(),
             "configApplyFailed error message must not be empty");

    LoggerRepository repo(h.db.connection());
    QCOMPARE(repo.findAll().size(), 1);
}

// ---- No POST when name + poll_interval unchanged after probe -------------

void TestLoggerFormSave::noRestCall_whenConfigUnchanged()
{
    Harness h("nopatch");
    QVERIFY(h.edge.isListening());
    h.edge.setPostBehavior(FakeEdgeServer::PostBehavior::Succeed);
    QVERIFY2(h.probe(), "Probe failed");

    bool saveOk = false;
    QObject::connect(&h.form, &LoggerFormController::formSaveFinished,
                     [&](bool ok, qint64, const QString &) { saveOk = ok; });

    // "EdgeName" and poll=2 match what the fake server returned → empty patch
    // → saveLoggerFromForm skips the REST call entirely.
    h.form.saveLoggerFromForm(true, -1,
                              QStringLiteral("EdgeName"),
                              QStringLiteral("127.0.0.1"),
                              5020, h.edge.port(),
                              QStringLiteral("tok"),
                              1, 2, 5);

    QVERIFY2(saveOk, "Save should succeed with no REST call needed");
    QCOMPARE(h.edge.postCount(), 0);
}

QTEST_MAIN(TestLoggerFormSave)
#include "test_logger_form_save.moc"
