/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QDialog>
#include <QShortcut>
#include <QToolButton>

class Paths;
class Profile;
class Style;

namespace Ui {
class LoginScreen;
}

class LoginScreen : public QDialog
{
    Q_OBJECT

public:
    LoginScreen(Paths& paths, Style& style, int themeColor,
                const QString& initialProfileName = QString(), QWidget* parent = nullptr);
    ~LoginScreen() override;
    bool event(QEvent* event) final;

signals:
    void windowStateChanged(Qt::WindowStates states);
    void autoLoginChanged(bool state);
    void createNewProfile(QString name, const QString& pass);
    void failure(const QString& title, const QString& message);
    void loadProfile(QString name, const QString& pass);

public slots:
    void onProfileLoaded();
    void onProfileLoadFailed();
    void onAutoLoginChanged(bool state);

private slots:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void onAutoLoginCheckboxChanged(Qt::CheckState state);
#else
    void onAutoLoginCheckboxChanged(int state);
#endif
    void onLoginUsernameSelected(const QString& name);
    void onPasswordEdited();
    // Buttons to change page
    void onNewProfilePageClicked();
    void onLoginPageClicked();
    // Buttons to submit form
    void onCreateNewProfile();
    void onLogin();
    void onImportProfile();

private:
    void reset(const QString& initialProfileName = QString());
    void retranslateUi();
    void showCapsIndicator();
    void hideCapsIndicator();
    void checkCapsLock();

private:
    Ui::LoginScreen* ui;
    QShortcut quitShortcut;
    Paths& paths;
};
