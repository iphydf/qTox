/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2026 The TokTok team.
 */

#include <QObject>
#include <QtPlugin>

#include <QtQmlCompiler/qqmlsa.h>
#include <functional>
#include <memory>
#include <tuple>

using namespace Qt::StringLiterals;

namespace {
// Qt Quick shows text that looks like HTML as HTML, and fetches its images, unless the Text it's in
// sets a textFormat: names and messages can't go there. The rule knows the text a file binds or
// assigns, not what gets there through an alias or a Binding.
constexpr QQmlSA::LoggerWarningId untrustedText{"Plugin.qTox.untrusted-text"};

constexpr QLatin1StringView templates{"QtQuick.Templates"};

/**
 * @brief A control that shows a property in a Text of its own.
 */
struct Control
{
    QLatin1StringView type;
    QLatin1StringView property;
    // The property a QML file can set to its own Text for it, if there's one.
    QLatin1StringView item;
};

// As the Material and Basic styles make them.
constexpr Control controls[] = {
    {"AbstractButton"_L1, "text"_L1, "contentItem"_L1},
    {"Action"_L1, "text"_L1, {}},
    {"Dialog"_L1, "title"_L1, "header"_L1},
    {"GroupBox"_L1, "title"_L1, "label"_L1},
    {"Menu"_L1, "title"_L1, {}},
    {"TextArea"_L1, "placeholderText"_L1, {}},
    {"TextField"_L1, "placeholderText"_L1, {}},
};

bool isQuote(QChar c)
{
    return c == u'"' || c == u'\'';
}

bool isOpening(QChar c)
{
    return c == u'(' || c == u'[' || c == u'{';
}

bool isClosing(QChar c)
{
    return c == u')' || c == u']' || c == u'}';
}

/** Where the string or bracket that starts at @p begin ends, with all that's inside it, or -1. */
qsizetype endOf(QStringView code, qsizetype begin)
{
    const QChar first = code[begin];
    for (qsizetype i = begin + 1; i >= 0 && i < code.size();) {
        if (isQuote(first)) {
            if (code[i] == first) {
                return i + 1;
            }
            i += code[i] == u'\\' ? 2 : 1;
        } else if (isClosing(code[i])) {
            return i + 1;
        } else if (isQuote(code[i]) || isOpening(code[i])) {
            i = endOf(code, i);
        } else {
            ++i;
        }
    }
    return -1;
}

/** Whether @p code is nothing but a string. */
bool isString(QStringView code)
{
    return !code.isEmpty() && isQuote(code.first()) && endOf(code, 0) == code.size();
}

/** Whether @p code is nothing but a call of qsTr() for a string. */
bool isTranslation(QStringView code)
{
    constexpr QLatin1StringView call{"qsTr"};
    if (!code.startsWith(call) || code.size() == call.size() || code[call.size()] != u'('
        || endOf(code, call.size()) != code.size()) {
        return false;
    }
    QStringView arguments = code.sliced(call.size() + 1).chopped(1).trimmed();
    // The text and what tells it from the same text elsewhere. After them comes only a number.
    for (int i = 0; i < 2; ++i) {
        const qsizetype end =
            arguments.isEmpty() || !isQuote(arguments.first()) ? -1 : endOf(arguments, 0);
        if (end < 0) {
            return false;
        }
        arguments = arguments.sliced(end).trimmed();
        if (arguments.isEmpty()) {
            return true;
        }
        if (arguments.first() != u',') {
            return false;
        }
        arguments = arguments.sliced(1).trimmed();
    }
    return true;
}

/**
 * @brief Whether @p code can only be our own words: a string, the translation of one, or a choice
 *        between such.
 *
 * Says no to all that it doesn't know.
 */
bool isOwnWords(QStringView code)
{
    code = code.trimmed();
    // The first choice's "?", and how many more of them wait for their ":".
    qsizetype choice = -1;
    int inner = 0;
    for (qsizetype i = 0; i < code.size();) {
        if (isQuote(code[i]) || isOpening(code[i])) {
            i = endOf(code, i);
            if (i < 0) {
                return false;
            }
            continue;
        }
        if (code[i] == u'?') {
            inner += choice < 0 ? 0 : 1;
            choice = choice < 0 ? i : choice;
        } else if (code[i] == u':' && choice >= 0) {
            if (inner == 0) {
                return isOwnWords(code.sliced(choice + 1, i - choice - 1))
                       && isOwnWords(code.sliced(i + 1));
            }
            --inner;
        }
        ++i;
    }
    return choice < 0 && (isString(code) || isTranslation(code));
}

bool setsTextFormat(const QQmlSA::Element& text)
{
    return text.hasPropertyBindings(u"textFormat"_s);
}

/** Whether the object @p element has in @p item, its own before its style's, sets a textFormat. */
bool itemSetsTextFormat(const QQmlSA::Element& element, QLatin1StringView item)
{
    const QList<QQmlSA::Binding> items = element.propertyBindings(item);
    return !items.isEmpty() && items.first().hasObject() && setsTextFormat(items.first().objectType());
}

/** What's wrong with text that isn't our own words in @p text, a Text: nothing if it's fine. */
QString textProblem(const QQmlSA::Element& text)
{
    return setsTextFormat(text) ? QString{} : u"This text shows as HTML: set the textFormat."_s;
}

/** The same for @p control's property of @p element. */
QString controlProblem(const QQmlSA::Element& element, const Control& control)
{
    if (control.item.isEmpty()) {
        return u"This %1 shows as HTML: only our own words can go here."_s.arg(control.property);
    }
    return itemSetsTextFormat(element, control.item)
               ? QString{}
               : u"This %1 shows as HTML: give its control a %2 that sets its textFormat."_s
                     .arg(control.property, control.item);
}

const QString attachedToolTipProblem =
    u"The attached ToolTip shows this text as HTML: use a ToolTip with a contentItem that sets its "
    "textFormat."_s;

// What the file binds. Read from each object, as a pass for a property hears only of the bindings
// Qt can work out the type of.
class BindingPass : public QQmlSA::ElementPass
{
public:
    explicit BindingPass(QQmlSA::PassManager* manager)
        : ElementPass{manager}
        , text{resolveType("QtQuick", "Text")}
        , toolTip{resolveType(templates, "ToolTip")}
        , attachedToolTip{resolveAttached(templates, "ToolTip")}
    {
        for (const Control& control : controls) {
            controlTypes.append(resolveType(templates, control.type));
        }
    }

    bool shouldRun(const QQmlSA::Element& element) override
    {
        return element.scopeType() == QQmlSA::ScopeType::QMLScope;
    }

    void run(const QQmlSA::Element& element) override
    {
        if (element.inherits(text)) {
            check(element, "text"_L1, textProblem(element));
        }
        for (qsizetype i = 0; i < controlTypes.size(); ++i) {
            if (element.inherits(controlTypes[i])) {
                check(element, controls[i].property, controlProblem(element, controls[i]));
            }
        }
        // A ToolTip takes its text from show() too, which says nothing of the ToolTip it's called
        // on, so each one needs its own Text.
        if (element.inherits(toolTip) && !itemSetsTextFormat(element, "contentItem"_L1)) {
            emitWarning(u"This ToolTip shows text as HTML: give it a contentItem that sets its "
                        "textFormat."_s,
                        untrustedText, element.sourceLocation());
        }
        for (const QQmlSA::Binding& binding : element.ownPropertyBindings()) {
            if (binding.bindingType() == QQmlSA::BindingType::AttachedProperty
                && binding.attachedType().inherits(attachedToolTip)) {
                check(binding.attachedType(), "text"_L1, attachedToolTipProblem);
            }
        }
    }

private:
    /** Warns of @p problem where @p element binds @p property to more than our own words. */
    void check(const QQmlSA::Element& element, QLatin1StringView property, const QString& problem)
    {
        if (problem.isEmpty()) {
            return;
        }
        for (const QQmlSA::Binding& binding : element.ownPropertyBindings(property)) {
            if (!isConstant(binding)) {
                emitWarning(problem, untrustedText, binding.sourceLocation());
            }
        }
    }

    bool isConstant(const QQmlSA::Binding& binding)
    {
        switch (binding.bindingType()) {
        case QQmlSA::BindingType::StringLiteral:
        case QQmlSA::BindingType::Translation:
        case QQmlSA::BindingType::TranslationById:
            return true;
        case QQmlSA::BindingType::Script:
            return isOwnWords(sourceCode(binding.sourceLocation()));
        default:
            return false;
        }
    }

    const QQmlSA::Element text;
    const QQmlSA::Element toolTip;
    const QQmlSA::Element attachedToolTip;
    // As in controls.
    QList<QQmlSA::Element> controlTypes;
};

// What the file's code assigns to a property, which is never our own words to this pass.
class WritePass : public QQmlSA::PropertyPass
{
public:
    using Problem = std::function<QString(const QQmlSA::Element&)>;

    WritePass(QQmlSA::PassManager* manager, Problem problem_)
        : PropertyPass{manager}
        , problem{std::move(problem_)}
    {
    }

    void onWrite(const QQmlSA::Element& element, const QString& propertyName,
                 const QQmlSA::Element& value, const QQmlSA::Element& writeScope,
                 QQmlSA::SourceLocation location) override
    {
        std::ignore = propertyName;
        std::ignore = value;
        std::ignore = writeScope;
        const QString found = problem(element);
        if (!found.isEmpty()) {
            emitWarning(found, untrustedText, location);
        }
    }

private:
    const Problem problem;
};

// What the file's code does with the ToolTip attached to an item, which can't have its own Text.
class AttachedToolTipPass : public QQmlSA::PropertyPass
{
public:
    explicit AttachedToolTipPass(QQmlSA::PassManager* manager)
        : PropertyPass{manager}
        , attached{resolveAttached(templates, "ToolTip")}
    {
    }

    void onWrite(const QQmlSA::Element& element, const QString& propertyName,
                 const QQmlSA::Element& value, const QQmlSA::Element& writeScope,
                 QQmlSA::SourceLocation location) override
    {
        std::ignore = value;
        std::ignore = writeScope;
        check(element, propertyName, "text"_L1, location);
    }

    void onCall(const QQmlSA::Element& element, const QString& propertyName,
                const QQmlSA::Element& readScope, QQmlSA::SourceLocation location) override
    {
        std::ignore = readScope;
        check(element, propertyName, "show"_L1, location);
    }

    // The ToolTip that all items share.
    void onRead(const QQmlSA::Element& element, const QString& propertyName,
                const QQmlSA::Element& readScope, QQmlSA::SourceLocation location) override
    {
        std::ignore = readScope;
        check(element, propertyName, "toolTip"_L1, location);
    }

private:
    void check(const QQmlSA::Element& element, const QString& propertyName,
               QLatin1StringView unsafe, QQmlSA::SourceLocation location)
    {
        if (element.inherits(attached) && propertyName == unsafe) {
            emitWarning(attachedToolTipProblem, untrustedText, location);
        }
    }

    const QQmlSA::Element attached;
};
} // namespace

/**
 * @brief qTox's own standards for its QML code.
 *
 * A standard we want across the user interface goes here as a rule with a warning of its own, not
 * left to review: the build fails on any warning.
 */
class QtoxLintPlugin : public QObject, public QQmlSA::LintPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QmlLintPluginInterface_iid FILE "qtoxlintplugin.json")
    Q_INTERFACES(QQmlSA::LintPlugin)

public:
    void registerPasses(QQmlSA::PassManager* manager, const QQmlSA::Element& rootElement) override
    {
        std::ignore = rootElement;
        manager->registerElementPass(std::make_unique<BindingPass>(manager));
        manager->registerPropertyPass(std::make_shared<WritePass>(manager, textProblem), "QtQuick",
                                      "Text", "text");
        for (const Control& control : controls) {
            const auto problem = [&control](const QQmlSA::Element& element) {
                return controlProblem(element, control);
            };
            manager->registerPropertyPass(std::make_shared<WritePass>(manager, problem), templates,
                                          control.type, control.property);
        }
        // For every type: the attached type has no name to ask for.
        manager->registerPropertyPass(std::make_shared<AttachedToolTipPass>(manager), "", "");
    }
};

#include "qtoxlintplugin.moc"
