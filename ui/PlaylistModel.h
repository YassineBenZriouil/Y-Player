#pragma once

#include "core/Playlist.h"

#include <QAbstractListModel>

namespace yp::ui {

// List model over a yp::Playlist the window owns. All structural changes go through this
// model so views stay in sync. Display text is the file name only (no disk access), so
// very large playlists stay instant.
class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit PlaylistModel(Playlist& playlist, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    void insertPaths(int row, const QStringList& paths);  // row < 0 appends
    void removeRowsAt(const QList<int>& rows);
    void clearAll();
    void setCurrent(int row);
    void resetFromPlaylist();  // after the playlist was replaced wholesale

Q_SIGNALS:
    void filesDropped(int row, const QStringList& paths);  // external drops; the window decides

private:
    void moveRows(QList<int> rows, int destination);

    Playlist& playlist_;
};

}  // namespace yp::ui
