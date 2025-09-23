#include "bloodtypetool.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <cmath>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(BloodTypeTool);

// BloodTypeCalculator 实现
QList<HeritageResult> BloodTypeCalculator::calculateOffspring(const CompleteBloodType& parent1, const CompleteBloodType& parent2) {
    QList<HeritageResult> results;

    // 获取ABO杂交结果
    QList<ABOGenotype> aboResults = crossABO(parent1.aboGenotype, parent2.aboGenotype);

    // 获取Rh杂交结果
    QList<RhGenotype> rhResults = crossRh(parent1.rhGenotype, parent2.rhGenotype);

    // 组合所有可能的结果
    QMap<QString, HeritageResult> resultMap;

    for (const ABOGenotype& abo : aboResults) {
        for (const RhGenotype& rh : rhResults) {
            CompleteBloodType offspring(abo, rh);
            QString key = offspring.getPhenotypeString();

            if (resultMap.contains(key)) {
                resultMap[key].count++;
            } else {
                HeritageResult result(offspring, 0.0, 1);
                result.description = QString("基因型: %1").arg(offspring.getGenotypeString());
                resultMap[key] = result;
            }
        }
    }

    // 计算概率
    int totalCombinations = aboResults.size() * rhResults.size();
    for (auto it = resultMap.begin(); it != resultMap.end(); ++it) {
        it.value().probability = (double)it.value().count / totalCombinations;
        results.append(it.value());
    }

    // 按概率降序排序
    std::sort(results.begin(), results.end(), [](const HeritageResult& a, const HeritageResult& b) {
        return a.probability > b.probability;
    });

    return results;
}

QList<ABOGenotype> BloodTypeCalculator::crossABO(const ABOGenotype& parent1, const ABOGenotype& parent2) {
    QList<ABOGenotype> results;

    // 父本1的配子
    QList<char> gametes1 = {parent1.allele1, parent1.allele2};
    // 父本2的配子
    QList<char> gametes2 = {parent2.allele1, parent2.allele2};

    // 组合所有可能的配子
    for (char g1 : gametes1) {
        for (char g2 : gametes2) {
            results.append(ABOGenotype(g1, g2));
        }
    }

    return results;
}

QList<RhGenotype> BloodTypeCalculator::crossRh(const RhGenotype& parent1, const RhGenotype& parent2) {
    QList<RhGenotype> results;

    // 父本1的配子
    QList<bool> gametes1 = {parent1.allele1, parent1.allele2};
    // 父本2的配子
    QList<bool> gametes2 = {parent2.allele1, parent2.allele2};

    // 组合所有可能的配子
    for (bool g1 : gametes1) {
        for (bool g2 : gametes2) {
            results.append(RhGenotype(g1, g2));
        }
    }

    return results;
}

QList<ABOGenotype> BloodTypeCalculator::getAllPossibleABOGenotypes(ABOBloodType bloodType) {
    QList<ABOGenotype> genotypes;

    switch (bloodType) {
        case ABOBloodType::A:
            genotypes.append(ABOGenotype('A', 'A'));
            genotypes.append(ABOGenotype('A', 'O'));
            break;
        case ABOBloodType::B:
            genotypes.append(ABOGenotype('B', 'B'));
            genotypes.append(ABOGenotype('B', 'O'));
            break;
        case ABOBloodType::AB:
            genotypes.append(ABOGenotype('A', 'B'));
            break;
        case ABOBloodType::O:
            genotypes.append(ABOGenotype('O', 'O'));
            break;
    }

    return genotypes;
}

QList<RhGenotype> BloodTypeCalculator::getAllPossibleRhGenotypes(RhBloodType bloodType) {
    QList<RhGenotype> genotypes;

    switch (bloodType) {
        case RhBloodType::Positive:
            genotypes.append(RhGenotype(true, true));   // DD
            genotypes.append(RhGenotype(true, false));  // Dd
            break;
        case RhBloodType::Negative:
            genotypes.append(RhGenotype(false, false)); // dd
            break;
    }

    return genotypes;
}

QString BloodTypeCalculator::getBloodTypeDescription(const CompleteBloodType& bloodType) {
    QString description;

    // ABO系统描述
    switch (bloodType.aboGenotype.getBloodType()) {
        case ABOBloodType::A:
            description += "A型血：红细胞表面有A抗原，血清中有抗B抗体。";
            break;
        case ABOBloodType::B:
            description += "B型血：红细胞表面有B抗原，血清中有抗A抗体。";
            break;
        case ABOBloodType::AB:
            description += "AB型血：红细胞表面有A和B抗原，血清中无抗A、抗B抗体，被称为万能受血者。";
            break;
        case ABOBloodType::O:
            description += "O型血：红细胞表面无A、B抗原，血清中有抗A、抗B抗体，被称为万能献血者。";
            break;
    }

    description += "\n\n";

    // Rh系统描述
    if (bloodType.rhGenotype.getBloodType() == RhBloodType::Positive) {
        description += "Rh阳性：红细胞表面有Rh抗原（D抗原）。";
    } else {
        description += "Rh阴性：红细胞表面无Rh抗原（D抗原），较为稀少。";
    }

    return description;
}

QStringList BloodTypeCalculator::getCompatibleDonors(const CompleteBloodType& recipient) {
    QStringList donors;

    ABOBloodType recipientABO = recipient.aboGenotype.getBloodType();
    RhBloodType recipientRh = recipient.rhGenotype.getBloodType();

    // ABO系统兼容性
    QList<ABOBloodType> compatibleABO;
    switch (recipientABO) {
        case ABOBloodType::A:
            compatibleABO = {ABOBloodType::A, ABOBloodType::O};
            break;
        case ABOBloodType::B:
            compatibleABO = {ABOBloodType::B, ABOBloodType::O};
            break;
        case ABOBloodType::AB:
            compatibleABO = {ABOBloodType::A, ABOBloodType::B, ABOBloodType::AB, ABOBloodType::O};
            break;
        case ABOBloodType::O:
            compatibleABO = {ABOBloodType::O};
            break;
    }

    // Rh系统兼容性
    QList<RhBloodType> compatibleRh;
    if (recipientRh == RhBloodType::Positive) {
        compatibleRh = {RhBloodType::Positive, RhBloodType::Negative};
    } else {
        compatibleRh = {RhBloodType::Negative};
    }

    // 组合兼容的血型
    for (ABOBloodType abo : compatibleABO) {
        for (RhBloodType rh : compatibleRh) {
            QString aboStr;
            switch (abo) {
                case ABOBloodType::A: aboStr = "A"; break;
                case ABOBloodType::B: aboStr = "B"; break;
                case ABOBloodType::AB: aboStr = "AB"; break;
                case ABOBloodType::O: aboStr = "O"; break;
            }

            QString rhStr = (rh == RhBloodType::Positive) ? "+" : "-";
            donors.append(aboStr + rhStr);
        }
    }

    return donors;
}

QStringList BloodTypeCalculator::getCompatibleRecipients(const CompleteBloodType& donor) {
    QStringList recipients;

    ABOBloodType donorABO = donor.aboGenotype.getBloodType();
    RhBloodType donorRh = donor.rhGenotype.getBloodType();

    // ABO系统兼容性
    QList<ABOBloodType> compatibleABO;
    switch (donorABO) {
        case ABOBloodType::A:
            compatibleABO = {ABOBloodType::A, ABOBloodType::AB};
            break;
        case ABOBloodType::B:
            compatibleABO = {ABOBloodType::B, ABOBloodType::AB};
            break;
        case ABOBloodType::AB:
            compatibleABO = {ABOBloodType::AB};
            break;
        case ABOBloodType::O:
            compatibleABO = {ABOBloodType::A, ABOBloodType::B, ABOBloodType::AB, ABOBloodType::O};
            break;
    }

    // Rh系统兼容性
    QList<RhBloodType> compatibleRh;
    if (donorRh == RhBloodType::Positive) {
        compatibleRh = {RhBloodType::Positive};
    } else {
        compatibleRh = {RhBloodType::Positive, RhBloodType::Negative};
    }

    // 组合兼容的血型
    for (ABOBloodType abo : compatibleABO) {
        for (RhBloodType rh : compatibleRh) {
            QString aboStr;
            switch (abo) {
                case ABOBloodType::A: aboStr = "A"; break;
                case ABOBloodType::B: aboStr = "B"; break;
                case ABOBloodType::AB: aboStr = "AB"; break;
                case ABOBloodType::O: aboStr = "O"; break;
            }

            QString rhStr = (rh == RhBloodType::Positive) ? "+" : "-";
            recipients.append(aboStr + rhStr);
        }
    }

    return recipients;
}

// BloodTypeCalculator_Widget 实现
BloodTypeCalculator_Widget::BloodTypeCalculator_Widget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    updateUI();
}

void BloodTypeCalculator_Widget::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧输入区域
    m_inputWidget = new QWidget();
    m_inputWidget->setFixedWidth(350);
    m_inputLayout = new QVBoxLayout(m_inputWidget);

    QString groupStyle =
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "}";

    // 父本1血型设置
    m_parent1Group = new QGroupBox("👨 父亲血型");
    m_parent1Group->setStyleSheet(groupStyle);
    m_parent1Layout = new QFormLayout(m_parent1Group);

    m_parent1ABOCombo = new QComboBox();
    m_parent1ABOCombo->addItems({"A", "B", "AB", "O"});

    m_parent1RhCombo = new QComboBox();
    m_parent1RhCombo->addItems({"阳性(+)", "阴性(-)"});
    m_parent1RhCombo->setStyleSheet(m_parent1ABOCombo->styleSheet());

    m_parent1Layout->addRow("ABO血型:", m_parent1ABOCombo);
    m_parent1Layout->addRow("Rh血型:", m_parent1RhCombo);

    // 父本2血型设置
    m_parent2Group = new QGroupBox("👩 母亲血型");
    m_parent2Group->setStyleSheet(groupStyle);
    m_parent2Layout = new QFormLayout(m_parent2Group);

    m_parent2ABOCombo = new QComboBox();
    m_parent2ABOCombo->addItems({"A", "B", "AB", "O"});
    m_parent2ABOCombo->setStyleSheet(m_parent1ABOCombo->styleSheet());

    m_parent2RhCombo = new QComboBox();
    m_parent2RhCombo->addItems({"阳性(+)", "阴性(-)"});
    m_parent2RhCombo->setStyleSheet(m_parent1ABOCombo->styleSheet());

    m_parent2Layout->addRow("ABO血型:", m_parent2ABOCombo);
    m_parent2Layout->addRow("Rh血型:", m_parent2RhCombo);

    // 控制按钮组
    m_controlGroup = new QGroupBox("操作控制");
    m_controlGroup->setStyleSheet(groupStyle);
    m_controlLayout = new QVBoxLayout(m_controlGroup);

    m_calculateBtn = new QPushButton("🧬 计算遗传概率");
    m_calculateBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    m_clearBtn = new QPushButton("🗑️ 清空结果");
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #95a5a6;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #7f8c8d;"
        "}"
    );

    m_copyResultBtn = new QPushButton("📋 复制结果");
    m_copyResultBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_saveResultBtn = new QPushButton("💾 保存结果");
    m_saveResultBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_copyResultBtn->setEnabled(false);
    m_saveResultBtn->setEnabled(false);

    m_controlLayout->addWidget(m_calculateBtn);
    m_controlLayout->addWidget(m_clearBtn);
    m_controlLayout->addWidget(m_copyResultBtn);
    m_controlLayout->addWidget(m_saveResultBtn);

    // 组装左侧布局
    m_inputLayout->addWidget(m_parent1Group);
    m_inputLayout->addWidget(m_parent2Group);
    m_inputLayout->addWidget(m_controlGroup);
    m_inputLayout->addStretch();

    // 右侧结果区域
    m_resultWidget = new QWidget();
    m_resultLayout = new QVBoxLayout(m_resultWidget);

    m_resultGroup = new QGroupBox("👶 后代血型遗传概率");
    m_resultGroup->setStyleSheet(groupStyle);
    m_resultGroupLayout = new QVBoxLayout(m_resultGroup);

    // 结果表格
    m_resultTable = new QTableWidget();
    m_resultTable->setColumnCount(4);
    m_resultTable->setHorizontalHeaderLabels({"血型", "基因型", "概率", "比例"});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setStyleSheet(
        "QTableWidget {"
        "border: 1px solid #bdc3c7;"
        "gridline-color: #ecf0f1;"
        "background-color: #ffffff;"
        "}"
        "QTableWidget::item {"
        "padding: 8px;"
        "border-bottom: 1px solid #ecf0f1;"
        "}"
        "QHeaderView::section {"
        "background-color: #34495e;"
        "color: white;"
        "padding: 8px;"
        "border: none;"
        "font-weight: bold;"
        "}"
    );

    m_resultGroupLayout->addWidget(m_resultTable);
    m_resultLayout->addWidget(m_resultGroup);

    // 组装主布局
    m_mainSplitter->addWidget(m_inputWidget);
    m_mainSplitter->addWidget(m_resultWidget);
    m_mainSplitter->setSizes({350, 600});

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_calculateBtn, &QPushButton::clicked, this, &BloodTypeCalculator_Widget::onCalculateClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &BloodTypeCalculator_Widget::onClearClicked);
    connect(m_copyResultBtn, &QPushButton::clicked, this, &BloodTypeCalculator_Widget::onCopyResultClicked);
    connect(m_saveResultBtn, &QPushButton::clicked, this, &BloodTypeCalculator_Widget::onSaveResultClicked);

    connect(m_parent1ABOCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BloodTypeCalculator_Widget::onParentBloodTypeChanged);
    connect(m_parent1RhCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BloodTypeCalculator_Widget::onParentBloodTypeChanged);
    connect(m_parent2ABOCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BloodTypeCalculator_Widget::onParentBloodTypeChanged);
    connect(m_parent2RhCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BloodTypeCalculator_Widget::onParentBloodTypeChanged);
}

void BloodTypeCalculator_Widget::onCalculateClicked() {
    // 获取父本血型
    ABOBloodType parent1ABO = stringToBloodType(m_parent1ABOCombo->currentText());
    RhBloodType parent1Rh = stringToRhType(m_parent1RhCombo->currentText());

    ABOBloodType parent2ABO = stringToBloodType(m_parent2ABOCombo->currentText());
    RhBloodType parent2Rh = stringToRhType(m_parent2RhCombo->currentText());

    // 为了简化计算，这里假设每种血型只有一种可能的基因型
    // 实际应用中可能需要考虑多种基因型的情况
    ABOGenotype parent1ABOGeno, parent2ABOGeno;
    RhGenotype parent1RhGeno, parent2RhGeno;

    // 根据表型推测最常见的基因型
    switch (parent1ABO) {
        case ABOBloodType::A:
            parent1ABOGeno = ABOGenotype('A', 'O'); // 假设为AO
            break;
        case ABOBloodType::B:
            parent1ABOGeno = ABOGenotype('B', 'O'); // 假设为BO
            break;
        case ABOBloodType::AB:
            parent1ABOGeno = ABOGenotype('A', 'B');
            break;
        case ABOBloodType::O:
            parent1ABOGeno = ABOGenotype('O', 'O');
            break;
    }

    switch (parent2ABO) {
        case ABOBloodType::A:
            parent2ABOGeno = ABOGenotype('A', 'O'); // 假设为AO
            break;
        case ABOBloodType::B:
            parent2ABOGeno = ABOGenotype('B', 'O'); // 假设为BO
            break;
        case ABOBloodType::AB:
            parent2ABOGeno = ABOGenotype('A', 'B');
            break;
        case ABOBloodType::O:
            parent2ABOGeno = ABOGenotype('O', 'O');
            break;
    }

    // Rh基因型
    if (parent1Rh == RhBloodType::Positive) {
        parent1RhGeno = RhGenotype(true, false); // 假设为Dd
    } else {
        parent1RhGeno = RhGenotype(false, false); // dd
    }

    if (parent2Rh == RhBloodType::Positive) {
        parent2RhGeno = RhGenotype(true, false); // 假设为Dd
    } else {
        parent2RhGeno = RhGenotype(false, false); // dd
    }

    CompleteBloodType parent1(parent1ABOGeno, parent1RhGeno);
    CompleteBloodType parent2(parent2ABOGeno, parent2RhGeno);

    // 计算遗传结果
    m_lastResults = BloodTypeCalculator::calculateOffspring(parent1, parent2);

    // 显示结果
    displayResults(m_lastResults);

    m_copyResultBtn->setEnabled(true);
    m_saveResultBtn->setEnabled(true);
}

void BloodTypeCalculator_Widget::onClearClicked() {
    m_resultTable->setRowCount(0);
    m_lastResults.clear();
    m_copyResultBtn->setEnabled(false);
    m_saveResultBtn->setEnabled(false);
}

void BloodTypeCalculator_Widget::onCopyResultClicked() {
    if (m_lastResults.isEmpty()) {
        return;
    }

    QString resultText = "血型遗传概率计算结果\n";
    resultText += "========================\n\n";
    resultText += QString("父亲血型: %1%2\n").arg(m_parent1ABOCombo->currentText(),
                         m_parent1RhCombo->currentText().contains("+") ? "+" : "-");
    resultText += QString("母亲血型: %1%2\n\n").arg(m_parent2ABOCombo->currentText(),
                         m_parent2RhCombo->currentText().contains("+") ? "+" : "-");

    resultText += "后代血型概率:\n";
    for (const HeritageResult& result : m_lastResults) {
        resultText += QString("• %1: %2% (比例 %3/%4)\n")
                     .arg(result.bloodType.getPhenotypeString())
                     .arg(result.probability * 100, 0, 'f', 1)
                     .arg(result.count)
                     .arg(m_lastResults.size() > 0 ? 16 : 1); // 假设4x4=16种组合
    }

    QApplication::clipboard()->setText(resultText);
    QMessageBox::information(this, "成功", "计算结果已复制到剪贴板");
}

void BloodTypeCalculator_Widget::onSaveResultClicked() {
    if (m_lastResults.isEmpty()) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存血型遗传计算结果",
        QString("blood_type_result_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "文本文件 (*.txt);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);

            stream << "血型遗传概率计算结果\n";
            stream << "========================\n\n";
            stream << "计算时间: " << QDateTime::currentDateTime().toString() << "\n\n";

            stream << QString("父亲血型: %1%2\n").arg(m_parent1ABOCombo->currentText(),
                             m_parent1RhCombo->currentText().contains("+") ? "+" : "-");
            stream << QString("母亲血型: %1%2\n\n").arg(m_parent2ABOCombo->currentText(),
                             m_parent2RhCombo->currentText().contains("+") ? "+" : "-");

            stream << "后代血型概率详情:\n";
            stream << "血型\t基因型\t\t概率\t比例\n";
            stream << "------------------------------------\n";

            for (const HeritageResult& result : m_lastResults) {
                stream << QString("%1\t%2\t%3%\t%4/16\n")
                       .arg(result.bloodType.getPhenotypeString())
                       .arg(result.bloodType.getGenotypeString())
                       .arg(result.probability * 100, 0, 'f', 1)
                       .arg(result.count);
            }

            QMessageBox::information(this, "成功", "计算结果已保存");
        } else {
            QMessageBox::warning(this, "错误", "无法保存文件");
        }
    }
}

void BloodTypeCalculator_Widget::onParentBloodTypeChanged() {
    // 当父母血型改变时可以实时显示一些提示信息
    updateUI();
}

void BloodTypeCalculator_Widget::displayResults(const QList<HeritageResult>& results) {
    m_resultTable->setRowCount(results.size());

    for (int i = 0; i < results.size(); ++i) {
        const HeritageResult& result = results[i];

        // 血型
        auto* typeItem = new QTableWidgetItem(result.bloodType.getPhenotypeString());
        typeItem->setTextAlignment(Qt::AlignCenter);
        typeItem->setFont(QFont("Arial", 11, QFont::Bold));

        // 基因型
        auto* genotypeItem = new QTableWidgetItem(result.bloodType.getGenotypeString());
        genotypeItem->setTextAlignment(Qt::AlignCenter);

        // 概率
        auto* probItem = new QTableWidgetItem(QString("%1%").arg(result.probability * 100, 0, 'f', 1));
        probItem->setTextAlignment(Qt::AlignCenter);

        // 比例
        auto* ratioItem = new QTableWidgetItem(QString("%1/16").arg(result.count));
        ratioItem->setTextAlignment(Qt::AlignCenter);

        // 根据概率设置颜色
        QColor bgColor;
        if (result.probability >= 0.25) {
            bgColor = QColor(220, 255, 220); // 浅绿色 - 高概率
        } else if (result.probability >= 0.125) {
            bgColor = QColor(255, 255, 220); // 浅黄色 - 中等概率
        } else {
            bgColor = QColor(255, 220, 220); // 浅红色 - 低概率
        }

        typeItem->setBackground(bgColor);
        genotypeItem->setBackground(bgColor);
        probItem->setBackground(bgColor);
        ratioItem->setBackground(bgColor);

        m_resultTable->setItem(i, 0, typeItem);
        m_resultTable->setItem(i, 1, genotypeItem);
        m_resultTable->setItem(i, 2, probItem);
        m_resultTable->setItem(i, 3, ratioItem);
    }

    m_resultTable->resizeColumnsToContents();
}

QString BloodTypeCalculator_Widget::bloodTypeToString(ABOBloodType type) {
    switch (type) {
        case ABOBloodType::A: return "A";
        case ABOBloodType::B: return "B";
        case ABOBloodType::AB: return "AB";
        case ABOBloodType::O: return "O";
    }
    return "O";
}

QString BloodTypeCalculator_Widget::rhTypeToString(RhBloodType type) {
    return (type == RhBloodType::Positive) ? "阳性(+)" : "阴性(-)";
}

ABOBloodType BloodTypeCalculator_Widget::stringToBloodType(const QString& str) {
    if (str == "A") return ABOBloodType::A;
    if (str == "B") return ABOBloodType::B;
    if (str == "AB") return ABOBloodType::AB;
    return ABOBloodType::O;
}

RhBloodType BloodTypeCalculator_Widget::stringToRhType(const QString& str) {
    return str.contains("+") ? RhBloodType::Positive : RhBloodType::Negative;
}

void BloodTypeCalculator_Widget::updateUI() {
    // 可以根据当前选择的血型显示一些提示信息
}

// BloodTypeKnowledge 实现
BloodTypeKnowledge::BloodTypeKnowledge(QWidget* parent)
    : QWidget(parent) {
    setupUI();
    loadKnowledgeContent();
}

void BloodTypeKnowledge::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 顶部控制区域
    m_topLayout = new QHBoxLayout();

    m_topicGroup = new QGroupBox("知识主题");
    m_topicGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
    );
    m_topicLayout = new QVBoxLayout(m_topicGroup);

    m_topicCombo = new QComboBox();
    m_topicCombo->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    m_buttonLayout = new QHBoxLayout();
    m_copyBtn = new QPushButton("📋 复制内容");
    m_saveBtn = new QPushButton("💾 保存内容");

    QString btnStyle =
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}";

    m_copyBtn->setStyleSheet(btnStyle);
    m_saveBtn->setStyleSheet(btnStyle.replace("#3498db", "#27ae60").replace("#2980b9", "#219a52"));

    m_buttonLayout->addWidget(m_copyBtn);
    m_buttonLayout->addWidget(m_saveBtn);
    m_buttonLayout->addStretch();

    m_topicLayout->addWidget(m_topicCombo);
    m_topicLayout->addLayout(m_buttonLayout);

    m_topLayout->addWidget(m_topicGroup);
    m_topLayout->addStretch();

    // 内容显示区域
    m_contentGroup = new QGroupBox("详细内容");
    m_contentGroup->setStyleSheet(m_topicGroup->styleSheet());
    m_contentLayout = new QVBoxLayout(m_contentGroup);

    m_contentText = new QTextEdit();
    m_contentText->setReadOnly(true);
    m_contentText->setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "border-radius: 4px;"
        "background-color: #f8f9fa;"
        "font-size: 12px;"
        "line-height: 1.6;"
        "}"
    );

    m_contentLayout->addWidget(m_contentText);

    // 组装布局
    m_mainLayout->addLayout(m_topLayout);
    m_mainLayout->addWidget(m_contentGroup);

    // 连接信号
    connect(m_topicCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BloodTypeKnowledge::onTopicChanged);
    connect(m_copyBtn, &QPushButton::clicked, this, &BloodTypeKnowledge::onCopyContentClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &BloodTypeKnowledge::onSaveContentClicked);
}

void BloodTypeKnowledge::loadKnowledgeContent() {
    // 加载血型知识内容
    m_knowledgeContent["ABO血型系统"] =
        "ABO血型系统是最重要的血型系统之一，由奥地利科学家兰德施泰纳于1900年发现。\n\n"
        "🔬 基本原理:\n"
        "• A型血：红细胞表面有A抗原，血清中有抗B抗体\n"
        "• B型血：红细胞表面有B抗原，血清中有抗A抗体\n"
        "• AB型血：红细胞表面有A和B抗原，血清中无抗A、抗B抗体\n"
        "• O型血：红细胞表面无A、B抗原，血清中有抗A、抗B抗体\n\n"
        "🧬 遗传机制:\n"
        "ABO血型由第9号染色体上的ABO基因决定，该基因有三个主要等位基因：\n"
        "• A等位基因：编码A糖基转移酶，产生A抗原\n"
        "• B等位基因：编码B糖基转移酶，产生B抗原\n"
        "• O等位基因：为隐性基因，不产生有功能的酶\n\n"
        "📊 人群分布:\n"
        "• 中国人群：O型(34%), A型(28%), B型(27%), AB型(11%)\n"
        "• 世界平均：O型(45%), A型(40%), B型(11%), AB型(4%)";

    m_knowledgeContent["Rh血型系统"] =
        "Rh血型系统是第二重要的血型系统，由兰德施泰纳和维纳于1940年发现。\n\n"
        "🔬 基本原理:\n"
        "• Rh阳性(Rh+)：红细胞表面有D抗原\n"
        "• Rh阴性(Rh-)：红细胞表面无D抗原\n\n"
        "🧬 遗传机制:\n"
        "Rh血型主要由D基因决定：\n"
        "• D等位基因：显性，产生D抗原\n"
        "• d等位基因：隐性，不产生D抗原\n"
        "• DD或Dd基因型：Rh阳性\n"
        "• dd基因型：Rh阴性\n\n"
        "⚠️ 临床意义:\n"
        "• 新生儿溶血病：Rh阴性母亲怀Rh阳性胎儿时可能发生\n"
        "• 输血配型：除ABO配型外，Rh配型也很重要\n\n"
        "📊 人群分布:\n"
        "• 中国人群：Rh阳性(99.7%), Rh阴性(0.3%)\n"
        "• 欧洲人群：Rh阳性(85%), Rh阴性(15%)";

    m_knowledgeContent["血型遗传规律"] =
        "血型遗传遵循孟德尔遗传定律，表现为共显性和隐性遗传模式。\n\n"
        "🧬 ABO遗传规律:\n"
        "• A和B等位基因对O等位基因呈显性\n"
        "• A和B等位基因之间呈共显性\n"
        "• 可能的基因型：AA, AO(A型), BB, BO(B型), AB(AB型), OO(O型)\n\n"
        "📋 遗传组合表:\n"
        "父母血型 → 可能的子女血型\n"
        "• A × A → A, O\n"
        "• A × B → A, B, AB, O\n"
        "• A × AB → A, B, AB\n"
        "• A × O → A, O\n"
        "• B × B → B, O\n"
        "• B × AB → A, B, AB\n"
        "• B × O → B, O\n"
        "• AB × AB → A, B, AB\n"
        "• AB × O → A, B\n"
        "• O × O → O\n\n"
        "🔍 Rh遗传规律:\n"
        "• D基因对d基因呈完全显性\n"
        "• DD或Dd → Rh阳性\n"
        "• dd → Rh阴性\n"
        "• Rh阴性父母的子女必为Rh阴性\n"
        "• Rh阳性父母可能有Rh阴性子女";

    m_knowledgeContent["血型与输血"] =
        "血型配型是安全输血的基础，不相容的血型会导致严重的输血反应。\n\n"
        "🩸 ABO配型原则:\n"
        "• 同型输血：最安全的选择\n"
        "• O型红细胞：可输给任何ABO血型(万能供血者)\n"
        "• AB型患者：可接受任何ABO血型的红细胞(万能受血者)\n"
        "• A型：可接受A型和O型\n"
        "• B型：可接受B型和O型\n\n"
        "⚡ Rh配型原则:\n"
        "• Rh阳性：可接受Rh阳性和Rh阴性\n"
        "• Rh阴性：只能接受Rh阴性\n"
        "• Rh阴性患者输入Rh阳性血液会产生抗D抗体\n\n"
        "📋 完整配型表:\n"
        "受血者 → 可接受的供血者\n"
        "• A+ → A+, A-, O+, O-\n"
        "• A- → A-, O-\n"
        "• B+ → B+, B-, O+, O-\n"
        "• B- → B-, O-\n"
        "• AB+ → 所有血型\n"
        "• AB- → A-, B-, AB-, O-\n"
        "• O+ → O+, O-\n"
        "• O- → O-\n\n"
        "⚠️ 注意事项:\n"
        "• 首次输血相对安全，多次输血需要更严格的配型\n"
        "• 除ABO和Rh外，还有其他血型系统需要考虑";

    m_knowledgeContent["血型与疾病"] =
        "研究表明，不同血型与某些疾病的易感性存在关联。\n\n"
        "🔬 A型血相关:\n"
        "• 胃癌风险相对较高\n"
        "• 心脑血管疾病风险增加\n"
        "• 对痘类病毒抵抗力较强\n\n"
        "🔬 B型血相关:\n"
        "• 糖尿病风险相对较高\n"
        "• 对结核病抵抗力较强\n"
        "• 泌尿系统感染风险较低\n\n"
        "🔬 AB型血相关:\n"
        "• 记忆力衰退风险较高\n"
        "• 心脏病风险相对较低\n"
        "• 某些癌症风险增加\n\n"
        "🔬 O型血相关:\n"
        "• 胃溃疡和十二指肠溃疡风险较高\n"
        "• 对疟疾抵抗力较强\n"
        "• 血液凝固时间较长\n\n"
        "⚠️ 重要提醒:\n"
        "• 血型与疾病的关联只是统计学相关性\n"
        "• 不能仅凭血型预测疾病\n"
        "• 生活方式和环境因素更为重要\n"
        "• 定期体检和健康生活是预防疾病的关键";

    m_knowledgeContent["血型检测方法"] =
        "血型检测是医学检验的基础项目，有多种检测方法。\n\n"
        "🧪 正向定型(细胞定型):\n"
        "• 用已知抗体检测未知红细胞上的抗原\n"
        "• 抗A血清 + 红细胞：凝集为A型\n"
        "• 抗B血清 + 红细胞：凝集为B型\n"
        "• 两者都凝集为AB型，都不凝集为O型\n\n"
        "🧪 反向定型(血清定型):\n"
        "• 用已知红细胞检测未知血清中的抗体\n"
        "• A型细胞 + 血清：凝集说明有抗A抗体\n"
        "• B型细胞 + 血清：凝集说明有抗B抗体\n\n"
        "🔬 Rh检测:\n"
        "• 抗D血清 + 红细胞\n"
        "• 凝集为Rh阳性，不凝集为Rh阴性\n"
        "• 弱D检测：间接抗球蛋白试验\n\n"
        "📋 质量控制:\n"
        "• 正反向定型结果必须一致\n"
        "• 使用阳性和阴性对照\n"
        "• 定期校准检测试剂\n"
        "• 严格按照操作规程进行\n\n"
        "🏥 检测意义:\n"
        "• 输血前配型检查\n"
        "• 新生儿溶血病筛查\n"
        "• 器官移植配型\n"
        "• 亲子鉴定辅助";

    // 填充下拉框
    m_topicCombo->clear();
    for (auto it = m_knowledgeContent.begin(); it != m_knowledgeContent.end(); ++it) {
        m_topicCombo->addItem(it.key());
    }

    // 显示第一个主题的内容
    if (!m_knowledgeContent.isEmpty()) {
        displayContent(m_topicCombo->currentText());
    }
}

void BloodTypeKnowledge::onTopicChanged() {
    displayContent(m_topicCombo->currentText());
}

void BloodTypeKnowledge::onCopyContentClicked() {
    QString content = m_contentText->toPlainText();
    QApplication::clipboard()->setText(content);
    QMessageBox::information(this, "成功", "内容已复制到剪贴板");
}

void BloodTypeKnowledge::onSaveContentClicked() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存血型知识内容",
        QString("blood_type_knowledge_%1.txt").arg(m_topicCombo->currentText()),
        "文本文件 (*.txt);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_contentText->toPlainText();
            QMessageBox::information(this, "成功", "内容已保存");
        } else {
            QMessageBox::warning(this, "错误", "无法保存文件");
        }
    }
}

void BloodTypeKnowledge::displayContent(const QString& topic) {
    if (m_knowledgeContent.contains(topic)) {
        m_contentText->setPlainText(m_knowledgeContent[topic]);
    }
}

// 其他组件的简化实现...
BloodTypeCompatibility::BloodTypeCompatibility(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void BloodTypeCompatibility::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    auto* label = new QLabel("血型配对功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    m_mainLayout->addWidget(label);
}

BloodTypeStatistics::BloodTypeStatistics(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void BloodTypeStatistics::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    auto* label = new QLabel("血型统计功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    m_mainLayout->addWidget(label);
}

BloodTypeSimulation::BloodTypeSimulation(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void BloodTypeSimulation::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    auto* label = new QLabel("血型遗传模拟功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    m_mainLayout->addWidget(label);
}

// BloodTypeTool 主类实现
BloodTypeTool::BloodTypeTool(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void BloodTypeTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建标签页控件
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 4px 8px;"
        "margin-right: 2px;"
        "font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "background-color: #d5dbdb;"
        "}"
    );

    // 创建各个标签页
    m_calculator = new BloodTypeCalculator_Widget();
    m_tabWidget->addTab(m_calculator, "🧬 遗传计算");

    m_knowledge = new BloodTypeKnowledge();
    m_tabWidget->addTab(m_knowledge, "📚 血型知识");

    m_compatibility = new BloodTypeCompatibility();
    m_tabWidget->addTab(m_compatibility, "🩸 配型分析");

    m_statistics = new BloodTypeStatistics();
    m_tabWidget->addTab(m_statistics, "📊 统计分析");

    m_simulation = new BloodTypeSimulation();
    m_tabWidget->addTab(m_simulation, "🎲 遗传模拟");

    m_mainLayout->addWidget(m_tabWidget);
}

// BloodTypeCompatibility 槽函数实现
void BloodTypeCompatibility::onCheckCompatibilityClicked() {
    // TODO: 实现配型检查功能
}

void BloodTypeCompatibility::onClearClicked() {
    // TODO: 实现清除功能
}

void BloodTypeCompatibility::onBloodTypeChanged() {
    // TODO: 实现血型变化处理
}

// BloodTypeStatistics 槽函数实现
void BloodTypeStatistics::onGenerateStatsClicked() {
    // TODO: 实现统计生成功能
}

void BloodTypeStatistics::onClearStatsClicked() {
    // TODO: 实现清除统计功能
}

void BloodTypeStatistics::onExportStatsClicked() {
    // TODO: 实现导出统计功能
}

void BloodTypeStatistics::onRegionChanged() {
    // TODO: 实现地区变化处理
}

// BloodTypeSimulation 槽函数实现
void BloodTypeSimulation::onRunSimulationClicked() {
    // TODO: 实现运行模拟功能
}

void BloodTypeSimulation::onClearSimulationClicked() {
    // TODO: 实现清除模拟功能
}

void BloodTypeSimulation::onSaveSimulationClicked() {
    // TODO: 实现保存模拟功能
}

void BloodTypeSimulation::onGenerationCountChanged() {
    // TODO: 实现代数变化处理
}

