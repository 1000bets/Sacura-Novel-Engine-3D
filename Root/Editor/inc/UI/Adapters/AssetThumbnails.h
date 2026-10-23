#pragma once

#include "Assets/AssetTypes.h"
#include <QModelIndex>

class AssetRegistry;
class QAbstractItemModel;
class QComboBox;
class QAction;

class AssetThumbnails
{
public:
    static void Invalidate();
    static void Bind(QAction& Action, const AssetRegistry& Registry, const AssetKey& Key);
    static void Bind(QAbstractItemModel& Model, const QModelIndex& Index, const AssetRegistry& Registry, const AssetKey& Key);
    static void Bind(QComboBox& Picker, int Index, const AssetRegistry& Registry, const AssetKey& Key);
};
