#ifndef GUI_FILTERSETUPWIDGET_H
#define GUI_FILTERSETUPWIDGET_H

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include "filter/CompositeFilter.h"

/// Collapsible file filter rule editor for backup tabs.
class FilterSetupWidget : public QWidget {
    Q_OBJECT
public:
    explicit FilterSetupWidget(QWidget* parent = nullptr);

    /// Build a CompositeFilter from the current rows.
    /// Caller owns the returned filter; only non-empty rows are added.
    CompositeFilter buildFilter() const;

    /// Whether filtering is enabled (checkbox checked).
    bool isEnabled() const;

    /// Enable/disable the entire widget.
    void setFormEnabled(bool enabled);

private slots:
    void onAddRow();
    void onRemoveRow(int row);

private:
    QGroupBox*    groupBox_;
    QCheckBox*    enableCheck_;
    QTableWidget* table_;
    QPushButton*  addBtn_;

    void setupUi();
};

#endif // GUI_FILTERSETUPWIDGET_H
