#pragma once

#include "ui/ShortcutMap.h"

#include <QDialog>

class QKeySequenceEdit;
class QLabel;
class QTableWidget;

namespace yp::ui {

// Edits a copy of the shortcut map; the caller takes map() when the dialog is accepted.
class ShortcutDialog : public QDialog {
    Q_OBJECT
public:
    ShortcutDialog(ShortcutMap map, QWidget* parent = nullptr);

    [[nodiscard]] const ShortcutMap& map() const { return map_; }

private:
    void refreshRow(int row);
    void loadSelection();
    void assign();
    [[nodiscard]] QString selectedId() const;
    [[nodiscard]] QString textFor(const QString& id) const;

    ShortcutMap map_;
    QTableWidget* table_ = nullptr;
    QKeySequenceEdit* editor_ = nullptr;
    QLabel* message_ = nullptr;
};

}  // namespace yp::ui
