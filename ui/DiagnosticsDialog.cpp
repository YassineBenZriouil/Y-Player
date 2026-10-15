#include "ui/DiagnosticsDialog.h"

#include "core/PlaybackEngine.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace yp::ui {

DiagnosticsDialog::DiagnosticsDialog(const IPlaybackEngine& engine, QWidget* parent)
    : QDialog(parent), engine_(engine) {
    setWindowTitle(tr("Diagnostics"));
    resize(560, 720);
    auto* layout = new QVBoxLayout(this);
    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAccessibleName(tr("Diagnostics"));
    layout->addWidget(table_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* copy = buttons->addButton(tr("Copy to Clipboard"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(copy, &QPushButton::clicked, this, [this] {
        QStringList lines;
        for (int r = 0; r < table_->rowCount(); ++r) {
            lines << table_->item(r, 0)->text() + QStringLiteral(": ") + table_->item(r, 1)->text();
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });

    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, &DiagnosticsDialog::refresh);
}

void DiagnosticsDialog::showEvent(QShowEvent* event) {
    refresh();
    timer_.start();
    QDialog::showEvent(event);
}

void DiagnosticsDialog::hideEvent(QHideEvent* event) {
    timer_.stop();
    QDialog::hideEvent(event);
}

void DiagnosticsDialog::refresh() {
    const QList<std::pair<QString, QString>> rows = engine_.diagnostics();
    table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < 2; ++c) {
            const QString& text = c == 0 ? rows.at(r).first : rows.at(r).second;
            if (QTableWidgetItem* item = table_->item(r, c)) {
                if (item->text() != text) item->setText(text);
            } else {
                table_->setItem(r, c, new QTableWidgetItem(text));
            }
        }
    }
}

}  // namespace yp::ui
