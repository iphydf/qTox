/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#include "profileimporter.h"

#include "src/persistence/paths.h"
#include "src/persistence/profile.h"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

/**
 * @class ProfileImporter
 * @brief This class provides the ability to import profile.
 * @note This class can be used before @a Nexus instance will be created,
 * consequently it can't use @a GUI class. Therefore it should use QMessageBox
 * to create dialog forms.
 */
ProfileImporter::ProfileImporter(Paths& paths_, QWidget* parent)
    : QWidget(parent)
    , paths{paths_}
{
}

/**
 * @brief Show a file dialog. Selected file will be imported as Tox profile.
 * @return True, if the import was successful. False otherwise.
 */
bool ProfileImporter::importProfile()
{
    const QString title = tr("Import profile", "import dialog title");
    const QString filter = tr("Tox save file (*.tox)", "import dialog filter");
    const QString dir = QDir::homePath();

    // TODO: Change all QFileDialog instances across project to use
    // this instead of Q_NULLPTR. Possibly requires >Qt 5.9 due to:
    // https://bugreports.qt.io/browse/QTBUG-59184
    const QString path = QFileDialog::getOpenFileName(Q_NULLPTR, title, dir, filter);

    return importProfile(path);
}

/**
 * @brief Asks the user a question with Yes/No choices.
 * @param title Title of question window.
 * @param message Text in question window.
 * @return True if the answer is positive, false otherwise.
 */
bool ProfileImporter::askQuestion(QString title, QString message)
{
    const QMessageBox::Icon icon = QMessageBox::Warning;
    QMessageBox box(icon, title, message, QMessageBox::NoButton, this);
    QPushButton* pushButton1 = box.addButton(QApplication::tr("Yes"), QMessageBox::AcceptRole);
    QPushButton* pushButton2 = box.addButton(QApplication::tr("No"), QMessageBox::RejectRole);
    box.setDefaultButton(pushButton2);
    box.setEscapeButton(pushButton2);

    box.exec();
    return box.clickedButton() == pushButton1;
}

/**
 * @brief Try to import Tox profile.
 * @param path Path to Tox profile.
 * @return True, if the import was successful. False otherwise.
 */
bool ProfileImporter::importProfile(const QString& path)
{
    if (path.isEmpty())
        return false;

    const QFileInfo info(path);
    if (!info.exists()) {
        QMessageBox::warning(this, tr("File doesn't exist"), tr("Profile doesn't exist"),
                             QMessageBox::Ok);
        return false;
    }

    const QString profile = Profile::importName(path);

    if (profile.isEmpty()) {
        QMessageBox::warning(this, tr("Ignoring non-Tox file", "popup title"),
                             tr("Warning: You have chosen a file that is not a "
                                "Tox save file; ignoring.",
                                "popup text"),
                             QMessageBox::Ok);
        return false; // ignore importing non-tox file
    }

    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Couldn't import the profile"),
                             tr("Unable to open this file."), QMessageBox::Ok);
        return false;
    }

    if (Profile::exists(profile, paths)) {
        const QString title = tr("Profile already exists", "import confirm title");
        const QString message = tr("A profile named \"%1\" already exists. "
                                   "Do you want to replace it?",
                                   "import confirm text")
                                    .arg(profile);
        const bool erase = askQuestion(title, message);

        if (!erase)
            return false; // import cancelled
    }

    switch (Profile::importSave(profile, file.readAll(), paths)) {
    case Profile::ImportResult::OK:
        break;
    case Profile::ImportResult::NotToxSave:
        QMessageBox::warning(this, tr("Couldn't import the profile"),
                             tr("This is not a Tox save file."), QMessageBox::Ok);
        return false;
    case Profile::ImportResult::InUse:
        QMessageBox::warning(this, tr("Couldn't import the profile"),
                             tr("The profile \"%1\" is in use.").arg(profile), QMessageBox::Ok);
        return false;
    case Profile::ImportResult::NotWritable:
        QMessageBox::warning(this, tr("Couldn't import the profile"),
                             tr("Can't write %1.").arg(Profile::getProfilePath(profile, paths)),
                             QMessageBox::Ok);
        return false;
    }
    // no good way to update the ui from here... maybe we need a Widget:refreshUi() function...
    // such a thing would simplify other code as well I believe
    QMessageBox::information(this, tr("Profile imported"),
                             tr("%1.tox was successfully imported").arg(profile), QMessageBox::Ok);

    return true; // import successful
}
