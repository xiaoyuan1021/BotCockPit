#include "nodetablemodel.hpp"

NodeTableModel::NodeTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int NodeTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return rows_.size();
}

int NodeTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 3;
}

QVariant NodeTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
        return {};
    }
    const NodeRow& row = rows_.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        if (index.column() == 0 || role == NameRole) {
            return row.name;
        }
        if (index.column() == 1) {
            return row.status;
        }
        return row.lastHb;
    case StatusRole:
        return row.status;
    case LastHbRole:
        return row.lastHb;
    default:
        return {};
    }
}

QHash<int, QByteArray> NodeTableModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {StatusRole, "status"},
        {LastHbRole, "lastHbMs"},
    };
}

QString NodeTableModel::headerName(int section) const
{
    switch (section) {
    case 0:
        return QStringLiteral("name");
    case 1:
        return QStringLiteral("status");
    case 2:
        return QStringLiteral("last_hb");
    default:
        return {};
    }
}

void NodeTableModel::setNodes(const QVariantList& nodes)
{
    beginResetModel();
    rows_.clear();
    rows_.reserve(nodes.size());
    for (const QVariant& item : nodes) {
        const QVariantMap m = item.toMap();
        NodeRow row;
        row.name = m.value(QStringLiteral("name")).toString();
        row.status = m.value(QStringLiteral("status")).toString();
        row.lastHb = static_cast<qint64>(
            m.value(QStringLiteral("last_hb_ms")).toLongLong());
        rows_.push_back(row);
    }
    endResetModel();
}
