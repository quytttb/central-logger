#pragma once

#include <QObject>
#include <QList>

class QProcess;

namespace CentralLogger::Core {

// Owns only processes launched by this application instance. An absent config
// is normal; an invalid config never starts a partial set of adapters.
class AdapterProcessManager final : public QObject
{
    Q_OBJECT
public:
    explicit AdapterProcessManager(QObject *parent = nullptr);
    ~AdapterProcessManager() override;
    bool startFromFile(const QString &path, const QString &defaultProgram, QString *error = nullptr);
    void stop();
    int processCount() const { return m_processes.size(); }

private:
    QList<QProcess *> m_processes;
    bool m_stopping = false;
};

} // namespace CentralLogger::Core
