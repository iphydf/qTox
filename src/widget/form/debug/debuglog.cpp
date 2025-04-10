/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2014-2019 by The qTox Project Contributors
 * Copyright © 2024-2025 The TokTok team.
 */

#include "debuglog.h"

#include "ui_debuglog.h"

#include "src/model/debug/debuglogmodel.h"
#include "src/persistence/paths.h"
#include "src/widget/style.h"
#include "src/widget/translator.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QMetaEnum>
#include <QPushButton>
#include <QTimer>

#include <memory>
#include <tox/tox.h>

namespace {
QStringList loadLogs(Paths& paths)
{
    const QString logfile = paths.getAppCacheDirPath() + "qtox.log";

    QFile file(logfile);
    if (!file.exists()) {
        qDebug() << "No debug file found";
        return {};
    }

    QStringList debugLog;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            debugLog.append(in.readLine());
        }
        file.close();
    } else {
        qDebug() << "Unable to open file" << logfile;
        return {};
    }

    return debugLog;
}
} // namespace

DebugLogForm::DebugLogForm(Paths& paths, Style& style, QWidget* parent)
    : GenericForm{QPixmap(":/img/settings/general.png"), style, parent}
    , paths_{paths}
    , ui_{std::make_unique<Ui::DebugLog>()}
    , debugLogModel_{std::make_unique<DebugLogModel>(this)}
    , reloadTimer_{std::make_unique<QTimer>(this)}
{
    ui_->setupUi(this);

    // Reload logs every 5 seconds
    reloadTimer_->start(5000);
    connect(reloadTimer_.get(), &QTimer::timeout, this, [this, &paths] {
        if (ui_->cbAutoReload->isChecked()) {
            debugLogModel_->reload(loadLogs(paths));
        }
        if (ui_->cbAutoScroll->isChecked()) {
            ui_->debugLog->scrollToBottom();
        }
    });

    const auto filterEnum = QMetaEnum::fromType<DebugLogModel::Filter>();
    for (int i = 0; i < filterEnum.keyCount(); ++i) {
        ui_->cmbFilters->addItem(QString::fromUtf8(filterEnum.key(i)));
    }
    connect(ui_->cmbFilters, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        debugLogModel_->setFilter(static_cast<DebugLogModel::Filter>(index));
    });

    ui_->debugLog->setModel(debugLogModel_.get());
    connect(ui_->debugLog, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        const int originalIndex = debugLogModel_->originalIndex(index);
        ui_->cmbFilters->setCurrentIndex(0);
        ui_->debugLog->scrollTo(debugLogModel_->index(originalIndex));
    });

    Translator::registerHandler([this] { retranslateUi(); }, this);
}

DebugLogForm::~DebugLogForm()
{
    Translator::unregister(this);
}

void DebugLogForm::showEvent(QShowEvent* event)
{
    qDebug() << "Loading logs for debug log view";
    debugLogModel_->reload(loadLogs(paths_));

    GenericForm::showEvent(event);
}

/**
 * @brief Retranslate all elements in the form.
 */
void DebugLogForm::retranslateUi()
{
    ui_->retranslateUi(this);
}
