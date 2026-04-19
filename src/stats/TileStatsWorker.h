// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

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
    void calculateMBTiles(TileStatistics& result);
    void calculatePMTiles(TileStatistics& result);

    QString m_filePath;
};
