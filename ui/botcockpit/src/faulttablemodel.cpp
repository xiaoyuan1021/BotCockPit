#include "faulttablemodel.hpp"

#include <algorithm>

FaultTableModel::FaultTableModel(QObject* parent) : QAbstractTableModel(parent) {}

int FaultTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : visible_.size();
}

int FaultTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant FaultTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= visible_.size()) {
        return {};
    }
    const Row& r = visible_.at(index.row());
    switch (role) {
    case CodeRole:
    case Qt::DisplayRole:
        return index.column() == 1 ? r.level
             : index.column() == 2 ? r.node
             : index.column() == 3 ? r.detail
             : r.code;
    case LevelRole: return r.level;
    case NodeRole: return r.node;
    case DetailRole: return r.detail;
    case ActiveRole: return r.active;
    default: return {};
    }
}

QHash<int, QByteArray> FaultTableModel::roleNames() const
{
    return {
        {CodeRole, "code"},
        {LevelRole, "level"},
        {NodeRole, "node"},
        {DetailRole, "detail"},
        {ActiveRole, "active"},
    };
}

QString FaultTableModel::headerName(int section) const
{
    switch (section) {
    case 0: return QStringLiteral("code");
    case 1: return QStringLiteral("level");
    case 2: return QStringLiteral("node");
    case 3: return QStringLiteral("detail");
    default: return {};
    }
}

void FaultTableModel::setLevelFilter(const QString& level)
{
    if (level_filter_ == level) {
        return;
    }
    level_filter_ = level;
    rebuildVisible();
}

void FaultTableModel::setFaults(const QVariantList& faults)
{
    all_.clear();
    all_.reserve(faults.size());
    for (const QVariant& item : faults) {
        const QVariantMap m = item.toMap();
        Row r;
        r.code = m.value(QStringLiteral("code")).toString();
        r.level = m.value(QStringLiteral("level")).toString();
        r.node = m.value(QStringLiteral("node")).toString();
        r.detail = m.value(QStringLiteral("detail")).toString();
        r.active = m.value(QStringLiteral("active"), true).toBool();
        all_.push_back(r);
    }
    // ERROR first, then WARN, then INFO
    auto rank = [](const QString& lv) {
        if (lv == QLatin1String("ERROR")) return 0;
        if (lv == QLatin1String("WARN")) return 1;
        return 2;
    };
    std::sort(all_.begin(), all_.end(),
              [&rank](const Row& a, const Row& b) {
                  if (rank(a.level) != rank(b.level)) {
                      return rank(a.level) < rank(b.level);
                  }
                  return a.code < b.code;
              });
    rebuildVisible();
}

void FaultTableModel::rebuildVisible()
{
    beginResetModel();
    visible_.clear();
    for (const Row& r : all_) {
        if (level_filter_.isEmpty() || r.level == level_filter_) {
            visible_.push_back(r);
        }
    }
    endResetModel();
    emit countChanged();
}
