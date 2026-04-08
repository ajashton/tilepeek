#pragma once

#include <QFrame>

class QLabel;

class ToastWidget : public QFrame {
    Q_OBJECT
public:
    enum class Level { Info, Warning, Error };

    ToastWidget(Level level, const QString& message, QWidget* parent = nullptr);

    Level level() const { return m_level; }

signals:
    void dismissed();

private:
    Level m_level;
};
