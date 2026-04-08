#pragma once

#include "model/TileStatistics.h"

#include <QObject>
#include <QString>

class TileStatsWorker : public QObject {
    Q_OBJECT
public:
    explicit TileStatsWorker(const QString& filePath, QObject* parent = nullptr);

public slots:
    void calculate();

signals:
    void finished(TileStatistics stats);

private:
    QString m_filePath;
};
