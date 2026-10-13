#include "ui/ShortcutDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace yp::ui {

ShortcutDialog::ShortcutDialog(ShortcutMap map, QWidget* parent) : QDialog(parent), map_(std::move(map)) {
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(560, 620);
    auto* layout = new QVBoxLayout(this);

    table_ = new QTableWidget(static_cast<int>(map_.definitions().size()), 2, this);
    table_->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int row = 0; row < table_->rowCount(); ++row) {
        const ActionDef& d = map_.definitions().at(row);
        auto* name = new QTableWidgetItem(QString(d.text).remove(QLatin1Char('&')));
        name->setData(Qt::UserRole, d.id);
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem);
        refreshRow(row);
    }
    layout->addWidget(table_, 1);

    auto* editRow = new QHBoxLayout;
    editor_ = new QKeySequenceEdit(this);
    editor_->setMaximumSequenceLength(1);
    editor_->setClearButtonEnabled(true);
    auto* assignButton = new QPushButton(tr("Assign"), this);
    auto* resetButton = new QPushButton(tr("Default"), this);
    editRow->addWidget(new QLabel(tr("Shortcut:"), this));
    editRow->addWidget(editor_, 1);
    editRow->addWidget(assignButton);
    editRow->addWidget(resetButton);
    layout->addLayout(editRow);

    message_ = new QLabel(this);
    message_->setWordWrap(true);
    layout->addWidget(message_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    layout->addWidget(buttons);

    connect(table_, &QTableWidget::itemSelectionChanged, this, &ShortcutDialog::loadSelection);
    connect(assignButton, &QPushButton::clicked, this, &ShortcutDialog::assign);
    connect(resetButton, &QPushButton::clicked, this, [this] {
        if (selectedId().isEmpty()) return;
        map_.reset(selectedId());
        for (int row = 0; row < table_->rowCount(); ++row) refreshRow(row);  // reset may free keys elsewhere
        loadSelection();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        map_.resetAll();
        for (int row = 0; row < table_->rowCount(); ++row) refreshRow(row);
        loadSelection();
        message_->setText(tr("All shortcuts restored to their defaults."));
    });
    table_->selectRow(0);
}

QString ShortcutDialog::selectedId() const {
    const QList<QTableWidgetItem*> items = table_->selectedItems();
    return items.isEmpty() ? QString() : table_->item(items.first()->row(), 0)->data(Qt::UserRole).toString();
}

QString ShortcutDialog::textFor(const QString& id) const {
    QStringList keys;
    for (const QKeySequence& k : map_.shortcuts(id)) keys << k.toString(QKeySequence::NativeText);
    return keys.join(QStringLiteral(", "));
}

void ShortcutDialog::refreshRow(int row) {
    table_->item(row, 1)->setText(textFor(table_->item(row, 0)->data(Qt::UserRole).toString()));
}

void ShortcutDialog::loadSelection() {
    const QList<QKeySequence> keys = map_.shortcuts(selectedId());
    editor_->setKeySequence(keys.value(0));
    message_->clear();
}

void ShortcutDialog::assign() {
    const QString id = selectedId();
    if (id.isEmpty()) return;
    // The editor holds one key; assigning replaces all keys of this action (Default restores them).
    QList<QKeySequence> keys;
    if (!editor_->keySequence().isEmpty()) keys << editor_->keySequence();
    QString conflict;
    if (!map_.set(id, keys, &conflict)) {
        QString owner;
        for (const ActionDef& d : map_.definitions()) {
            if (d.id == conflict) owner = QString(d.text).remove(QLatin1Char('&'));
        }
        message_->setText(tr("%1 is already used by \"%2\". Change that shortcut first.")
                              .arg(editor_->keySequence().toString(QKeySequence::NativeText), owner));
        return;
    }
    refreshRow(table_->currentRow());
    message_->setText(keys.isEmpty() ? tr("Shortcut removed.") : tr("Shortcut assigned."));
}

}  // namespace yp::ui
