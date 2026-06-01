#include "MainWindow.hpp"
#include "DashboardWidget.hpp"
#include "RewardEditorWidget.hpp"
#include "StatisticsWidget.hpp"
#include "SettingsWidget.hpp"
#include "../core/Application.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QDesktopServices>
#include <QUrl>
#include <QPushButton>
#include <QApplication>
#include <QRandomGenerator>
#include <QPixmap>
#include <QByteArray>
#include <QDialog>
#include <QMessageBox>
#include <QTextBrowser>
#include <QLabel>

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "Unknown"
#endif

#ifndef BUILD_IS_CUSTOMIZED
#define BUILD_IS_CUSTOMIZED 0
#endif

MainWindow::MainWindow(Application* app, QWidget* parent)
    : QMainWindow(parent)
    , m_app(app)
{
    if (BUILD_IS_CUSTOMIZED) {
        setWindowTitle(QString("Twitch Channel Point Manager - %1 (Custom Build)").arg(APP_VERSION_STRING));
        statusBar()->showMessage("© BLUE000 (Original Creator)");
        statusBar()->setStyleSheet("color: #888888; background-color: #121214;");
    } else {
        setWindowTitle(QString("Twitch Channel Point Manager - %1").arg(APP_VERSION_STRING));
    }
    
    resize(850, 600);

    setupUi();
    setupConnections();

    // 全体のイベント検知用にアプリケーションインスタンスへイベントフィルタを設定
    qApp->installEventFilter(this);

    // 5分間の無操作タイマーをセットアップ
    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(5 * 60 * 1000); // 5分 = 300,000ミリ秒
    connect(m_idleTimer, &QTimer::timeout, this, &MainWindow::onIdleTimeout);
    m_idleTimer->start();
}

void MainWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    m_dashboardWidget = new DashboardWidget(m_app, m_tabWidget);
    m_rewardEditorWidget = new RewardEditorWidget(m_app, m_tabWidget);
    m_statisticsWidget = new StatisticsWidget(m_app, m_tabWidget);
    m_settingsWidget = new SettingsWidget(m_app, m_tabWidget);

    m_tabWidget->addTab(m_dashboardWidget, tr("🏠 ダッシュボード"));
    m_tabWidget->addTab(m_rewardEditorWidget, tr("🎁 報酬演出管理"));
    m_tabWidget->addTab(m_statisticsWidget, tr("📊 統計ランキング"));
    m_tabWidget->addTab(m_settingsWidget, tr("⚙️ システム設定"));

    // 右上の操作ボタンコンテナ作成 (ABOUT & HELP)
    QWidget* cornerContainer = new QWidget(m_tabWidget);
    QHBoxLayout* cornerLayout = new QHBoxLayout(cornerContainer);
    cornerLayout->setContentsMargins(0, 0, 6, 0);
    cornerLayout->setSpacing(6);

    m_aboutButton = new QPushButton(tr("ℹ️ ABOUT"), cornerContainer);
    m_aboutButton->setCursor(Qt::PointingHandCursor);
    m_aboutButton->setToolTip(tr("アプリケーション情報とライセンス表記を表示します"));

    m_helpButton = new QPushButton(tr("❓ HELP"), cornerContainer);
    m_helpButton->setCursor(Qt::PointingHandCursor);
    m_helpButton->setToolTip(tr("GitHubのオンラインマニュアル（README.md）を開きます"));

    QString cornerButtonStyle = R"(
        QPushButton {
            border: 1px solid #35353B;
            border-radius: 4px;
            padding: 4px 12px;
            color: #E1E1E6;
            background-color: #1D1D22;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #29292E;
            color: #FFFFFF;
            border-color: #4A4A52;
        }
        QPushButton:pressed {
            background-color: #121214;
        }
    )";
    m_aboutButton->setStyleSheet(cornerButtonStyle);
    m_helpButton->setStyleSheet(cornerButtonStyle);

    cornerLayout->addWidget(m_aboutButton);
    cornerLayout->addWidget(m_helpButton);
    cornerContainer->setLayout(cornerLayout);

    m_tabWidget->setCornerWidget(cornerContainer, Qt::TopRightCorner);

    // ABOUTダイアログの接続
    connect(m_aboutButton, &QPushButton::clicked, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("About Twitch Channel Point Manager"));
        dialog.resize(550, 420);
        dialog.setStyleSheet("QDialog { background-color: #1D1D22; }");

        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(15, 15, 15, 15);
        layout->setSpacing(12);

        QLabel* titleLabel = new QLabel(QString(tr("🦊 Twitch Channel Point Manager - %1")).arg(APP_VERSION_STRING), &dialog);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 15px; color: #FFFFFF;");
        layout->addWidget(titleLabel);

        QTextBrowser* textBrowser = new QTextBrowser(&dialog);
        textBrowser->setOpenExternalLinks(true);
        textBrowser->setStyleSheet(R"(
            QTextBrowser {
                background-color: #121214;
                color: #E1E1E6;
                border: 1px solid #29292E;
                border-radius: 4px;
                padding: 10px;
                font-family: Consolas, Monaco, monospace;
                font-size: 11px;
            }
        )");

        QString licenseHtml = tr(R"(
            <h3>■ 本システム（TwitchChannelPointManager）本体のライセンス</h3>
            <p><strong>MIT License</strong><br>
            Copyright (c) 2026 BLUE000<br><br>
            Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:<br><br>
            The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.</p>
            
            <hr style="border: 0; border-top: 1px solid #29292E;">
            
            <h3>■ サードパーティ製ライブラリのライセンス・権利表記</h3>
            <p><strong>1. TransCipher Library</strong><br>
            本システムで使用している暗号化・復号化ライブラリです。<br>
            Copyright (c) 2026 BLUE000.<br>
            Licensed under the MIT License.</p>
            
            <p><strong>2. Qt ツールキット (Qt6)</strong><br>
            本システムは <strong>Qt ツールキット (LGPLv3 ライセンス)</strong> を動的にリンクして使用しています。<br>
            Qt ツールキットの著作権は The Qt Company およびその貢献者に帰属します。<br>
            詳細およびソースコードについては、以下の公式サイトをご覧ください。<br>
            <a href="https://www.qt.io" style="color: #2196F3;">https://www.qt.io</a><br>
            LGPLv3の規約に基づき、ユーザーは独自の変更を加えたQtライブラリをリンクして本アプリを実行する権利が保障されています（動的リンク形式）。</p>
            
            <p><strong>3. Google Test (GTest)</strong><br>
            本システムの自動テストに使用されているテストフレームワークです。<br>
            Copyright 2008, Google Inc. All rights reserved.<br>
            Licensed under the 3-Clause BSD License.</p>
        )");

        textBrowser->setHtml(licenseHtml);
        layout->addWidget(textBrowser);

        QPushButton* closeButton = new QPushButton(tr("閉じる"), &dialog);
        closeButton->setStyleSheet("background-color: #29292E; color: #FFFFFF; border: 1px solid #35353B; border-radius: 4px; padding: 6px 15px; font-weight: bold;");
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        layout->addWidget(closeButton, 0, Qt::AlignRight);

        dialog.exec();
    });

    // オンラインヘルプボタンの接続
    connect(m_helpButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/BLUE000/TwitchChannelPointManager/blob/master/README.md"));
    });

    // 全体の基本スタイル
    setStyleSheet(R"(
        QMainWindow { background-color: #121214; }
        QDialog { background-color: #121214; }
        QTabWidget::pane { border: 1px solid #29292E; background-color: #1D1D22; top: -1px; }
        QTabBar::tab { background-color: #121214; color: #A9A9B2; border: 1px solid #29292E; padding: 10px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; }
        QTabBar::tab:selected { background-color: #1D1D22; color: #FFFFFF; border-bottom-color: #1D1D22; font-weight: bold; }
        QGroupBox { border: 1px solid #29292E; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #FFFFFF; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }
        QLabel { color: #E1E1E6; }
        QLineEdit { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QSpinBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px 26px 4px 6px; min-height: 26px; }
        QSpinBox::up-button { width: 20px; subcontrol-origin: border; subcontrol-position: top right; }
        QSpinBox::down-button { width: 20px; subcontrol-origin: border; subcontrol-position: bottom right; }
        QComboBox { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QPushButton { border: 1px solid #29292E; border-radius: 4px; padding: 5px; color: #FFFFFF; background-color: #29292E; }
        QPushButton:hover { background-color: #35353B; }

        /* QCheckBoxスタイル（ステータス文字の視認性確保） */
        QCheckBox { color: #E1E1E6; font-size: 13px; spacing: 5px; }
        QCheckBox::indicator { border: 1px solid #29292E; background-color: #121214; width: 14px; height: 14px; border-radius: 3px; }
        QCheckBox::indicator:checked { background-color: #2196F3; border-color: #2196F3; }

        /* QListWidgetのダークモードスタイル（文字とのコントラスト確保） */
        QListWidget { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 5px; }
        QListWidget::item { padding: 6px; border-bottom: 1px solid #1D1D22; border-radius: 3px; }
        QListWidget::item:hover { background-color: #1D1D22; }
        QListWidget::item:selected { background-color: #29292E; color: #FFFFFF; }
        
        /* QMessageBox用ダークモードスタイル */
        QMessageBox { background-color: #1D1D22; }
        QMessageBox QLabel { color: #E1E1E6; font-size: 13px; }
        QMessageBox QPushButton { background-color: #29292E; color: #FFFFFF; border: 1px solid #35353B; border-radius: 4px; padding: 5px 15px; min-width: 60px; }
        QMessageBox QPushButton:hover { background-color: #35353B; }
    )");
}

void MainWindow::setupConnections()
{
    // タブが切り替えられた際に自動的にデータを最新に更新する
    connect(m_tabWidget, &QTabWidget::currentChanged, [this](int index) {
        switch (index) {
            case 0: // ダッシュボード
                m_dashboardWidget->refreshStats();
                break;
            case 1: // 報酬管理
                m_rewardEditorWidget->reloadRewardsList();
                break;
            case 2: // 統計ランキング
                m_statisticsWidget->refreshRanking();
                break;
            case 3: // 設定
                m_settingsWidget->loadCurrentSettings();
                break;
        }
    });

    // Twitch の致命的な認証失敗イベント検知 -> 警告ダイアログ表示
    connect(m_app, &Application::authFailedFatal, this, [this](const QString& errorMessage) {
        QMessageBox::critical(this, tr("Twitch連携認証エラー"), errorMessage);
    });
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // マウス移動、ボタン押下/釈放、キー入力、スクロールを検知して無操作タイマーをリセット
    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::KeyPress ||
        event->type() == QEvent::Wheel) {
        resetIdleTimer();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resetIdleTimer()
{
    if (m_idleTimer) {
        m_idleTimer->start(); // 再スタートで5分カウントをリセット
    }
    if (m_okojoLabel && m_okojoLabel->isVisible()) {
        m_okojoLabel->hide();
    }
}

void MainWindow::onIdleTimeout()
{
    // ランダムに3つの画像のうち1つを選択
    int choice = QRandomGenerator::global()->bounded(3);

    QString base64Data;
    int origW = 0;
    int origH = 0;

    if (choice == 0) {
        // okojo28x28.png (28x28)
        base64Data = "iVBORw0KGgoAAAANSUhEUgAAABwAAAAcCAYAAAByDd+UAAAFLklEQVRIx61WS0yTWRTuGjZuXLs2LNmwcW8Im2kyZkYgiojADDBqYjQUeRgKlDfl1VIUpEIFSYFRHuElMDzHEsqAIKY8CvKyIwXkRwriN+dcpTLQCoxzkpve/v8957vn3nO+75eZTCYZAOeYn58/m5+fr83NzTXR0E5PT3uvra2drqioUOTk5HQVFRXh0aNHoP/QarVQq9VdNTU1Nz58+HB6ZmbGm33y8vJMHGNuchs57f2wesv2AND9PASSHw4HZ2VmQAy5cuCAVFxdLq6urcGcLCwsoKCiQAgMDpcXFRRAwtre3UVhYKPX398tdAn78+NEjJSXFxs76HC1aiqsQEfoLbDYbjmuTk5MIvRyMAUMj9NlasenU1FTbzs6OxyFA3gnNYSgswXy7GWnKFNAmcFLb3NxEoiIO639Oojz/PgYHB9HR0RFwCLC6uvqa1WpFbW4plIr4/wS2Z5IkQaVIoFN6grGxMXDsQ4B0HN61tbW4+MOPePv2Lb7XRkdHEfrTJQbDy5cvz7ksmoyMDGMRVd7/ZUqlEmlpaU1uq5R2o+CiOWifPn06MrirNVQXoPhyt4A6nc540In7ztPTEyqVyi1YaGgoTp06hYaGhn89393d5dbIcguYmZlpOhjM19eXVslw+/Ztt5n5+/uLNREREYfeE2CXW8CsrCzTmzdvMDU1JZqXm5gvPzw8HH19fW4zrKurQ1RUlPAhpgKxkxh8PcQ6ru+wp6dHHhYWZhoYGMDw8DCGhobQ0/0Hqg0VqH/6VGzCnY2MjKDh2TOxtp82Zjabhf/z58955555100l7vMkBpU3t3djebmZjQ2NqKxuhJNpRo0GsqwvvaV2uRyOSIjI53/lxYX0GR4SGu1aDI+QX19Pdrb21FVVcVZe7vMsLe3N8BgMGQFBwdb7XY7Xw7s42a06rJh1GTDMvEKRNCYmbKgo70Fgy/6MWl5jZ2dbZgHX6CW1nSU5EGafiV8+TivXr069vjx42jK+PwhwJWVlTP8S03qnZSUBKPRiIa6GjisE8CqTQRhs6/8jW3HFt6vrxGjvN+rHHqxjHXLKOpra0RmsbGx0tLS0lmOSQmcdnmkVDBn9Xp9VllZmYJZxxnsuPZlrUajkSorK2+Ul5crCeyMyyPlFy0tLdf3S1ViYqL07t27Y+OxOty7d8/KNMkxqGU8SCuV1I+eLjPcP0gTPaKjo6XS0lIWWXD1bm1tuSRqLhASZyHKMTExXa7iHQlIR3otKCjIyRhc5tTEgnk4OG+CFZ/YCRMTE4IAuKi8vLywVw8nAqRqNXIjn9QePHgAvr8TA4aEhDilnhv4W0aKDiKOz1VMLXXz5k39sQH5U4PuK4BYRwTgb5Pk5OQjM4uLi3PO/fz8rOPj4+foKjzcAlLjy9VFWmP6fR2uXI/CnTt3hHNnZ+eRGbLR95BTonx8fJCs0yBVp5XytFoj8fFXAW5tbT2TlJFu+t00gCnsYOS9HUnkTC0hnEtKSuBKIw8a9Zwgbz5eEl3ciosV8XhUEDNl5Kqb+HNTdjHoctPr7U3ny0z9Q9FPpBzY2NgAcyvz6nHUndumra1N3CeRB3pmp51x/1pfgX9IsCS7FPmrae8hj2jl58xYptLT08VcoVB8E4wlidhFZMdXwW20vLwsjnV/7FtJiZDlqNUBOeVlsOw6xENVZib2V2dCQgKys7O/CciVGR8fj7t374qN7llMmkrEfOWQkKIpkEg3b4iiYS0M/i0KZrsNqi9Zfa9xAcWnp6F/YRY/XwmSLBaLKJx/AJgqI/DuR9vuAAAAAElFTkSuQmCC";
        origW = 28;
        origH = 28;
    } else if (choice == 1) {
        // watching.png (16x16)
        base64Data = "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAALzSURBVDhPjZJrSFNhGMf/58zdcs2tsstUyqblhVFRIUlFFpomUdGd7EPQhyIEsehGsvyg9C3pQwQGUREVShaRm0qFKzG8FJUVlqlNt6bufmpuZzvn7ZzTkAqLfl/e933+/+d5n+flpSBQWba1sKCopDXo87S96el8eP7yXVgbb+BDT+eM9KXZF2i58tHeoyf2CNaQ6P+NmqqKdZ9sVpY4BgiZZAjhomSKWIQQ5yAhriFSV1V5wWw20/E0nN21MYUQUDTjGsnLyM6VE+0cQKUBoaY8gEwBfuYsgAB56zeW19fX68VwXc2ZfYkLMz+fOrT9CS2jlF2RKBemQgH0tj6AteEmxh122Jqb8NrWBjrKArpkuEaGW5xO5zexgEqX7PUzYYpQyg10SvqiLEUCreQdX7DYuAS5q9Yg6PNiwcJ0zNPpwA30gRsbQW9PxyshNyIWuH2s0hYYczDfI1EkaJJmBycViZQ6awX0ag2kHn+B902AUs/g9FqdJx5CuzDcwbVrkb95m3SW2x7dext/tmnptj32FBcf0EpugZrj5TnCmAFRE18s2vXUcoePhn+q02D/9A5W660pg5KK7E42pGkZnxsyMWAy6AoKd+wvoBLkkuFPZmp18lU5xvzl8zW+JDktK929/6Qxd1nK85YHg5Kh8eolM8/FpHanI/QtSAbfvSTu0SHSbmny9r/u5gJuF7lSe7pIKsDzvDnu/St93R0kxASlfW97K1tbcfiIlCxi//i+SlL+QcDrIWwkLO2F2ZliLYzxdOFblpVW8+ykJP4XMZY03Lne2u1hTOhwBq81Wiw+t2NY0uwD/YL+86av9iFp/dLfR7h4zPt1hLQ13ycvx4PkhdNPqLfBGOE4FkMtDdi+bSeeNt+Ham4qPKODUFM88jaV4uaNq1i5vggKGY1hux1JGSakZWdibHT8CdXlnFit0miqfU57if+ZBRFaDn3+Fkz6vVD5XQjTMszKNCEcjYHEophrSAXLRkLg+HOrDfqLPwBtGx+Y8u717QAAAABJRU5ErkJggg==";
        origW = 16;
        origH = 16;
    } else {
        // おこじょさんのイラスト2.png (18x19)
        base64Data = "iVBORw0KGgoAAAANSUhEUgAAABISAAATCAYAAACdkl3yAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAANLSURBVDhPYyAGTK0v6jq2adnfrQunXkgPslOFCpMEOPuqcmqvnzr8//+n1///f379f2ZHzV6gOMeEymRxiBIIYITSKCDF10zRzTu0lYuH19Q7IlGFgZkNIvH/H8PrB7cZVs2dtuH1m9ftPDxSV0t7e79CJLGALYtn7Pj//d1/ZHDy4K7/t08e/P//ya3/Z9Yv+ZQS5OLenx8vANWCHRxZv+zJ/w8vwQYc2rbu//NH9/6/efHs//dPH/7///jm/7YFU09DlcIBM5RGAYoSAt+lhAS8hX5/Z+AQkWAQkpBl4BUQZGBh52BgYOdieP/xw18Z5m8MEe52IT8/PTl67+XX31CtCFCfG80X4mxouWvexAf/P70CuwobuHx07//vLx78r4rz8QTpw3CRu6kBj6iA0P8Hz5+8ZeXh53/97Om7zx/esYtISHFClYCBmKwiw9efPxn2bVo75/jVuw/AgjPT0lgrY7x1wBwgWNRdzN1TlCYCYldHuSZvWjDl/t+/f6FuQYADW9bcBioB+hcKSuMCJjbnJf5vTI9whQrBwb5ls5+ANP379w+sGRns27DiClAJE0gdmPjDwOjxm4GF4eXbNyUgPjJ48OTp7A9vXv5nZMRMciJS0kpAZ4MTJtggZkbGV//+MzCo6RjZZdlrSYDEYGBSTcOyh7eu/v358wdUBAGYmVlYpPkZ2EFssEEfXr3YHp2UypBf38mhYu2UDhKDgaa5Mzv4+QVZjqxdxPD+9UuoKAT8+Pbt08WPDO9AbLBBN27fm/3104fPILabf3iZt5a4GZitxSfEw8cfxM7Dx6BhZsfw+vljkDAcvHn+5CaQ+gRig6P/0bsv36S4/j1Q09IOVtYxYjUwt44yVpFiERaS0JCUkfNQ0NBheHj3FoOSph4DGyhRgsF/hm0r50/bfujkEagAAtQkBUYc37356f9/v8Gx8vnVk/8b5k/+f3Lv1v/vnj8ERR1YHATOHt7zw0yaUwaqFWvuF+wuTnKXlldW+cfA9PXsod1HeURE2eUUlPXZOTirTGycJWUVlRj6agqL66cv74PqIR5UJfi7v3v1HOykDQumnQIKQcsWCMCaabGBSC8XXw5WJs3De7YtDk0tiAEKIaUHBgYA9ImjsPlX2Z4AAAAASUVORK5CYII=";
        origW = 18;
        origH = 19;
    }

    QByteArray ba = QByteArray::fromBase64(base64Data.toUtf8());
    QPixmap pixmap;
    pixmap.loadFromData(ba);

    if (pixmap.isNull()) {
        return;
    }

    // ステータスバーの現在の高さを取得して拡縮サイズを決定
    int sbHeight = statusBar()->height();
    if (sbHeight <= 0) sbHeight = 25; // フォールバック

    int targetH = sbHeight - 4; // 上下にマージン2px
    if (targetH < 8) targetH = sbHeight;

    int targetW = qRound(origW * ((double)targetH / (double)origH));

    QPixmap scaledPixmap = pixmap.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (!m_okojoLabel) {
        m_okojoLabel = new QLabel(statusBar());
        m_okojoLabel->setAttribute(Qt::WA_TransparentForMouseEvents); // ステータスバーの操作を邪魔しない
    }

    m_okojoLabel->setPixmap(scaledPixmap);
    m_okojoLabel->setFixedSize(targetW, targetH);

    // 空き領域（ステータスバーの横幅内）のランダムな位置に表示
    int maxX = statusBar()->width() - targetW;
    int x = maxX > 0 ? QRandomGenerator::global()->bounded(maxX) : 0;
    int y = (sbHeight - targetH) / 2;

    m_okojoLabel->move(x, y);
    m_okojoLabel->show();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    if (BUILD_IS_CUSTOMIZED) {
        setWindowTitle(QString(tr("Twitch Channel Point Manager - %1 (Custom Build)")).arg(APP_VERSION_STRING));
    } else {
        setWindowTitle(QString(tr("Twitch Channel Point Manager - %1")).arg(APP_VERSION_STRING));
    }

    m_tabWidget->setTabText(0, tr("🏠 ダッシュボード"));
    m_tabWidget->setTabText(1, tr("🎁 報酬演出管理"));
    m_tabWidget->setTabText(2, tr("📊 統計ランキング"));
    m_tabWidget->setTabText(3, tr("⚙️ システム設定"));

    m_aboutButton->setText(tr("ℹ️ ABOUT"));
    m_aboutButton->setToolTip(tr("アプリケーション情報とライセンス表記を表示します"));
    m_helpButton->setText(tr("❓ HELP"));
    m_helpButton->setToolTip(tr("GitHubのオンラインマニュアル（README.md）を開きます"));
}


