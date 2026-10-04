/*
  Q Light Controller Plus
  livecontrolmanager.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include "livecontrolmanager.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "contextmanager.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "qlcinputsource.h"
#include "universe.h"

namespace
{
const struct { const char *id; const char *name; const char *group; bool button; } s_targets[] = {
    { "intensity", QT_TR_NOOP("Intensity"), QT_TR_NOOP("Intensity"), false },
    { "hue", QT_TR_NOOP("Hue"), QT_TR_NOOP("Color"), false },
    { "saturation", QT_TR_NOOP("Saturation"), QT_TR_NOOP("Color"), false },
    { "color-value", QT_TR_NOOP("Color value"), QT_TR_NOOP("Color"), false },
    { "red", QT_TR_NOOP("Red"), QT_TR_NOOP("Color"), false },
    { "green", QT_TR_NOOP("Green"), QT_TR_NOOP("Color"), false },
    { "blue", QT_TR_NOOP("Blue"), QT_TR_NOOP("Color"), false },
    { "white", QT_TR_NOOP("White"), QT_TR_NOOP("Color"), false },
    { "amber", QT_TR_NOOP("Amber"), QT_TR_NOOP("Color"), false },
    { "uv", QT_TR_NOOP("UV"), QT_TR_NOOP("Color"), false },
    { "color-wheel-previous", QT_TR_NOOP("Previous color-wheel capability"), QT_TR_NOOP("Color"), true },
    { "color-wheel-next", QT_TR_NOOP("Next color-wheel capability"), QT_TR_NOOP("Color"), true },
    { "macro-previous", QT_TR_NOOP("Previous macro capability"), QT_TR_NOOP("Color"), true },
    { "macro-next", QT_TR_NOOP("Next macro capability"), QT_TR_NOOP("Color"), true },
    { "pan", QT_TR_NOOP("Pan"), QT_TR_NOOP("Position"), false },
    { "tilt", QT_TR_NOOP("Tilt"), QT_TR_NOOP("Position"), false },
    { "position-center", QT_TR_NOOP("Center position"), QT_TR_NOOP("Position"), true },
    { "zoom", QT_TR_NOOP("Zoom"), QT_TR_NOOP("Beam"), false },
    { "strobe", QT_TR_NOOP("Strobe rate"), QT_TR_NOOP("Beam"), false },
    { "gobo-wheel-previous", QT_TR_NOOP("Previous gobo-wheel capability"), QT_TR_NOOP("Beam"), true },
    { "gobo-wheel-next", QT_TR_NOOP("Next gobo-wheel capability"), QT_TR_NOOP("Beam"), true },
    { "highlight", QT_TR_NOOP("Highlight selection"), QT_TR_NOOP("Actions"), true },
    { "preset", QT_TR_NOOP("Capability preset"), QT_TR_NOOP("Presets"), true }
};

bool isButtonTarget(const QString &target)
{
    return target == QStringLiteral("position-center") || target == QStringLiteral("highlight")
            || target == QStringLiteral("preset") || target.endsWith(QStringLiteral("-next"))
            || target.endsWith(QStringLiteral("-previous"));
}
}

LiveControlManager::LiveControlManager(Doc *doc, ContextManager *contextManager, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_contextManager(contextManager)
    , m_nextId(1)
{
    connect(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged,
            this, &LiveControlManager::slotInputValueChanged);
    connect(m_contextManager, &ContextManager::selectedFixturesChanged,
            this, &LiveControlManager::slotSelectionChanged);
    connect(m_contextManager, &ContextManager::selectedFixturesChanged,
            this, &LiveControlManager::presetTargetsChanged);
    for (Universe *universe : m_doc->inputOutputMap()->universes())
        connect(universe, &Universe::inputPatchChanged,
                this, &LiveControlManager::slotInputPatchesChanged);
    connect(m_doc->inputOutputMap(), &InputOutputMap::universeAdded, this,
            [this](quint32 id)
    {
        Universe *universe = m_doc->inputOutputMap()->universe(id);
        if (universe)
            connect(universe, &Universe::inputPatchChanged,
                    this, &LiveControlManager::slotInputPatchesChanged);
    });
}

LiveControlManager::~LiveControlManager()
{
    reset();
}

QVariantList LiveControlManager::targets() const
{
    QVariantList result;
    for (const auto &target : s_targets)
    {
        if (QString::fromLatin1(target.id) == QStringLiteral("preset"))
            continue;
        QVariantMap item;
        item["id"] = QString::fromLatin1(target.id);
        item["name"] = tr(target.name);
        item["group"] = tr(target.group);
        item["button"] = target.button;
        result.append(item);
    }
    return result;
}

QVariantList LiveControlManager::mappings() const
{
    QVariantList result;
    for (const Mapping &mapping : m_mappings)
    {
        QVariantMap item;
        item["id"] = mapping.id;
        item["target"] = mapping.target;
        item["universe"] = mapping.universe;
        item["channel"] = mapping.channel;
        item["sourceName"] = sourceName(mapping.universe, mapping.channel);
        item["valid"] = sourceValid(mapping.universe, mapping.channel);
        item["preset"] = mapping.preset;
        item["customFeedback"] = supportsCustomFeedback(mapping);
        item["name"] = mapping.target == QStringLiteral("preset")
                ? mapping.preset.value("name").toString() : mapping.target;
        result.append(item);
    }
    return result;
}

QVariantList LiveControlManager::presetTargets() const
{
    return m_contextManager->liveControlPresets();
}

QVariantList LiveControlManager::inputUniverses() const
{
    QVariantList result;
    for (Universe *universe : m_doc->inputOutputMap()->universes())
    {
        if (!universe || !universe->inputPatch() || !universe->inputPatch()->isPatched())
            continue;
        QVariantMap item;
        item["mLabel"] = QStringLiteral("%1: %2").arg(universe->id() + 1).arg(universe->name());
        item["mValue"] = universe->id();
        result.append(item);
    }
    return result;
}

bool LiveControlManager::learning() const { return !m_learningTarget.isEmpty(); }
QString LiveControlManager::learningTarget() const { return m_learningTarget; }

bool LiveControlManager::isKnownTarget(const QString &target) const
{
    for (const auto &known : s_targets)
        if (target == QString::fromLatin1(known.id))
            return true;
    return false;
}

void LiveControlManager::beginLearn(const QString &target, const QVariantMap &preset)
{
    if (!isKnownTarget(target))
        return;
    m_learningTarget = target;
    m_learningPreset = preset;
    emit learningChanged();
}

void LiveControlManager::cancelLearn()
{
    if (m_learningTarget.isEmpty())
        return;
    m_learningTarget.clear();
    m_learningPreset.clear();
    emit learningChanged();
}

bool LiveControlManager::addMapping(const QString &target, quint32 universe, quint32 channel,
                                    const QVariantMap &preset)
{
    if (!isKnownTarget(target) || !sourceValid(universe, channel))
        return false;
    for (const Mapping &item : m_mappings)
        if (item.target == target && item.universe == universe && item.channel == channel && item.preset == preset)
            return false;

    Mapping mapping;
    mapping.id = m_nextId++;
    mapping.target = target;
    mapping.universe = universe;
    mapping.channel = channel;
    mapping.preset = preset;
    configureSource(mapping);
    m_mappings.append(mapping);
    Mapping &added = m_mappings.last();
    const qreal current = m_contextManager->liveControlValue(added.target);
    if (current >= 0)
    {
        if (added.source)
            added.source->updateOuputValue(uchar(qBound(0, qRound(current), 255)));
        sendFeedback(added, qRound(current));
    }
    else if (isButtonTarget(added.target))
        sendFeedback(added, 0, QLCInputFeedback::LowerValue);
    m_doc->setModified();
    emit mappingsChanged();
    return true;
}

void LiveControlManager::configureSource(Mapping &mapping)
{
    Universe *universe = m_doc->inputOutputMap()->universe(mapping.universe);
    if (!universe || !universe->inputPatch() || !universe->inputPatch()->profile())
        return;
    QLCInputProfile *profile = universe->inputPatch()->profile();
    QLCInputChannel *channel = profile->channel(mapping.channel);
    if (!channel)
        return;

    const QVariant profileParams = profile->channelExtraParams(channel);
    if (mapping.feedbackLowerParams.toInt() == -1)
        mapping.feedbackLowerParams = profileParams;
    if (mapping.feedbackUpperParams.toInt() == -1)
        mapping.feedbackUpperParams = profileParams;
    if (mapping.feedbackMonitorParams.toInt() == -1)
        mapping.feedbackMonitorParams = profileParams;

    if (channel->type() == QLCInputChannel::Button)
    {
        if (mapping.feedbackLower == 0)
            mapping.feedbackLower = channel->lowerValue();
        if (mapping.feedbackUpper == UCHAR_MAX)
            mapping.feedbackUpper = channel->upperValue();
    }

    if (!channel || channel->movementType() != QLCInputChannel::Relative)
        return;

    mapping.source.reset(new QLCInputSource(mapping.universe, mapping.channel));
    mapping.source->setSensitivity(channel->movementSensitivity());
    mapping.source->setWorkingMode(QLCInputSource::Relative);
    connect(mapping.source.data(), &QLCInputSource::inputValueChanged,
            this, &LiveControlManager::slotRelativeValueChanged);
    qreal current = m_contextManager->liveControlValue(mapping.target);
    if (current >= 0)
        mapping.source->updateOuputValue(uchar(qBound(0, qRound(current), 255)));
}

bool LiveControlManager::supportsCustomFeedback(const Mapping &mapping) const
{
    Universe *universe = m_doc->inputOutputMap()->universe(mapping.universe);
    if (!universe || !universe->inputPatch() || !universe->inputPatch()->profile())
        return false;
    QLCInputChannel *channel = universe->inputPatch()->profile()->channel(mapping.channel);
    return channel && channel->type() == QLCInputChannel::Button;
}

QVariant LiveControlManager::mappingFeedbackInfo(int id) const
{
    for (const Mapping &mapping : m_mappings)
    {
        if (mapping.id != id)
            continue;

        QVariantMap info;
        info["lowerValue"] = mapping.feedbackLower;
        info["upperValue"] = mapping.feedbackUpper;
        info["monitorValue"] = mapping.feedbackMonitor;
        info["hasColorTable"] = false;
        info["hasMIDIChannelTable"] = false;

        Universe *universe = m_doc->inputOutputMap()->universe(mapping.universe);
        InputPatch *patch = universe ? universe->inputPatch() : nullptr;
        QLCInputProfile *profile = patch ? patch->profile() : nullptr;
        if (!profile)
            return info;

        if (profile->hasColorTable())
        {
            info["hasColorTable"] = true;
            QVariantList colors;
            const auto colorTable = profile->colorTable();
            for (auto it = colorTable.cbegin(); it != colorTable.cend(); ++it)
            {
                QVariantMap color;
                color["index"] = it.key();
                color["name"] = it.value().first;
                color["color"] = it.value().second.name();
                colors.append(color);
                if (it.key() == mapping.feedbackLower) info["lowerColor"] = it.value().second.name();
                if (it.key() == mapping.feedbackUpper) info["upperColor"] = it.value().second.name();
                if (it.key() == mapping.feedbackMonitor) info["monitorColor"] = it.value().second.name();
            }
            info["colorTable"] = colors;
        }

        if (profile->type() == QLCInputProfile::MIDI && profile->hasMidiChannelTable())
        {
            info["hasMIDIChannelTable"] = true;
            QVariantList channels;
            channels.append(tr("From plugin settings"));
            const auto midiChannelTable = profile->midiChannelTable();
            for (auto it = midiChannelTable.cbegin(); it != midiChannelTable.cend(); ++it)
                channels.append(it.value());
            info["midiChannelTable"] = channels;
            if (mapping.feedbackLowerParams.isValid()) info["lowerChannel"] = mapping.feedbackLowerParams.toInt() + 1;
            if (mapping.feedbackUpperParams.isValid()) info["upperChannel"] = mapping.feedbackUpperParams.toInt() + 1;
            if (mapping.feedbackMonitorParams.isValid()) info["monitorChannel"] = mapping.feedbackMonitorParams.toInt() + 1;
        }
        return info;
    }
    return QVariant();
}

bool LiveControlManager::updateMappingFeedbackValues(int id, quint8 lower, quint8 upper, quint8 monitor)
{
    for (Mapping &mapping : m_mappings)
    {
        if (mapping.id != id)
            continue;
        mapping.feedbackLower = lower;
        mapping.feedbackUpper = upper;
        mapping.feedbackMonitor = monitor;
        m_doc->setModified();
        emit mappingsChanged();
        sendFeedback(mapping, lower, QLCInputFeedback::LowerValue);
        return true;
    }
    return false;
}

bool LiveControlManager::updateMappingFeedbackExtraParams(int id, int lower, int upper, int monitor)
{
    for (Mapping &mapping : m_mappings)
    {
        if (mapping.id != id)
            continue;
        mapping.feedbackLowerParams = lower - 1;
        mapping.feedbackUpperParams = upper - 1;
        mapping.feedbackMonitorParams = monitor - 1;
        m_doc->setModified();
        emit mappingsChanged();
        sendFeedback(mapping, mapping.feedbackLower, QLCInputFeedback::LowerValue);
        return true;
    }
    return false;
}

void LiveControlManager::removeMapping(int id)
{
    for (int i = 0; i < m_mappings.count(); ++i)
    {
        if (m_mappings.at(i).id != id)
            continue;
        if (isButtonTarget(m_mappings[i].target))
            sendFeedback(m_mappings[i], 0, QLCInputFeedback::LowerValue);
        if (m_mappings[i].source)
            m_mappings[i].source->setWorkingMode(QLCInputSource::Absolute);
        m_mappings.removeAt(i);
        m_doc->setModified();
        emit mappingsChanged();
        return;
    }
}

void LiveControlManager::clearMappings()
{
    if (m_mappings.isEmpty())
        return;
    for (Mapping &mapping : m_mappings)
    {
        if (isButtonTarget(mapping.target))
            sendFeedback(mapping, 0, QLCInputFeedback::LowerValue);
        if (mapping.source)
            mapping.source->setWorkingMode(QLCInputSource::Absolute);
    }
    m_mappings.clear();
    m_doc->setModified();
    emit mappingsChanged();
}

void LiveControlManager::reset()
{
    cancelLearn();
    for (Mapping &mapping : m_mappings)
    {
        if (isButtonTarget(mapping.target))
            sendFeedback(mapping, 0, QLCInputFeedback::LowerValue);
        if (mapping.source)
            mapping.source->setWorkingMode(QLCInputSource::Absolute);
    }
    m_mappings.clear();
    m_nextId = 1;
    emit mappingsChanged();
}

bool LiveControlManager::sourceValid(quint32 universeId, quint32 channelId) const
{
    Universe *universe = m_doc->inputOutputMap()->universe(universeId);
    Q_UNUSED(channelId)
    return universe && universe->inputPatch() && universe->inputPatch()->isPatched();
}

QString LiveControlManager::sourceName(quint32 universeId, quint32 channelId) const
{
    Universe *universe = m_doc->inputOutputMap()->universe(universeId);
    if (!universe)
        return tr("Universe %1, channel %2").arg(universeId + 1).arg(channelId + 1);
    QString channelName = tr("Channel %1").arg(channelId + 1);
    if (universe->inputPatch() && universe->inputPatch()->profile())
    {
        QLCInputChannel *channel = universe->inputPatch()->profile()->channel(channelId);
        if (channel && !channel->name().isEmpty())
            channelName = channel->name();
    }
    return QStringLiteral("%1 / %2").arg(universe->name(), channelName);
}

void LiveControlManager::slotInputValueChanged(quint32 universe, quint32 channel, uchar value,
                                                const QString &key)
{
    Q_UNUSED(key)
    if (!m_learningTarget.isEmpty())
    {
        const QString target = m_learningTarget;
        const QVariantMap preset = m_learningPreset;
        cancelLearn();
        addMapping(target, universe, channel, preset);
        return;
    }

    for (Mapping &mapping : m_mappings)
    {
        if (mapping.universe != universe || mapping.channel != channel)
            continue;
        if (mapping.source)
            mapping.source->updateInputValue(value);
        else if (dispatch(mapping, value))
            sendFeedback(mapping, value, isButtonTarget(mapping.target)
                         ? QLCInputFeedback::UpperValue : QLCInputFeedback::Undefinded);
        else if (isButtonTarget(mapping.target) && value == 0)
            sendFeedback(mapping, value, QLCInputFeedback::LowerValue);
    }
}

void LiveControlManager::slotRelativeValueChanged(quint32 universe, quint32 channel, uchar value)
{
    QLCInputSource *source = qobject_cast<QLCInputSource *>(sender());
    for (Mapping &mapping : m_mappings)
    {
        if (mapping.universe == universe && mapping.channel == channel && mapping.source.data() == source)
        {
            if (dispatch(mapping, value, true))
                sendFeedback(mapping, value, isButtonTarget(mapping.target)
                             ? QLCInputFeedback::UpperValue : QLCInputFeedback::Undefinded);
            else if (isButtonTarget(mapping.target) && value == 0)
                sendFeedback(mapping, value, QLCInputFeedback::LowerValue);
            mapping.source->updateOuputValue(value);
            break;
        }
    }
}

bool LiveControlManager::dispatch(Mapping &mapping, uchar value, bool relative)
{
    const bool button = isButtonTarget(mapping.target);
    if (button)
    {
        const bool pressed = value > 0;
        const bool wasPressed = mapping.hasValue && mapping.lastValue > 0;
        mapping.hasValue = true;
        mapping.lastValue = value;
        if (!pressed || wasPressed)
            return false;
        m_contextManager->applyLiveControl(mapping.target, value, false, mapping.preset);
        emit controlValueChanged(mapping.target, value, mapping.preset);
        return true;
    }

    if (!relative && !mapping.caught)
    {
        const qreal current = m_contextManager->liveControlValue(mapping.target);
        if (current < 0) // mixed values: deliberately unify on first movement
            mapping.caught = true;
        else if (!mapping.hasValue)
        {
            mapping.hasValue = true;
            mapping.lastValue = value;
            if (qRound(current) != value)
                return false;
            mapping.caught = true;
        }
        else if ((mapping.lastValue <= current && value >= current)
                 || (mapping.lastValue >= current && value <= current))
            mapping.caught = true;
        else
        {
            mapping.lastValue = value;
            return false;
        }
    }
    mapping.hasValue = true;
    mapping.lastValue = value;
    m_contextManager->applyLiveControl(mapping.target, value, relative, mapping.preset);
    emit controlValueChanged(mapping.target, value, mapping.preset);
    return true;
}

void LiveControlManager::sendFeedback(Mapping &mapping, int value, QLCInputFeedback::FeedbackType type)
{
    QVariant params = mapping.feedbackUpperParams;
    if (type == QLCInputFeedback::LowerValue)
    {
        value = mapping.feedbackLower;
        params = mapping.feedbackLowerParams;
    }
    else if (type == QLCInputFeedback::UpperValue)
    {
        value = mapping.feedbackUpper;
        params = mapping.feedbackUpperParams;
    }
    else if (type == QLCInputFeedback::MonitorValue)
    {
        value = mapping.feedbackMonitor;
        params = mapping.feedbackMonitorParams;
    }
    m_doc->inputOutputMap()->sendFeedBack(mapping.universe, mapping.channel,
                                          uchar(qBound(0, value, 255)), params);
}

void LiveControlManager::slotSelectionChanged()
{
    for (Mapping &mapping : m_mappings)
    {
        mapping.caught = false;
        mapping.hasValue = false;
        const qreal current = m_contextManager->liveControlValue(mapping.target);
        if (mapping.source)
        {
            if (current >= 0)
                mapping.source->updateOuputValue(uchar(qBound(0, qRound(current), 255)));
        }
        if (current >= 0)
            sendFeedback(mapping, qRound(current));
        else if (isButtonTarget(mapping.target))
            sendFeedback(mapping, mapping.feedbackLower, QLCInputFeedback::LowerValue);
    }
    emit mappingsChanged();
}

void LiveControlManager::slotInputPatchesChanged()
{
    for (Mapping &mapping : m_mappings)
    {
        if (mapping.source)
            mapping.source->setWorkingMode(QLCInputSource::Absolute);
        mapping.source.clear();
        mapping.caught = false;
        mapping.hasValue = false;
        configureSource(mapping);
    }
    emit mappingsChanged();
    emit inputUniversesChanged();
}

bool LiveControlManager::saveXML(QXmlStreamWriter *writer) const
{
    writer->writeStartElement(KXMLQLCLiveControls);
    for (const Mapping &mapping : m_mappings)
    {
        writer->writeStartElement(QStringLiteral("Mapping"));
        writer->writeAttribute(QStringLiteral("Target"), mapping.target);
        writer->writeAttribute(QStringLiteral("Universe"), QString::number(mapping.universe));
        writer->writeAttribute(QStringLiteral("Channel"), QString::number(mapping.channel));
        if (mapping.feedbackLower != 0)
            writer->writeAttribute(QStringLiteral("LowerValue"), QString::number(mapping.feedbackLower));
        if (mapping.feedbackUpper != UCHAR_MAX)
            writer->writeAttribute(QStringLiteral("UpperValue"), QString::number(mapping.feedbackUpper));
        if (mapping.feedbackMonitor != UCHAR_MAX)
            writer->writeAttribute(QStringLiteral("MonitorValue"), QString::number(mapping.feedbackMonitor));
        if (mapping.feedbackLowerParams.toInt() != -1)
            writer->writeAttribute(QStringLiteral("LowerParams"), mapping.feedbackLowerParams.toString());
        if (mapping.feedbackUpperParams.toInt() != -1)
            writer->writeAttribute(QStringLiteral("UpperParams"), mapping.feedbackUpperParams.toString());
        if (mapping.feedbackMonitorParams.toInt() != -1)
            writer->writeAttribute(QStringLiteral("MonitorParams"), mapping.feedbackMonitorParams.toString());
        for (auto it = mapping.preset.cbegin(); it != mapping.preset.cend(); ++it)
            writer->writeTextElement(it.key(), it.value().toString());
        writer->writeEndElement();
    }
    writer->writeEndElement();
    return !writer->hasError();
}

bool LiveControlManager::loadXML(QXmlStreamReader &reader)
{
    reset();
    while (reader.readNextStartElement())
    {
        if (reader.name() != QStringLiteral("Mapping"))
        {
            reader.skipCurrentElement();
            continue;
        }
        const auto attrs = reader.attributes();
        const QString target = attrs.value(QStringLiteral("Target")).toString();
        const quint32 universe = attrs.value(QStringLiteral("Universe")).toUInt();
        const quint32 channel = attrs.value(QStringLiteral("Channel")).toUInt();
        QVariantMap preset;
        while (reader.readNextStartElement())
            preset[reader.name().toString()] = reader.readElementText();
        if (preset.contains(QStringLiteral("group")))
            preset[QStringLiteral("group")] = preset.value(QStringLiteral("group")).toInt();
        if (preset.contains(QStringLiteral("preset")))
            preset[QStringLiteral("preset")] = preset.value(QStringLiteral("preset")).toInt();
        if (isKnownTarget(target))
        {
            bool duplicate = false;
            for (const Mapping &item : m_mappings)
                if (item.target == target && item.universe == universe && item.channel == channel
                        && item.preset == preset)
                    duplicate = true;
            if (duplicate)
                continue;
            Mapping mapping;
            mapping.id = m_nextId++;
            mapping.target = target;
            mapping.universe = universe;
            mapping.channel = channel;
            mapping.preset = preset;
            if (attrs.hasAttribute(QStringLiteral("LowerValue")))
                mapping.feedbackLower = uchar(attrs.value(QStringLiteral("LowerValue")).toUInt());
            if (attrs.hasAttribute(QStringLiteral("UpperValue")))
                mapping.feedbackUpper = uchar(attrs.value(QStringLiteral("UpperValue")).toUInt());
            if (attrs.hasAttribute(QStringLiteral("MonitorValue")))
                mapping.feedbackMonitor = uchar(attrs.value(QStringLiteral("MonitorValue")).toUInt());
            if (attrs.hasAttribute(QStringLiteral("LowerParams")))
                mapping.feedbackLowerParams = attrs.value(QStringLiteral("LowerParams")).toInt();
            if (attrs.hasAttribute(QStringLiteral("UpperParams")))
                mapping.feedbackUpperParams = attrs.value(QStringLiteral("UpperParams")).toInt();
            if (attrs.hasAttribute(QStringLiteral("MonitorParams")))
                mapping.feedbackMonitorParams = attrs.value(QStringLiteral("MonitorParams")).toInt();
            configureSource(mapping);
            m_mappings.append(mapping);
        }
    }
    emit mappingsChanged();
    return !reader.hasError();
}
