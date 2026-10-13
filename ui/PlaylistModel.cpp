#include "ui/PlaylistModel.h"

#include <QDataStream>
#include <QFileInfo>
#include <QFont>
#include <QIODevice>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

namespace yp::ui {
namespace {

const QString kRowsMime = QStringLiteral("application/x-yplayer-playlist-rows");

}  // namespace

PlaylistModel::PlaylistModel(Playlist& playlist, QObject* parent) : QAbstractListModel(parent), playlist_(playlist) {}

int PlaylistModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : playlist_.size(); }

QVariant PlaylistModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= playlist_.size()) return {};
    const QString& path = playlist_.at(index.row());
    switch (role) {
        case Qt::DisplayRole: return QFileInfo(path).fileName();
        case Qt::ToolTipRole: return path;
        case Qt::FontRole:
            if (index.row() == playlist_.current()) {
                QFont bold;
                bold.setBold(true);
                return bold;
            }
            return {};
        default: return {};
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const {
    const Qt::ItemFlags base = QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
    return index.isValid() ? base | Qt::ItemIsDragEnabled : base;
}

Qt::DropActions PlaylistModel::supportedDropActions() const { return Qt::MoveAction | Qt::CopyAction; }

QStringList PlaylistModel::mimeTypes() const { return {kRowsMime, QStringLiteral("text/uri-list")}; }

QMimeData* PlaylistModel::mimeData(const QModelIndexList& indexes) const {
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    for (const QModelIndex& i : indexes) stream << i.row();
    auto* mime = new QMimeData;
    mime->setData(kRowsMime, encoded);
    return mime;
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction /*action*/, int row, int /*column*/,
                                 const QModelIndex& parent) {
    const int destination = row >= 0 ? row : (parent.isValid() ? parent.row() : playlist_.size());
    if (data->hasFormat(kRowsMime)) {
        QList<int> rows;
        QDataStream stream(data->data(kRowsMime));
        while (!stream.atEnd()) {
            int r = 0;
            stream >> r;
            rows << r;
        }
        moveRows(rows, destination);
        return false;  // we moved the rows ourselves; stop the view from also removing them
    }
    QStringList paths;
    for (const QUrl& url : data->urls()) {
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    if (paths.isEmpty()) return false;
    Q_EMIT filesDropped(destination, paths);
    return true;
}

void PlaylistModel::moveRows(QList<int> rows, int destination) {
    std::ranges::sort(rows);
    rows.erase(std::ranges::unique(rows).begin(), rows.end());
    if (rows.isEmpty()) return;
    // shortcut: a full reset instead of per-row move signals; fine for interactive drags,
    // revisit if views ever need to keep selection across reorders.
    beginResetModel();
    QStringList moved;
    for (const int r : rows) moved << playlist_.at(r);
    const int currentRow = playlist_.current();
    const QString currentPath = currentRow >= 0 ? playlist_.at(currentRow) : QString();
    const bool currentMoved = rows.contains(currentRow);
    const auto before = static_cast<int>(std::ranges::count_if(rows, [&](int r) { return r < destination; }));
    playlist_.removeAt(rows);
    const int insertAt = destination - before;
    playlist_.insert(insertAt, moved);
    if (currentMoved) playlist_.setCurrent(insertAt + static_cast<int>(rows.indexOf(currentRow)));
    endResetModel();
}

void PlaylistModel::insertPaths(int row, const QStringList& paths) {
    if (paths.isEmpty()) return;
    const int at = row < 0 || row > playlist_.size() ? playlist_.size() : row;
    beginInsertRows({}, at, at + static_cast<int>(paths.size()) - 1);
    playlist_.insert(at, paths);
    endInsertRows();
}

void PlaylistModel::removeRowsAt(const QList<int>& rows) {
    if (rows.isEmpty()) return;
    beginResetModel();
    playlist_.removeAt(rows);
    endResetModel();
}

void PlaylistModel::clearAll() {
    beginResetModel();
    playlist_.clear();
    endResetModel();
}

void PlaylistModel::setCurrent(int row) {
    const int old = playlist_.current();
    playlist_.setCurrent(row);
    for (const int r : {old, playlist_.current()}) {
        if (r >= 0) Q_EMIT dataChanged(index(r), index(r), {Qt::FontRole});
    }
}

void PlaylistModel::resetFromPlaylist() {
    beginResetModel();
    endResetModel();
}

}  // namespace yp::ui
