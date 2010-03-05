/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtGui/QDesktopWidget>
#include <QDebug>
#include <QDataStream>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomAttr>
#include <QSettings>
#include <QCloseEvent>
#include "clipboardmodel.h"

inline bool readDatFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QDataStream in(&device);
    in >> map["items"];
    return true;
}

inline bool writeDatFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    QDataStream out(&device);
    out << map["items"];
    return true;
}

MainWindow::MainWindow(const QString &css, QWidget *parent)
: QMainWindow(parent), ui(new Ui::MainWindow)
{
    // global stylesheet
    setStyleSheet(css);

    ui->setupUi(this);
    ui->clipboardBrowser->readSettings(css);
    ui->clipboardBrowser->startMonitoring();

    // main window: icon & title
    this->setWindowTitle("CopyQ");
    m_icon = QIcon(":images/icon.svg");
    setWindowIcon(m_icon);

    // tray
    tray = new QSystemTrayIcon(this);
    tray->setIcon(m_icon);
    tray->setToolTip(
            tr("left click to show or hide, middle click to quit") );

    // signals & slots
    connect( ui->clipboardBrowser, SIGNAL(requestSearch(QEvent*)),
            this, SLOT(enterSearchMode(QEvent*)) );
    connect( ui->clipboardBrowser, SIGNAL(hideSearch()),
            this, SLOT(enterBrowseMode()) );
    connect( ui->clipboardBrowser, SIGNAL(error(const QString)),
            this, SLOT(showError(const QString)) );
    connect( tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)) );

    // settings
    readSettings();

    // browse mode by default
    m_browsemode = false;
    enterBrowseMode();

    tray->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#ifndef WIN32
    showMinimized();
#endif
    hide();
    event->ignore();
}

void MainWindow::showError(const QString msg)
{
    tray->showMessage(QString("Error"), msg);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if ( event->modifiers() == Qt::ControlModifier )
        if ( event->key() == Qt::Key_Q ) {
            close();
            QApplication::exit();
        }

    switch( event->key() ) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            ui->clipboardBrowser->keyEvent(event);
            break;
        
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ui->clipboardBrowser->moveToClipboard( ui->clipboardBrowser->currentIndex() );
            close();
            break;

        case Qt::Key_F3:
            break;

        case Qt::Key_Escape:
            if ( ui->searchBar->isVisible() )
                enterBrowseMode();
            else {
                close();
            }
            break;

        default:
            QMainWindow::keyPressEvent(event);
            break;
    }
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    ui->clipboardBrowser->writeSettings();
    ui->clipboardBrowser->saveItems();
}

void MainWindow::readSettings()
{
    QSettings settings;

    settings.beginGroup("MainWindow");
    resize(settings.value("size", QSize(400, 400)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();
}

void MainWindow::handleMessage(const QString& message)
{
    // TODO: local encoding
    QStringList args = message.split(QChar('\n'));
    const QString &cmd = args.at(0);
    ClipboardBrowser *c = ui->clipboardBrowser;

    // force check clipboard (update clipboard browser)
    c->checkClipboard();

    // show/hide main window
    if ( cmd == "toggle")
        toggleVisible();

    else if ( cmd == "action" ) {
        // show action dialog
        if ( args.length() == 1 )
            c->openActionDialog(0);
        // action [row] "cmd" "[sep]"
        else {
            QString arg, cmd, sep;

            args.pop_front();
            arg = args.takeFirst();

            // get row
            bool ok;
            int row = arg.toInt(&ok);
            if (ok) {
                if ( args.isEmpty() )
                    goto actionError;
                arg = args.takeFirst();
            }
            else
                row = 0;

            // get command
            cmd = arg;

            // get separator
            sep = args.isEmpty() ?
                  QString('\n') : args.takeFirst();

            if ( !args.isEmpty() )
                goto actionError;

            c->action(row, cmd, sep);
            return;

            actionError:
            showError("Bad \"action\" command syntax!\n"
                  "action [row] cmd [sep]");
        }
    }

    // add new item
    else if ( cmd == "add" )
        c->add(args);

    // edit clipboard item
    else if ( cmd == "edit" ) {
        c->setCurrent(0);
        c->openEditor();
    }

    // create new item and edit it
    else if ( cmd == "new" ) {
        c->add(args[1], false);
        c->setCurrent(0);
        c->openEditor();
    }

    // TODO: get item text
    // TODO: move item

    else
        showError("Unknown command");
}

void MainWindow::toggleVisible()
{
    if ( isVisible() )
        close();
    else {
        show();
        activateWindow();
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if ( reason == QSystemTrayIcon::MiddleClick )
        QApplication::exit();
    else
        toggleVisible();
}

void MainWindow::enterSearchMode(QEvent *event)
{
    enterBrowseMode(false);
    ui->searchBar->event(event);
    if ( ui->searchBar->text().isEmpty() )
        enterBrowseMode(true);
}

void MainWindow::enterBrowseMode(bool browsemode)
{
    QLineEdit *l = ui->searchBar;

    if (m_browsemode == browsemode) return;
    m_browsemode = browsemode;

    if(m_browsemode){
        // browse mode
        l->hide();
//        ui->clipboardBrowser->setFocus(Qt::ShortcutFocusReason);
    }
    else {
        // search mode
        l->show();
        l->setFocus(Qt::ShortcutFocusReason);
        l->selectAll();
    }
}

void MainWindow::center() {
    int x, y;
    int screenWidth, screenHeight;
    int width, height;
    QSize windowSize;

    QDesktopWidget *desktop = QApplication::desktop();

    width = frameGeometry().width();
    height = frameGeometry().height();

    screenWidth = desktop->width();
    screenHeight = desktop->height();

    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

    move( x, y );
}

MainWindow::~MainWindow()
{
    writeSettings();
    delete ui;
}

void MainWindow::on_searchBar_textEdited(const QString &)
{
    timer_search.start(100,this);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == timer_search.timerId() ) {
        ui->clipboardBrowser->filterItems( ui->searchBar->text() );
        timer_search.stop();
    }
    else
        QMainWindow::timerEvent(event);
}
