/*
  Q Light Controller Plus
  livecontrolmanager.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef LIVECONTROLMANAGER_H
#define LIVECONTROLMANAGER_H

#include <QObject>
#include <QSharedPointer>
#include <QVariant>

class ContextManager;
class Doc;
class QLCInputSource;
class QXmlStreamReader;
class QXmlStreamWriter;

#define KXMLQLCLiveControls QStringLiteral("LiveControls")

class LiveControlManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList targets READ targets CONSTANT)
    Q_PROPERTY(QVariantList mappings READ mappings NOTIFY mappingsChanged)
    Q_PROPERTY(QVariantList presetTargets READ presetTargets NOTIFY presetTargetsChanged)
    Q_PROPERTY(QVariantList inputUniverses READ inputUniverses NOTIFY inputUniversesChanged)
    Q_PROPERTY(bool learning READ learning NOTIFY learningChanged)
    Q_PROPERTY(QString learningTarget READ learningTarget NOTIFY learningChanged)

public:
    LiveControlManager(Doc *doc, ContextManager *contextManager, QObject *parent = nullptr);
    ~LiveControlManager() override;

    QVariantList targets() const;
    QVariantList mappings() const;
    QVariantList presetTargets() const;
    QVariantList inputUniverses() const;
    bool learning() const;
    QString learningTarget() const;

    Q_INVOKABLE void beginLearn(const QString &target, const QVariantMap &preset = QVariantMap());
    Q_INVOKABLE void cancelLearn();
    Q_INVOKABLE bool addMapping(const QString &target, quint32 universe, quint32 channel,
                                const QVariantMap &preset = QVariantMap());
    Q_INVOKABLE void removeMapping(int id);
    Q_INVOKABLE void clearMappings();

    bool loadXML(QXmlStreamReader &reader);
    bool saveXML(QXmlStreamWriter *writer) const;
    void reset();

signals:
    void mappingsChanged();
    void presetTargetsChanged();
    void learningChanged();
    void inputUniversesChanged();

private slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key);
    void slotRelativeValueChanged(quint32 universe, quint32 channel, uchar value);
    void slotSelectionChanged();
    void slotInputPatchesChanged();

private:
    struct Mapping
    {
        int id = -1;
        QString target;
        quint32 universe = 0;
        quint32 channel = 0;
        QVariantMap preset;
        bool caught = false;
        bool hasValue = false;
        uchar lastValue = 0;
        QSharedPointer<QLCInputSource> source;
    };

    bool isKnownTarget(const QString &target) const;
    void configureSource(Mapping &mapping);
    void dispatch(Mapping &mapping, uchar value, bool relative = false);
    QString sourceName(quint32 universe, quint32 channel) const;
    bool sourceValid(quint32 universe, quint32 channel) const;

    Doc *m_doc;
    ContextManager *m_contextManager;
    QList<Mapping> m_mappings;
    int m_nextId;
    QString m_learningTarget;
    QVariantMap m_learningPreset;
};

#endif
