/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/notification.h"

#include "common/appconfig.h"
#include "common/common.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <memory>

Notification::Notification(int id)
    : m_id(id)
    , m_body(nullptr)
    , m_titleLabel(nullptr)
    , m_iconLabel(nullptr)
    , m_msgLabel(nullptr)
    , m_opacity(1.0)
    , m_icon(0)
{
    auto bodyLayout = new QVBoxLayout(this);
    bodyLayout->setMargin(8);
    m_body = new QWidget(this);
    bodyLayout->addWidget(m_body);
    bodyLayout->setSizeConstraint(QLayout::SetMaximumSize);

    auto layout = new QGridLayout(m_body);
    layout->setMargin(0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("NotificationTitle");
    layout->addWidget(m_titleLabel, 0, 0, 1, 2, Qt::AlignCenter);
    m_titleLabel->setTextFormat(Qt::PlainText);
    setTitle( QString() );

    m_iconLabel = new QLabel(this);
    layout->addWidget(m_iconLabel, 1, 0, Qt::AlignTop);

    m_msgLabel = new QLabel(this);
    layout->addWidget(m_msgLabel, 1, 1, Qt::AlignAbsolute);

    setWindowFlags(Qt::ToolTip);
    setWindowOpacity(m_opacity);
}

void Notification::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
    if (title.isEmpty()) {
        m_titleLabel->hide();
    } else {
        m_titleLabel->show();
        m_titleLabel->adjustSize();
    }
}

void Notification::setMessage(const QString &msg, Qt::TextFormat format)
{
    m_msgLabel->setTextFormat(format);
    m_msgLabel->setText(msg);
}

void Notification::setPixmap(const QPixmap &pixmap)
{
    m_msgLabel->setPixmap(pixmap);
}

void Notification::setIcon(ushort icon)
{
    m_icon = icon;
}

void Notification::setInterval(int msec)
{
    if (msec >= 0) {
        initSingleShotTimer( &m_timer, msec, this, SLOT(onTimeout()) );
        m_timer.start();
    } else {
        m_timer.stop();
    }
}

void Notification::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(m_opacity);
}

void Notification::updateIcon()
{
    const QColor color = getDefaultIconColor(*this);
    const auto height = static_cast<int>( m_msgLabel->fontMetrics().lineSpacing() * 1.2 );
    const QPixmap pixmap = createPixmap(m_icon, color, height);
    m_iconLabel->setPixmap(pixmap);
    m_iconLabel->resize(pixmap.size());
}

void Notification::adjust()
{
    m_body->adjustSize();
    adjustSize();
}

void Notification::mousePressEvent(QMouseEvent *)
{
    m_timer.stop();

    emit closeNotification(this);
}

void Notification::enterEvent(QEvent *event)
{
    setWindowOpacity(1.0);
    m_timer.stop();
    QWidget::enterEvent(event);
}

void Notification::leaveEvent(QEvent *event)
{
    setWindowOpacity(m_opacity);
    if ( m_timer.interval() > 0 )
        m_timer.start();
    QWidget::leaveEvent(event);
}

void Notification::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter p(this);

    // black outer border
    p.setPen(Qt::black);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // light inner border
    p.setPen( palette().color(QPalette::Window).lighter(300) );
    p.drawRect(rect().adjusted(1, 1, -2, -2));
}

void Notification::showEvent(QShowEvent *event)
{
    // QTBUG-33078: Window opacity must be set after show event.
    setWindowOpacity(m_opacity);
    QWidget::showEvent(event);
}

void Notification::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit closeNotification(this);
}

void Notification::onTimeout()
{
    emit closeNotification(this);
}
