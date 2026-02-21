/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2015-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include "src/core/toxencrypt.h"

#include <QDataStream>
#include <QSettings>
#include <QString>
#include <QVector>

#include <utility>

class SettingsSerializer
{
public:
    enum class RecordTag : uint8_t
    {
        Value = 0,
        GroupStart = 1,
        ArrayStart = 2,
        ArrayValue = 3,
        ArrayEnd = 4,
    };
    explicit SettingsSerializer(QString filePath_, const ToxEncrypt* passKey_ = nullptr);

    static bool isSerializedFormat(QString filePath);

    void load();
    void save();

    void beginGroup(const QString& prefix);
    void endGroup();

    int beginReadArray(const QString& prefix);
    void beginWriteArray(const QString& prefix, int size = -1);
    void endArray();
    void setArrayIndex(int i);

    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

private:
    struct Value
    {
        Value()
            : group{-2}
            , array{-2}
            , arrayIndex{-2}
            , key{QString()}
            , value{}
        {
        }
        Value(qint64 group_, qint64 array_, int arrayIndex_, QString key_, QVariant value_)
            : group{group_}
            , array{array_}
            , arrayIndex{arrayIndex_}
            , key{std::move(key_)}
            , value{std::move(value_)}
        {
        }
        qint64 group;
        qint64 array;
        int arrayIndex;
        QString key;
        QVariant value;
    };

    struct Array
    {
        qint64 group;
        int size;
        QString name;
        QVector<int> values;
    };

private:
    const Value* findValue(const QString& key) const;
    Value* findValue(const QString& key);
    void readSerialized();
    void readIni();
    void removeValue(const QString& key);
    void removeGroup(int group);
    static void writePackedVariant(QDataStream& dataStream, const QVariant& v);

private:
    QString path;
    const ToxEncrypt* passKey;
    int group, array, arrayIndex;
    QStringList groups;
    QVector<Array> arrays;
    QVector<Value> values;
    static const char magic[];
};
