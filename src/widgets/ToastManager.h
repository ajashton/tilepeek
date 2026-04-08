#pragma once

#include "widgets/ToastWidget.h"

#include <QList>
#include <QObject>

class ToastManager : public QObject {
    Q_OBJECT
public:
    explicit ToastManager(QWidget* parentWidget);

    void showError(const QString& message);
    void showWarning(const QString& message);
    void showInfo(const QString& message);
    void clearAll();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void addToast(ToastWidget::Level level, const QString& message);
    void repositionToasts();
    void onToastDismissed();

    QWidget* m_parentWidget;
    QList<ToastWidget*> m_toasts;
};
