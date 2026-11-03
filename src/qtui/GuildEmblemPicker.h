#pragma once
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <functional>
#include <memory>

namespace ro::guild {
struct EmblemSelection { QString path; bool failed=false; };

// Only the native picker runs on the worker. Deliver its result on the context's
// thread, so game state, sockets and Qt Quick are always accessed by the game loop.
inline bool PickEmblemAsync(QObject* context, std::function<EmblemSelection()> pick,
                            std::function<void(const EmblemSelection&)> complete)
{
    static QPointer<QThread> active;
    if (!context || active) return false;
    auto result=std::make_shared<EmblemSelection>();
    auto* worker=QThread::create([result,pick=std::move(pick)] { *result=pick(); });
    active=worker;
    QObject::connect(worker,&QThread::finished,context,[result,complete=std::move(complete)] {
        active.clear();
        complete(*result);
    },Qt::QueuedConnection);
    QObject::connect(worker,&QThread::finished,worker,&QObject::deleteLater);
    worker->start();
    return true;
}
}
