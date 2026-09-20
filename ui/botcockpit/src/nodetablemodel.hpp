#pragma once

#include <QAbstractTableModel>
#include <QVariantList>

class NodeTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        StatusRole,
        LastHbRole
    };

    explicit NodeTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString headerName(int section) const;

public slots:
    void setNodes(const QVariantList& nodes);

private:
    struct NodeRow {
        QString name;
        QString status;
        qint64 lastHb = 0;
    };
    QVector<NodeRow> rows_;
};
