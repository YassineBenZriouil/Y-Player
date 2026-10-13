#pragma once

#include <QDockWidget>

class QListView;

namespace yp::ui {

class PlaylistModel;

// Dockable playlist: double-click/Enter plays, Delete removes, drag to reorder, drop files to add.
class PlaylistPanel : public QDockWidget {
    Q_OBJECT
public:
    PlaylistPanel(PlaylistModel& model, QWidget* parent = nullptr);

    [[nodiscard]] QList<int> selectedRows() const;
    void scrollToRow(int row);

Q_SIGNALS:
    void playRequested(int row);
    void removeRequested(const QList<int>& rows);
    void addRequested();
    void clearRequested();

private:
    QListView* view_ = nullptr;
};

}  // namespace yp::ui
