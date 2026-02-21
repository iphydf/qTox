/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2017-2019 by The qTox Project Contributors
 * Copyright © 2024-2026 The TokTok team.
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

struct Tox_Pass_Key;

class ToxEncrypt
{
public:
    ~ToxEncrypt();
    ToxEncrypt() = delete;
    ToxEncrypt(const ToxEncrypt& other) = delete;
    ToxEncrypt& operator=(const ToxEncrypt& other) = delete;

    static int getMinBytes();
    static bool isEncrypted(const QByteArray& ciphertext);
    static QByteArray encryptPass(const QString& password, const QByteArray& plaintext);
    static QByteArray decryptPass(const QString& password, const QByteArray& ciphertext);
    static std::unique_ptr<ToxEncrypt> makeToxEncrypt(const QString& password);
    static std::unique_ptr<ToxEncrypt> makeToxEncrypt(const QString& password,
                                                      const QByteArray& toxSave);
    QByteArray encrypt(const QByteArray& plaintext) const;
    QByteArray decrypt(const QByteArray& ciphertext) const;

private:
    explicit ToxEncrypt(Tox_Pass_Key* key);

private:
    Tox_Pass_Key* passKey = nullptr;
};
