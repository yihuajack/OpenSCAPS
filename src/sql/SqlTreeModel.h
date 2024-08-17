//
// Created by Yihua Liu on 2024/8/12.
//

#ifndef SUISAPP_SQLMODEL_H
#define SUISAPP_SQLMODEL_H

#include <QAbstractItemModel>
#include <QtSql/QSqlDatabase>

#include "SqlTreeItem.h"

class SqlTreeModel : QAbstractItemModel {
    Q_OBJECT

public:
    Q_DISABLE_COPY_MOVE(SqlTreeModel);

    explicit SqlTreeModel(const QStringList &headers, const QString &data, QAbstractItemModel *parent = nullptr);
    ~SqlTreeModel() override;

    QSqlDatabase currentDatabase() const;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    bool setHeaderData(int section, Qt::Orientation orientation,
                       const QVariant &value, int role = Qt::EditRole) override;

    bool insertColumns(int position, int columns, const QModelIndex &parent = {}) override;
    bool removeColumns(int position, int columns, const QModelIndex &parent = {}) override;
    bool insertRows(int position, int rows, const QModelIndex &parent = {}) override;
    bool removeRows(int position, int rows, const QModelIndex &parent = {}) override;

public slots:
    void refresh();
//    void showMetaData();

private:
    SqlTreeItem *getItem(const QModelIndex &index) const;

    std::unique_ptr<SqlTreeItem> rootItem;
};


#endif  // SUISAPP_SQLMODEL_H
