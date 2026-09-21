#pragma once

#include <QAbstractTableModel>
#include <QVariantList>

class FaultTableModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        CodeRole = Qt::UserRole + 1,
        LevelRole,
        NodeRole,
        DetailRole,
        ActiveRole
    };

    explicit FaultTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString headerName(int section) const;
    // levelFilter: "" | "ERROR" | "WARN" | "INFO"
    Q_INVOKABLE void setLevelFilter(const QString& level);

public slots:
    void setFaults(const QVariantList& faults);

signals:
    void countChanged();

private:
    struct Row {
        QString code;
        QString level;
        QString node;
        QString detail;
        bool active = true;
    };
    void rebuildVisible();

    QVector<Row> all_;
    QVector<Row> visible_;
    QString level_filter_;
};
