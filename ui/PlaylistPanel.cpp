#include "ui/PlaylistPanel.h"

#include "ui/PlaylistModel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace yp::ui {

PlaylistPanel::PlaylistPanel(PlaylistModel& model, QWidget* parent) : QDockWidget(tr("Playlist"), parent) {
    setObjectName(QStringLiteral("playlistDock"));
    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);

    view_ = new QListView(body);
    view_->setModel(&model);
    view_->setUniformItemSizes(true);  // O(1) layout for very large playlists
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setDragDropMode(QAbstractItemView::DragDrop);
    view_->setDefaultDropAction(Qt::MoveAction);
    view_->setDropIndicatorShown(true);
    view_->setAccessibleName(tr("Playlist"));
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(view_, 1);

    connect(view_, &QListView::activated, this, [this](const QModelIndex& i) { Q_EMIT playRequested(i.row()); });

    auto* remove = new QAction(tr("Remove"), view_);
    remove->setShortcut(QKeySequence::Delete);
    remove->setShortcutContext(Qt::WidgetShortcut);
    connect(remove, &QAction::triggered, this, [this] {
        if (const QList<int> rows = selectedRows(); !rows.isEmpty()) Q_EMIT removeRequested(rows);
    });
    view_->addAction(remove);

    connect(view_, &QListView::customContextMenuRequested, this, [this, remove](const QPoint& pos) {
        QMenu menu(this);
        const QModelIndex at = view_->indexAt(pos);
        if (at.isValid()) menu.addAction(tr("Play"), this, [this, at] { Q_EMIT playRequested(at.row()); });
        menu.addAction(remove);
        menu.addSeparator();
        menu.addAction(tr("Add Files..."), this, &PlaylistPanel::addRequested);
        menu.addAction(tr("Clear Playlist"), this, &PlaylistPanel::clearRequested);
        menu.exec(view_->viewport()->mapToGlobal(pos));
    });

    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(tr("Add..."), body);
    auto* removeButton = new QPushButton(tr("Remove"), body);
    auto* clear = new QPushButton(tr("Clear"), body);
    connect(add, &QPushButton::clicked, this, &PlaylistPanel::addRequested);
    connect(removeButton, &QPushButton::clicked, remove, &QAction::trigger);
    connect(clear, &QPushButton::clicked, this, &PlaylistPanel::clearRequested);
    for (QPushButton* b : {add, removeButton, clear}) {
        b->setFocusPolicy(Qt::NoFocus);
        buttons->addWidget(b);
    }
    layout->addLayout(buttons);
    setWidget(body);
}

QList<int> PlaylistPanel::selectedRows() const {
    QList<int> rows;
    for (const QModelIndex& i : view_->selectionModel()->selectedIndexes()) rows << i.row();
    return rows;
}

void PlaylistPanel::scrollToRow(int row) {
    if (row >= 0) view_->scrollTo(view_->model()->index(row, 0));
}

}  // namespace yp::ui
