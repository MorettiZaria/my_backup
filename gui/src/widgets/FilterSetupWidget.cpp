#include "widgets/FilterSetupWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QHeaderView>
#include <QFormLayout>
#include <cstdlib>
#include <ctime>

FilterSetupWidget::FilterSetupWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void FilterSetupWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    groupBox_ = new QGroupBox(tr("Filter"));
    groupBox_->setCheckable(true);
    groupBox_->setChecked(false);
    mainLayout->addWidget(groupBox_);

    auto* groupLayout = new QVBoxLayout(groupBox_);

    // Table
    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({
        tr("Dimension"), tr("Value"), tr("Exclude"), tr("")
    });
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    groupLayout->addWidget(table_);

    // Add button
    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    addBtn_ = new QPushButton(tr("+ Add Rule"));
    btnLayout->addWidget(addBtn_);
    groupLayout->addLayout(btnLayout);

    connect(addBtn_, &QPushButton::clicked, this, &FilterSetupWidget::onAddRow);
}

void FilterSetupWidget::onAddRow() {
    int row = table_->rowCount();
    table_->insertRow(row);

    // Dimension combo
    auto* dimCombo = new QComboBox;
    dimCombo->addItem(tr("Path"),    "path");
    dimCombo->addItem(tr("Name"),    "name");
    dimCombo->addItem(tr("Type"),    "type");
    dimCombo->addItem(tr("Mtime"),   "mtime");
    dimCombo->addItem(tr("Atime"),   "atime");
    dimCombo->addItem(tr("Ctime"),   "ctime");
    dimCombo->addItem(tr("Size"),    "size");
    dimCombo->addItem(tr("Owner"),   "owner");
    dimCombo->addItem(tr("Group"),   "group");
    table_->setCellWidget(row, 0, dimCombo);

    // Value with dimension-aware placeholder
    auto* valueEdit = new QLineEdit;
    table_->setCellWidget(row, 1, valueEdit);

    // Update placeholder when dimension changes
    auto updatePlaceholder = [dimCombo, valueEdit]() {
        QString dim = dimCombo->currentData().toString();
        if (dim == "path" || dim == "name") {
            valueEdit->setPlaceholderText(tr("*.cpp  or  report-*.pdf"));
        } else if (dim == "type") {
            valueEdit->setPlaceholderText(tr("f(普通文件) d(目录) l(符号链接) p(管道) b/c(设备) s(socket)"));
        } else if (dim == "mtime" || dim == "atime" || dim == "ctime") {
            valueEdit->setPlaceholderText(tr("after:2024-06-01  or  before:2024-01-01  or  between:2024-01-01,2024-12-31"));
        } else if (dim == "size") {
            valueEdit->setPlaceholderText(tr("+1M  -500K  =1024  or  100:1000"));
        } else if (dim == "owner" || dim == "group") {
            valueEdit->setPlaceholderText(tr("1000 (UID/GID)  or  username"));
        }
    };
    connect(dimCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, updatePlaceholder);
    updatePlaceholder();  // initial placeholder

    // Exclude checkbox
    auto* exclCheck = new QCheckBox;
    auto* exclWidget = new QWidget;
    auto* exclLayout = new QHBoxLayout(exclWidget);
    exclLayout->addWidget(exclCheck);
    exclLayout->setAlignment(Qt::AlignCenter);
    exclLayout->setContentsMargins(0, 0, 0, 0);
    table_->setCellWidget(row, 2, exclWidget);

    // Remove button
    auto* removeBtn = new QPushButton(tr("X"));
    removeBtn->setFixedWidth(30);
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        QPushButton* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        for (int r = 0; r < table_->rowCount(); ++r) {
            if (table_->cellWidget(r, 3) == btn) {
                onRemoveRow(r);
                return;
            }
        }
    });
    table_->setCellWidget(row, 3, removeBtn);
}

void FilterSetupWidget::onRemoveRow(int row) {
    table_->removeRow(row);
}

bool FilterSetupWidget::isEnabled() const {
    return groupBox_->isChecked();
}

void FilterSetupWidget::setFormEnabled(bool enabled) {
    groupBox_->setEnabled(enabled);
    table_->setEnabled(enabled);
    addBtn_->setEnabled(enabled);
}

CompositeFilter FilterSetupWidget::buildFilter() const {
    CompositeFilter filter;
    if (!groupBox_->isChecked()) return filter;

    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* dimCombo = qobject_cast<QComboBox*>(table_->cellWidget(r, 0));
        auto* valueEdit = qobject_cast<QLineEdit*>(table_->cellWidget(r, 1));
        auto* exclWidget = table_->cellWidget(r, 2);
        QCheckBox* exclCheck = exclWidget ? exclWidget->findChild<QCheckBox*>() : nullptr;

        if (!dimCombo || !valueEdit) continue;
        std::string dimStr = dimCombo->currentData().toString().toStdString();
        std::string value   = valueEdit->text().trimmed().toStdString();
        if (value.empty()) continue;

        bool exclude = exclCheck ? exclCheck->isChecked() : false;

        if (dimStr == "path") {
            if (exclude) filter.addExcludePath(value);
            else         filter.addIncludePath(value);
        } else if (dimStr == "name") {
            if (exclude) filter.addExcludeName(value);
            else         filter.addIncludeName(value);
        } else if (dimStr == "type") {
            if (value.size() == 1) {
                if (exclude) filter.addExcludeType(value[0]);
                else         filter.addIncludeType(value[0]);
            }
        } else if (dimStr == "mtime" || dimStr == "atime" || dimStr == "ctime") {
            // Parse time spec: "after:YYYY-MM-DD" / "before:..." / "between:D1,D2"
            std::string spec = value;
            size_t colon = spec.find(':');
            if (colon == std::string::npos) continue;
            std::string opStr  = spec.substr(0, colon);
            std::string valStr = spec.substr(colon + 1);

            TimeOp op;
            time_t t1 = 0, t2 = 0;

            if (opStr == "after") {
                op = TimeOp::AFTER;
                struct tm tm{};
                if (valStr.size() >= 10) {
                    tm.tm_year = std::stoi(valStr.substr(0,4)) - 1900;
                    tm.tm_mon  = std::stoi(valStr.substr(5,2)) - 1;
                    tm.tm_mday = std::stoi(valStr.substr(8,2));
                }
                t1 = mktime(&tm);
            } else if (opStr == "before") {
                op = TimeOp::BEFORE;
                struct tm tm{};
                if (valStr.size() >= 10) {
                    tm.tm_year = std::stoi(valStr.substr(0,4)) - 1900;
                    tm.tm_mon  = std::stoi(valStr.substr(5,2)) - 1;
                    tm.tm_mday = std::stoi(valStr.substr(8,2));
                }
                t1 = mktime(&tm);
            } else if (opStr == "between") {
                op = TimeOp::BETWEEN;
                size_t comma = valStr.find(',');
                if (comma == std::string::npos) continue;
                struct tm tm1{}, tm2{};
                tm1.tm_year = std::stoi(valStr.substr(0,4)) - 1900;
                tm1.tm_mon  = std::stoi(valStr.substr(5,2)) - 1;
                tm1.tm_mday = std::stoi(valStr.substr(8,2));
                tm2.tm_year = std::stoi(valStr.substr(comma+1, 4)) - 1900;
                tm2.tm_mon  = std::stoi(valStr.substr(comma+6, 2)) - 1;
                tm2.tm_mday = std::stoi(valStr.substr(comma+9, 2));
                t1 = mktime(&tm1);
                t2 = mktime(&tm2);
            } else continue;

            FilterDimension fd = FilterDimension::MTIME;
            if (dimStr == "atime") fd = FilterDimension::ATIME;
            else if (dimStr == "ctime") fd = FilterDimension::CTIME;

            filter.addRule(std::make_unique<FilterRule>(
                fd, exclude ? FilterAction::EXCLUDE : FilterAction::INCLUDE, op, t1, t2));

        } else if (dimStr == "size") {
            // Parse: "+1M" / "-500K" / "=1024" / "100:1000"
            std::string s = value;
            off_t mult = 1;
            if (!s.empty()) {
                char last = s.back();
                if (last == 'G' || last == 'g') { mult = 1024LL*1024*1024; s.pop_back(); }
                else if (last == 'M' || last == 'm') { mult = 1024LL*1024; s.pop_back(); }
                else if (last == 'K' || last == 'k') { mult = 1024LL; s.pop_back(); }
                else if (last == 'B' || last == 'b') { mult = 1; s.pop_back(); }
            }
            SizeOp op;
            off_t v1 = 0, v2 = 0;
            size_t colon = s.find(':');
            if (colon != std::string::npos) {
                v1 = std::strtoll(s.substr(0, colon).c_str(), nullptr, 10);
                v2 = std::strtoll(s.substr(colon+1).c_str(), nullptr, 10);
                op = SizeOp::RANGE;
            } else if (!s.empty() && (s[0] == '+' || s[0] == '>')) {
                v1 = std::strtoll(s.substr(1).c_str(), nullptr, 10) * mult;
                op = SizeOp::GT;
            } else if (!s.empty() && (s[0] == '-' || s[0] == '<')) {
                v1 = std::strtoll(s.substr(1).c_str(), nullptr, 10) * mult;
                op = SizeOp::LT;
            } else {
                v1 = std::strtoll(s.c_str(), nullptr, 10) * mult;
                op = SizeOp::EQ;
            }
            filter.addRule(std::make_unique<FilterRule>(
                exclude ? FilterAction::EXCLUDE : FilterAction::INCLUDE, op, v1, v2));

        } else if (dimStr == "owner" || dimStr == "group") {
            FilterDimension fd = (dimStr == "owner") ? FilterDimension::OWNER
                                                      : FilterDimension::GROUP;
            char* end = nullptr;
            long id = std::strtol(value.c_str(), &end, 10);
            if (end && *end == '\0' && id >= 0) {
                filter.addRule(std::make_unique<FilterRule>(
                    fd, exclude ? FilterAction::EXCLUDE : FilterAction::INCLUDE,
                    static_cast<uid_t>(id)));
            } else {
                filter.addRule(std::make_unique<FilterRule>(
                    fd, exclude ? FilterAction::EXCLUDE : FilterAction::INCLUDE,
                    value, ByName{}));
            }
        }
    }

    return filter;
}
