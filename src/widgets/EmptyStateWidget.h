#pragma once

#include <QPixmap>
#include <QWidget>

class EmptyStateWidget : public QWidget {
    Q_OBJECT
public:
    explicit EmptyStateWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_patternTile;
};
