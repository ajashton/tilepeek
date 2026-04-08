#include "widgets/ToastManager.h"

#include <QEvent>

static constexpr int ToastMargin = 12;
static constexpr int ToastSpacing = 8;

ToastManager::ToastManager(QWidget* parentWidget)
    : QObject(parentWidget)
    , m_parentWidget(parentWidget)
{
    m_parentWidget->installEventFilter(this);
}

void ToastManager::showError(const QString& message)
{
    addToast(ToastWidget::Level::Error, message);
}

void ToastManager::showWarning(const QString& message)
{
    addToast(ToastWidget::Level::Warning, message);
}

void ToastManager::showInfo(const QString& message)
{
    addToast(ToastWidget::Level::Info, message);
}

void ToastManager::clearAll()
{
    for (auto* toast : m_toasts)
        toast->deleteLater();
    m_toasts.clear();
}

void ToastManager::addToast(ToastWidget::Level level, const QString& message)
{
    auto* toast = new ToastWidget(level, message, m_parentWidget);
    connect(toast, &ToastWidget::dismissed, this, &ToastManager::onToastDismissed);
    m_toasts.append(toast);
    toast->show();
    repositionToasts();
}

void ToastManager::onToastDismissed()
{
    auto* toast = qobject_cast<ToastWidget*>(sender());
    if (toast) {
        m_toasts.removeOne(toast);
        toast->deleteLater();
        repositionToasts();
    }
}

void ToastManager::repositionToasts()
{
    int x = m_parentWidget->width() - 360 - ToastMargin;
    int y = ToastMargin;

    for (auto* toast : m_toasts) {
        toast->move(x, y);
        toast->adjustSize();
        y += toast->height() + ToastSpacing;
    }
}

bool ToastManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_parentWidget && event->type() == QEvent::Resize)
        repositionToasts();
    return false;
}
